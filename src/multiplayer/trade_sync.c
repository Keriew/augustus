#include "trade_sync.h"

#ifdef ENABLE_MULTIPLAYER

#include "ownership.h"
#include "network/session.h"
#include "network/serialize.h"
#include "network/protocol.h"
#include "building/building.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "game/resource.h"
#include "core/log.h"

#include <string.h>

#define MAX_REPLICATED_TRADERS 128

typedef enum {
    TRADER_STATE_NONE = 0,
    TRADER_STATE_TRAVELING,      /* En route to destination */
    TRADER_STATE_AT_STORAGE,     /* At storage building, trading */
    TRADER_STATE_RETURNING,      /* Heading back to origin */
    TRADER_STATE_ABORTED         /* Could not complete, cleaning up */
} replicated_trader_state;

typedef struct {
    int active;
    int figure_id;
    int city_id;
    int route_id;
    uint32_t network_entity_id;
    replicated_trader_state state;
    uint32_t state_version;      /* Incremented on each state change */
    int last_resource;           /* Last traded resource (for display) */
    int last_amount;             /* Last traded amount */
} replicated_trader;

static struct {
    replicated_trader traders[MAX_REPLICATED_TRADERS];
    int trader_count;
    uint32_t next_network_entity_id;
} trade_data;

void mp_trade_sync_init(void)
{
    memset(&trade_data, 0, sizeof(trade_data));
    trade_data.next_network_entity_id = 1;
}

void mp_trade_sync_clear(void)
{
    mp_trade_sync_init();
}

static replicated_trader *find_trader(int figure_id)
{
    for (int i = 0; i < MAX_REPLICATED_TRADERS; i++) {
        if (trade_data.traders[i].active && trade_data.traders[i].figure_id == figure_id) {
            return &trade_data.traders[i];
        }
    }
    return NULL;
}

static replicated_trader *alloc_trader(int figure_id)
{
    replicated_trader *existing = find_trader(figure_id);
    if (existing) {
        return existing;
    }
    for (int i = 0; i < MAX_REPLICATED_TRADERS; i++) {
        if (!trade_data.traders[i].active) {
            memset(&trade_data.traders[i], 0, sizeof(replicated_trader));
            trade_data.traders[i].active = 1;
            trade_data.traders[i].figure_id = figure_id;
            trade_data.traders[i].network_entity_id = trade_data.next_network_entity_id++;
            trade_data.traders[i].state = TRADER_STATE_TRAVELING;
            trade_data.traders[i].state_version = 1;
            trade_data.trader_count++;
            return &trade_data.traders[i];
        }
    }
    log_error("Replicated trader table full", 0, figure_id);
    return NULL;
}

static void free_trader(replicated_trader *t)
{
    if (t && t->active) {
        t->active = 0;
        trade_data.trader_count--;
    }
}

/* ---- Host: emit trader lifecycle events ---- */

void mp_trade_sync_emit_trader_spawned(int figure_id, int city_id, int route_id)
{
    if (!net_session_is_host()) {
        return;
    }

    replicated_trader *t = alloc_trader(figure_id);
    if (!t) {
        return;
    }
    t->city_id = city_id;
    t->route_id = route_id;
    t->state = TRADER_STATE_TRAVELING;

    uint8_t buf[48];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_TRADER_SPAWNED);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_u32(&s, t->network_entity_id);
    net_write_i32(&s, figure_id);
    net_write_i32(&s, city_id);
    net_write_i32(&s, route_id);
    net_write_u32(&s, t->state_version);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

void mp_trade_sync_emit_trader_reached_storage(int figure_id, int storage_building_id)
{
    if (!net_session_is_host()) {
        return;
    }

    replicated_trader *t = find_trader(figure_id);
    if (!t) {
        return;
    }
    t->state = TRADER_STATE_AT_STORAGE;
    t->state_version++;

    uint8_t buf[32];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_TRADER_REACHED_STORAGE);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_u32(&s, t->network_entity_id);
    net_write_i32(&s, figure_id);
    net_write_i32(&s, storage_building_id);
    net_write_u32(&s, t->state_version);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

