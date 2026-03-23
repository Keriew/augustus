#include "empire_sync.h"

#ifdef ENABLE_MULTIPLAYER

#include "ownership.h"
#include "player_registry.h"
#include "network/serialize.h"
#include "network/session.h"
#include "network/protocol.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "city/resource.h"
#include "building/dock.h"
#include "core/log.h"

#include <string.h>

static struct {
    mp_city_trade_view views[MP_MAX_TRADE_VIEW_CITIES];
    int view_count;
    int dirty;
} sync_data;

void mp_empire_sync_init(void)
{
    memset(&sync_data, 0, sizeof(sync_data));
}

void mp_empire_sync_clear(void)
{
    mp_empire_sync_init();
}

static mp_city_trade_view *find_view(int city_id)
{
    for (int i = 0; i < sync_data.view_count; i++) {
        if (sync_data.views[i].city_id == city_id) {
            return &sync_data.views[i];
        }
    }
    return NULL;
}

static mp_city_trade_view *alloc_view(int city_id)
{
    mp_city_trade_view *existing = find_view(city_id);
    if (existing) {
        return existing;
    }
    if (sync_data.view_count >= MP_MAX_TRADE_VIEW_CITIES) {
        log_error("Trade view table full", 0, city_id);
        return NULL;
    }
    mp_city_trade_view *v = &sync_data.views[sync_data.view_count++];
    memset(v, 0, sizeof(mp_city_trade_view));
    v->city_id = city_id;
    return v;
}

void mp_empire_sync_register_player_city(int city_id, uint8_t player_id,
                                          int empire_object_id)
{
    mp_city_trade_view *view = alloc_view(city_id);
    if (view) {
        view->player_id = player_id;
        view->online = 1;
    }

    mp_ownership_set_city(city_id, MP_OWNER_REMOTE_PLAYER, player_id);
    mp_player_registry_set_city(player_id, city_id);

    sync_data.dirty = 1;
    log_info("Registered player city on empire", 0, city_id);
}

void mp_empire_sync_unregister_player_city(uint8_t player_id)
{
    for (int i = 0; i < sync_data.view_count; i++) {
        if (sync_data.views[i].player_id == player_id) {
            sync_data.views[i].online = 0;
            sync_data.dirty = 1;
            log_info("Player city marked offline", 0, player_id);
            break;
        }
    }
}

void mp_empire_sync_update_trade_views(void)
{
    /* Only the host should compute trade views from real game state */
    if (!net_session_is_host()) {
        return;
    }

    for (int i = 0; i < sync_data.view_count; i++) {
        mp_city_trade_view *view = &sync_data.views[i];
        if (!view->online) {
            continue;
        }

        empire_city *city = empire_city_get(view->city_id);
        if (!city || !city->in_use) {
            continue;
        }

        /* Update exportable/importable status for each resource */
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            view->exportable[r] = city->sells_resource[r] && city->is_open;
            view->importable[r] = city->buys_resource[r] && city->is_open;

            /* Get approximate stock for display */
            if (mp_ownership_is_city_local(view->city_id)) {
                view->stock_level[r] = city_resource_count_warehouses_amount(r);
            }
        }

        /* Update infrastructure status */
        view->dock_available = city->is_sea_trade ? building_dock_count_available() > 0 : 0;
        view->land_route_available = !city->is_sea_trade;
    }

    sync_data.dirty = 1;
}

void mp_empire_sync_broadcast_views(void)
{
    if (!sync_data.dirty || !net_session_is_host()) {
        return;
    }

    uint8_t buf[8192];
    uint32_t size = 0;
    mp_empire_sync_serialize(buf, &size);

    if (size > 0) {
        /* Wrap as HOST_EVENT with CITY_VIEW_UPDATED type */
        uint8_t event_buf[8192 + 16];
        net_serializer s;
        net_serializer_init(&s, event_buf, sizeof(event_buf));
        net_write_u16(&s, NET_EVENT_CITY_VIEW_UPDATED);
        net_write_u32(&s, net_session_get_authoritative_tick());
        net_write_u32(&s, size);
        net_write_raw(&s, buf, size);

        net_session_broadcast(NET_MSG_HOST_EVENT, event_buf,
                            (uint32_t)net_serializer_position(&s));
    }

    sync_data.dirty = 0;
}

const mp_city_trade_view *mp_empire_sync_get_trade_view(int city_id)
{
    return find_view(city_id);
}

