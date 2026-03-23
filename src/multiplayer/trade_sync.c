#include "trade_sync.h"

#ifdef ENABLE_MULTIPLAYER

#include "ownership.h"
#include "network/session.h"
#include "network/serialize.h"
#include "network/protocol.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "game/resource.h"
#include "core/log.h"

#include <string.h>

#define MAX_REPLICATED_TRADERS 128

typedef struct {
    int active;
    int figure_id;
    int city_id;
    int route_id;
    uint32_t network_entity_id;
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
            trade_data.traders[i].active = 1;
            trade_data.traders[i].figure_id = figure_id;
            trade_data.traders[i].network_entity_id = trade_data.next_network_entity_id++;
            trade_data.trader_count++;
            return &trade_data.traders[i];
        }
    }
    return NULL;
}

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

    /* Broadcast event */
    uint8_t buf[32];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_TRADER_SPAWNED);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_u32(&s, t->network_entity_id);
    net_write_i32(&s, figure_id);
    net_write_i32(&s, city_id);
    net_write_i32(&s, route_id);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

void mp_trade_sync_emit_trader_trade_executed(int figure_id, int resource,
                                               int amount, int buying)
{
    if (!net_session_is_host()) {
        return;
    }

    uint8_t buf[32];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_TRADER_TRADE_EXECUTED);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_i32(&s, figure_id);
    net_write_i32(&s, resource);
    net_write_i32(&s, amount);
    net_write_u8(&s, (uint8_t)buying);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

void mp_trade_sync_emit_trader_despawned(int figure_id)
{
    if (!net_session_is_host()) {
        return;
    }

    replicated_trader *t = find_trader(figure_id);
    if (t) {
        t->active = 0;
        trade_data.trader_count--;
    }

    uint8_t buf[16];
    net_serializer s;
    net_serializer_init(&s, buf, sizeof(buf));
    net_write_u16(&s, NET_EVENT_TRADER_DESPAWNED);
    net_write_u32(&s, net_session_get_authoritative_tick());
    net_write_i32(&s, figure_id);

    net_session_broadcast(NET_MSG_HOST_EVENT, buf, (uint32_t)net_serializer_position(&s));
}

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

            replicated_trader *t = alloc_trader(figure_id);
            if (t) {
                t->network_entity_id = net_id;
                t->city_id = city_id;
                t->route_id = route_id;
            }
            break;
        }
        case NET_EVENT_TRADER_TRADE_EXECUTED: {
            int figure_id = net_read_i32(&s);
            int resource = net_read_i32(&s);
            int amount = net_read_i32(&s);
            uint8_t buying = net_read_u8(&s);

            /* Client applies the trade result from host */
            int route_id = mp_ownership_get_trader_route(figure_id);
            if (trade_route_is_valid(route_id) && resource >= RESOURCE_MIN && resource < RESOURCE_MAX) {
                trade_route_increase_traded(route_id, resource, buying);
            }
            (void)amount;
            break;
        }
        case NET_EVENT_TRADER_DESPAWNED: {
            int figure_id = net_read_i32(&s);
            replicated_trader *t = find_trader(figure_id);
            if (t) {
                t->active = 0;
                trade_data.trader_count--;
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
        trade_data.trader_count++;
    }
}

#endif /* ENABLE_MULTIPLAYER */