void mp_trade_sync_emit_trader_trade_executed(int figure_id, int resource,
                                               int amount, int buying,
                                               int building_id)
{
    if (!net_session_is_host()) {
        return;
    }

    replicated_trader *t = find_trader(figure_id);
    if (t) {
        t->last_resource = resource;
        t->last_amount = amount;
        t->state_version++;
    }

    uint8_t buf[48];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_TRADER_TRADE_EXECUTED);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_u32(&s, t ? t->network_entity_id : 0);
    net_write_i32(&s, figure_id);
    net_write_i32(&s, resource);
    net_write_i32(&s, amount);
    net_write_u8(&s, (uint8_t)buying);
    net_write_i32(&s, building_id);
    net_write_u32(&s, t ? t->state_version : 0);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

void mp_trade_sync_emit_trader_returning(int figure_id)
{
    if (!net_session_is_host()) {
        return;
    }

    replicated_trader *t = find_trader(figure_id);
    if (t) {
        t->state = TRADER_STATE_RETURNING;
        t->state_version++;
    }

    uint8_t buf[32];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_TRADER_RETURNING);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_u32(&s, t ? t->network_entity_id : 0);
    net_write_i32(&s, figure_id);
    net_write_u32(&s, t ? t->state_version : 0);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

void mp_trade_sync_emit_trader_aborted(int figure_id, int reason)
{
    if (!net_session_is_host()) {
        return;
    }

    replicated_trader *t = find_trader(figure_id);
    if (t) {
        t->state = TRADER_STATE_ABORTED;
        t->state_version++;
    }

    uint8_t buf[32];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_TRADER_ABORTED);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_u32(&s, t ? t->network_entity_id : 0);
    net_write_i32(&s, figure_id);
    net_write_i32(&s, reason);
    net_write_u32(&s, t ? t->state_version : 0);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

void mp_trade_sync_emit_trader_despawned(int figure_id)
{
    if (!net_session_is_host()) {
        return;
    }

    replicated_trader *t = find_trader(figure_id);
    if (t) {
        free_trader(t);
    }

    uint8_t buf[16];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_TRADER_DESPAWNED);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_i32(&s, figure_id);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

/* ---- Host: broadcast route state ---- */

void mp_trade_sync_broadcast_route_state(int route_id)
{
    if (!net_session_is_host()) {
        return;
    }

    uint8_t buf[512];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_TRADE_POLICY_CHANGED);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_i32(&s, route_id);

    /* Write full route state */
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        net_write_i32(&s, trade_route_limit(route_id, r, 0));   /* buy limit */
        net_write_i32(&s, trade_route_traded(route_id, r, 0));   /* buy traded */
        net_write_i32(&s, trade_route_limit(route_id, r, 1));   /* sell limit */
        net_write_i32(&s, trade_route_traded(route_id, r, 1));   /* sell traded */
    }

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

void mp_trade_sync_broadcast_route_policy(int route_id, int resource,
                                           int is_export, int enabled)
{
    if (!net_session_is_host()) {
        return;
    }

    uint8_t buf[32];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_ROUTE_POLICY_SET);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_i32(&s, route_id);
    net_write_i32(&s, resource);
    net_write_u8(&s, (uint8_t)is_export);
    net_write_u8(&s, (uint8_t)enabled);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

void mp_trade_sync_broadcast_route_limit(int route_id, int resource,
                                          int is_buying, int amount)
{
    if (!net_session_is_host()) {
        return;
    }

    uint8_t buf[32];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_ROUTE_LIMIT_SET);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_i32(&s, route_id);
    net_write_i32(&s, resource);
    net_write_u8(&s, (uint8_t)is_buying);
    net_write_i32(&s, amount);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

/* ---- Client: handle events from host ---- */