int mp_empire_sync_get_city_id_for_player(uint8_t player_id)
{
    for (int i = 0; i < sync_data.view_count; i++) {
        if (sync_data.views[i].player_id == player_id) {
            return sync_data.views[i].city_id;
        }
    }
    return -1;
}

int mp_empire_sync_can_export_to_remote(int city_id, int resource)
{
    const mp_city_trade_view *view = find_view(city_id);
    if (!view || !view->online) {
        return 0;
    }
    if (resource < RESOURCE_MIN || resource >= RESOURCE_MAX) {
        return 0;
    }
    return view->importable[resource]; /* Remote can import = we can export to them */
}

int mp_empire_sync_can_import_from_remote(int city_id, int resource)
{
    const mp_city_trade_view *view = find_view(city_id);
    if (!view || !view->online) {
        return 0;
    }
    if (resource < RESOURCE_MIN || resource >= RESOURCE_MAX) {
        return 0;
    }
    return view->exportable[resource]; /* Remote can export = we can import from them */
}

void mp_empire_sync_serialize(uint8_t *buffer, uint32_t *size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, 8192);

    net_write_u16(&s, (uint16_t)sync_data.view_count);

    for (int i = 0; i < sync_data.view_count; i++) {
        mp_city_trade_view *v = &sync_data.views[i];
        net_write_i32(&s, v->city_id);
        net_write_i32(&s, v->player_id);
        net_write_u8(&s, (uint8_t)v->online);
        net_write_u8(&s, (uint8_t)v->dock_available);
        net_write_u8(&s, (uint8_t)v->land_route_available);

        for (int r = 0; r < RESOURCE_MAX; r++) {
            net_write_u8(&s, (uint8_t)v->exportable[r]);
            net_write_u8(&s, (uint8_t)v->importable[r]);
            net_write_i32(&s, v->stock_level[r]);
        }
    }

    *size = (uint32_t)net_serializer_position(&s);
}

void mp_empire_sync_deserialize(const uint8_t *buffer, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    uint16_t count = net_read_u16(&s);
    sync_data.view_count = 0;

    for (int i = 0; i < count && !net_serializer_has_overflow(&s); i++) {
        mp_city_trade_view *v = &sync_data.views[i];
        memset(v, 0, sizeof(mp_city_trade_view));

        v->city_id = net_read_i32(&s);
        v->player_id = net_read_i32(&s);
        v->online = net_read_u8(&s);
        v->dock_available = net_read_u8(&s);
        v->land_route_available = net_read_u8(&s);

        for (int r = 0; r < RESOURCE_MAX; r++) {
            v->exportable[r] = net_read_u8(&s);
            v->importable[r] = net_read_u8(&s);
            v->stock_level[r] = net_read_i32(&s);
        }

        sync_data.view_count++;
    }
}

void multiplayer_empire_sync_receive_event(const uint8_t *data, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)data, size);

    uint16_t event_type = net_read_u16(&s);
    uint32_t event_tick = net_read_u32(&s);
    (void)event_tick;

    switch (event_type) {
        case NET_EVENT_CITY_VIEW_UPDATED: {
            uint32_t payload_size = net_read_u32(&s);
            if (!net_serializer_has_overflow(&s) && payload_size > 0) {
                const uint8_t *payload = data + net_serializer_position(&s);
                mp_empire_sync_deserialize(payload, payload_size);
            }
            break;
        }
        case NET_EVENT_ROUTE_OPENED: {
            int city_id = net_read_i32(&s);
            uint8_t player_id = net_read_u8(&s);
            empire_city *city = empire_city_get(city_id);
            if (city) {
                city->is_open = 1;
            }
            log_info("Route opened by remote player", 0, player_id);
            break;
        }
        case NET_EVENT_ROUTE_CLOSED: {
            int city_id = net_read_i32(&s);
            uint8_t player_id = net_read_u8(&s);
            empire_city *city = empire_city_get(city_id);
            if (city) {
                city->is_open = 0;
            }
            log_info("Route closed by remote player", 0, player_id);
            break;
        }
        case NET_EVENT_TRADER_TRADE_EXECUTED:
        case NET_EVENT_TRADER_SPAWNED:
        case NET_EVENT_TRADER_DESPAWNED: {
            /* Handled by trade_sync */
            extern void mp_trade_sync_handle_event(uint16_t event_type,
                                                    const uint8_t *data, uint32_t remaining);
            uint32_t pos = (uint32_t)net_serializer_position(&s);
            if (pos < size) {
                mp_trade_sync_handle_event(event_type, data + pos, size - pos);
            }
            break;
        }
        default:
            break;
    }
}

#endif /* ENABLE_MULTIPLAYER */