void mp_trade_sync_handle_event(uint16_t event_type,
                                 const uint8_t *data, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)data, size);

    switch (event_type) {
        case NET_EVENT_TRADER_SPAWNED: {
            uint32_t net_id = net_read_u32(&s);
            int figure_id = net_read_i32(&s);
            int city_id = net_read_i32(&s);
            int route_id = net_read_i32(&s);
            uint32_t version = net_read_u32(&s);

            replicated_trader *t = alloc_trader(figure_id);
            if (t) {
                t->network_entity_id = net_id;
                t->city_id = city_id;
                t->route_id = route_id;
                t->state = TRADER_STATE_TRAVELING;
                t->state_version = version;
            }
            break;
        }
        case NET_EVENT_TRADER_REACHED_STORAGE: {
            uint32_t net_id = net_read_u32(&s);
            int figure_id = net_read_i32(&s);
            int storage_id = net_read_i32(&s);
            uint32_t version = net_read_u32(&s);

            replicated_trader *t = find_trader(figure_id);
            if (t) {
                t->state = TRADER_STATE_AT_STORAGE;
                t->state_version = version;
            }
            (void)net_id;
            (void)storage_id;
            break;
        }
        case NET_EVENT_TRADER_TRADE_EXECUTED: {
            uint32_t net_id = net_read_u32(&s);
            int figure_id = net_read_i32(&s);
            int resource = net_read_i32(&s);
            int amount = net_read_i32(&s);
            uint8_t buying = net_read_u8(&s);
            int building_id = net_read_i32(&s);
            uint32_t version = net_read_u32(&s);

            /* Client applies the trade result from host */
            replicated_trader *t = find_trader(figure_id);
            int route_id = t ? t->route_id : mp_ownership_get_trader_route(figure_id);
            if (trade_route_is_valid(route_id) && resource >= RESOURCE_MIN && resource < RESOURCE_MAX) {
                trade_route_increase_traded(route_id, resource, buying);
            }

            /* Apply warehouse/granary mutation on client so stock stays in sync */
            if (building_id > 0) {
                building *b = building_get(building_id);
                if (b && b->state == BUILDING_STATE_IN_USE) {
                    if (buying) {
                        /* Trader buying = city exporting = remove from warehouse */
                        if (b->type == BUILDING_GRANARY) {
                            building_granary_remove_export(b, resource, amount, 0);
                        } else if (b->type == BUILDING_WAREHOUSE) {
                            building_warehouse_remove_export(b, resource, amount, 0);
                        }
                    } else {
                        /* Trader selling = city importing = add to warehouse */
                        if (b->type == BUILDING_GRANARY) {
                            building_granary_add_import(b, resource, amount, 0);
                        } else if (b->type == BUILDING_WAREHOUSE) {
                            building_warehouse_add_import(b, resource, amount, 0);
                        }
                    }
                }
            }

            if (t) {
                t->last_resource = resource;
                t->last_amount = amount;
                t->state_version = version;
            }
            (void)net_id;
            break;
        }
        case NET_EVENT_TRADER_RETURNING: {
            uint32_t net_id = net_read_u32(&s);
            int figure_id = net_read_i32(&s);
            uint32_t version = net_read_u32(&s);

            replicated_trader *t = find_trader(figure_id);
            if (t) {
                t->state = TRADER_STATE_RETURNING;
                t->state_version = version;
            }
            (void)net_id;
            break;
        }
        case NET_EVENT_TRADER_ABORTED: {
            uint32_t net_id = net_read_u32(&s);
            int figure_id = net_read_i32(&s);
            int reason = net_read_i32(&s);
            uint32_t version = net_read_u32(&s);

            replicated_trader *t = find_trader(figure_id);
            if (t) {
                t->state = TRADER_STATE_ABORTED;
                t->state_version = version;
            }
            (void)net_id;
            (void)reason;
            break;
        }
        case NET_EVENT_TRADER_DESPAWNED: {
            int figure_id = net_read_i32(&s);
            replicated_trader *t = find_trader(figure_id);
            if (t) {
                free_trader(t);
            }
            break;
        }
        case NET_EVENT_TRADE_POLICY_CHANGED: {
            int route_id = net_read_i32(&s);
            if (trade_route_is_valid(route_id)) {
                for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                    int buy_limit = net_read_i32(&s);
                    int buy_traded = net_read_i32(&s);
                    int sell_limit = net_read_i32(&s);
                    int sell_traded = net_read_i32(&s);
                    trade_route_set_limit(route_id, r, buy_limit, 0);
                    trade_route_set_limit(route_id, r, sell_limit, 1);
                    (void)buy_traded;
                    (void)sell_traded;
                }
            }
            break;
        }
        default:
            break;
    }
}

int mp_trade_sync_is_authoritative(int figure_id)
{
    return net_session_is_host();
}

uint32_t mp_trade_sync_get_network_entity_id(int figure_id)
{
    replicated_trader *t = find_trader(figure_id);
    return t ? t->network_entity_id : 0;
}

int mp_trade_sync_get_trader_route(int figure_id)
{
    replicated_trader *t = find_trader(figure_id);
    return t ? t->route_id : -1;
}

/* ---- Serialization ---- */

void mp_trade_sync_serialize_routes(uint8_t *buffer, uint32_t *size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, 16384);

    int route_count = trade_route_count();
    net_write_i32(&s, route_count);

    for (int i = 0; i < route_count; i++) {
        net_write_u8(&s, (uint8_t)trade_route_is_valid(i));
        if (!trade_route_is_valid(i)) {
            continue;
        }
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            net_write_i32(&s, trade_route_limit(i, r, 0));
            net_write_i32(&s, trade_route_traded(i, r, 0));
            net_write_i32(&s, trade_route_limit(i, r, 1));
            net_write_i32(&s, trade_route_traded(i, r, 1));
        }
    }

    *size = (uint32_t)net_serializer_position(&s);
}

void mp_trade_sync_deserialize_routes(const uint8_t *buffer, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    int route_count = net_read_i32(&s);

    for (int i = 0; i < route_count && !net_serializer_has_overflow(&s); i++) {
        uint8_t valid = net_read_u8(&s);
        if (!valid) {
            continue;
        }
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            int buy_limit = net_read_i32(&s);
            int buy_traded = net_read_i32(&s);
            int sell_limit = net_read_i32(&s);
            int sell_traded = net_read_i32(&s);

            if (trade_route_is_valid(i)) {
                trade_route_set_limit(i, r, buy_limit, 0);
                trade_route_set_limit(i, r, sell_limit, 1);
            }
            (void)buy_traded;
            (void)sell_traded;
        }
    }
}

void mp_trade_sync_serialize_traders(uint8_t *buffer, uint32_t *size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, 8192);

    uint16_t count = 0;
    for (int i = 0; i < MAX_REPLICATED_TRADERS; i++) {
        if (trade_data.traders[i].active) {
            count++;
        }
    }
    net_write_u16(&s, count);

    for (int i = 0; i < MAX_REPLICATED_TRADERS; i++) {
        replicated_trader *t = &trade_data.traders[i];
        if (!t->active) {
            continue;
        }
        net_write_u32(&s, t->network_entity_id);
        net_write_i32(&s, t->figure_id);
        net_write_i32(&s, t->city_id);
        net_write_i32(&s, t->route_id);
        net_write_u8(&s, (uint8_t)t->state);
        net_write_u32(&s, t->state_version);
        net_write_i32(&s, t->last_resource);
        net_write_i32(&s, t->last_amount);
    }

    *size = (uint32_t)net_serializer_position(&s);
}

void mp_trade_sync_deserialize_traders(const uint8_t *buffer, uint32_t size)
{
    memset(trade_data.traders, 0, sizeof(trade_data.traders));
    trade_data.trader_count = 0;

    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    uint16_t count = net_read_u16(&s);

    for (int i = 0; i < count && i < MAX_REPLICATED_TRADERS && !net_serializer_has_overflow(&s); i++) {
        replicated_trader *t = &trade_data.traders[i];
        t->active = 1;
        t->network_entity_id = net_read_u32(&s);
        t->figure_id = net_read_i32(&s);
        t->city_id = net_read_i32(&s);
        t->route_id = net_read_i32(&s);
        t->state = (replicated_trader_state)net_read_u8(&s);
        t->state_version = net_read_u32(&s);
        t->last_resource = net_read_i32(&s);
        t->last_amount = net_read_i32(&s);
        trade_data.trader_count++;
    }
}

#endif /* ENABLE_MULTIPLAYER */
