#include "mp_trade_route.h"

#ifdef ENABLE_MULTIPLAYER

#include "mp_debug_log.h"
#include "network/serialize.h"
#include "game/resource.h"
#include "core/log.h"

#include <string.h>

static struct {
    mp_trade_route_instance routes[MP_TRADE_ROUTE_MAX];
    uint32_t next_instance_id;
    int count;
} route_data;

void mp_trade_route_init(void)
{
    memset(&route_data, 0, sizeof(route_data));
    route_data.next_instance_id = 1; /* 0 is invalid */
}

void mp_trade_route_clear(void)
{
    mp_trade_route_init();
}

static mp_trade_route_instance *alloc_route(void)
{
    for (int i = 0; i < MP_TRADE_ROUTE_MAX; i++) {
        if (!route_data.routes[i].in_use) {
            return &route_data.routes[i];
        }
    }
    return 0;
}

uint32_t mp_trade_route_create(uint8_t origin_player_id, int origin_city_id,
                                uint8_t dest_player_id, int dest_city_id,
                                int claudius_route_id, uint32_t network_route_id,
                                mp_trade_route_transport transport)
{
    mp_trade_route_instance *r = alloc_route();
    if (!r) {
        MP_LOG_ERROR("MP_TRADE_ROUTE", "Route table full (max %d)", MP_TRADE_ROUTE_MAX);
        return MP_TRADE_ROUTE_INVALID_ID;
    }

    memset(r, 0, sizeof(*r));
    r->in_use = 1;
    r->instance_id = route_data.next_instance_id++;
    r->network_route_id = network_route_id;
    r->origin_player_id = origin_player_id;
    r->origin_city_id = origin_city_id;
    r->dest_player_id = dest_player_id;
    r->dest_city_id = dest_city_id;
    r->claudius_route_id = claudius_route_id;
    r->is_player_to_player = (dest_player_id != 0xFF) ? 1 : 0;
    r->transport = transport;
    r->status = MP_TROUTE_ACTIVE;
    r->state_version = 1;

    /* Initialize all resources as disabled by default */
    for (int res = 0; res < RESOURCE_MAX; res++) {
        r->resources[res].export_enabled = 0;
        r->resources[res].import_enabled = 0;
        r->resources[res].export_limit = 0;
        r->resources[res].import_limit = 0;
    }

    route_data.count++;
    MP_LOG_INFO("MP_TRADE_ROUTE", "Created route #%u: player %d city %d -> player %d city %d (aug_route=%d)",
                r->instance_id, origin_player_id, origin_city_id,
                dest_player_id, dest_city_id, claudius_route_id);
    return r->instance_id;
}

int mp_trade_route_delete(uint32_t instance_id)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r) {
        return 0;
    }
    MP_LOG_INFO("MP_TRADE_ROUTE", "Deleted route #%u", instance_id);
    r->status = MP_TROUTE_DELETED;
    r->in_use = 0;
    route_data.count--;
    return 1;
}

int mp_trade_route_set_status(uint32_t instance_id, mp_trade_route_status status)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r) {
        return 0;
    }
    r->status = status;
    r->state_version++;
    return 1;
}

mp_trade_route_instance *mp_trade_route_get(uint32_t instance_id)
{
    if (instance_id == MP_TRADE_ROUTE_INVALID_ID) {
        return 0;
    }
    for (int i = 0; i < MP_TRADE_ROUTE_MAX; i++) {
        if (route_data.routes[i].in_use && route_data.routes[i].instance_id == instance_id) {
            return &route_data.routes[i];
        }
    }
    return 0;
}

mp_trade_route_instance *mp_trade_route_find_by_endpoints(int origin_city_id, int dest_city_id)
{
    for (int i = 0; i < MP_TRADE_ROUTE_MAX; i++) {
        mp_trade_route_instance *r = &route_data.routes[i];
        if (!r->in_use) continue;
        if (r->origin_city_id == origin_city_id && r->dest_city_id == dest_city_id) {
            return r;
        }
        /* Also check reverse direction for bidirectional routes */
        if (r->origin_city_id == dest_city_id && r->dest_city_id == origin_city_id) {
            return r;
        }
    }
    return 0;
}

mp_trade_route_instance *mp_trade_route_find_by_claudius_route(int claudius_route_id)
{
    for (int i = 0; i < MP_TRADE_ROUTE_MAX; i++) {
        mp_trade_route_instance *r = &route_data.routes[i];
        if (r->in_use && r->claudius_route_id == claudius_route_id) {
            return r;
        }
    }
    return 0;
}

int mp_trade_route_count_player_routes(uint8_t player_id)
{
    int count = 0;
    for (int i = 0; i < MP_TRADE_ROUTE_MAX; i++) {
        mp_trade_route_instance *r = &route_data.routes[i];
        if (!r->in_use) continue;
        if (r->origin_player_id == player_id || r->dest_player_id == player_id) {
            count++;
        }
    }
    return count;
}

int mp_trade_route_count_active(void)
{
    int count = 0;
    for (int i = 0; i < MP_TRADE_ROUTE_MAX; i++) {
        if (route_data.routes[i].in_use && route_data.routes[i].status == MP_TROUTE_ACTIVE) {
            count++;
        }
    }
    return count;
}

void mp_trade_route_foreach(mp_trade_route_callback cb, void *userdata)
{
    for (int i = 0; i < MP_TRADE_ROUTE_MAX; i++) {
        if (route_data.routes[i].in_use) {
            if (cb(&route_data.routes[i], userdata)) {
                return;
            }
        }
    }
}

void mp_trade_route_foreach_active(mp_trade_route_callback cb, void *userdata)
{
    for (int i = 0; i < MP_TRADE_ROUTE_MAX; i++) {
        mp_trade_route_instance *r = &route_data.routes[i];
        if (r->in_use && r->status == MP_TROUTE_ACTIVE) {
            if (cb(r, userdata)) {
                return;
            }
        }
    }
}

/* ---- Resource policy ---- */

int mp_trade_route_set_resource_export(uint32_t instance_id, int resource, int enabled, int limit)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r || resource < RESOURCE_MIN || resource >= RESOURCE_MAX) {
        return 0;
    }
    r->resources[resource].export_enabled = enabled ? 1 : 0;
    r->resources[resource].export_limit = limit;
    r->state_version++;
    return 1;
}

int mp_trade_route_set_resource_import(uint32_t instance_id, int resource, int enabled, int limit)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r || resource < RESOURCE_MIN || resource >= RESOURCE_MAX) {
        return 0;
    }
    r->resources[resource].import_enabled = enabled ? 1 : 0;
    r->resources[resource].import_limit = limit;
    r->state_version++;
    return 1;
}

int mp_trade_route_can_export(uint32_t instance_id, int resource)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r || r->status != MP_TROUTE_ACTIVE) return 0;
    if (resource < RESOURCE_MIN || resource >= RESOURCE_MAX) return 0;
    if (!r->resources[resource].export_enabled) return 0;
    if (r->resources[resource].export_limit > 0 &&
        r->resources[resource].exported_this_year >= r->resources[resource].export_limit) {
        return 0;
    }
    return 1;
}

int mp_trade_route_can_import(uint32_t instance_id, int resource)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r || r->status != MP_TROUTE_ACTIVE) return 0;
    if (resource < RESOURCE_MIN || resource >= RESOURCE_MAX) return 0;
    if (!r->resources[resource].import_enabled) return 0;
    if (r->resources[resource].import_limit > 0 &&
        r->resources[resource].imported_this_year >= r->resources[resource].import_limit) {
        return 0;
    }
    return 1;
}

int mp_trade_route_export_remaining(uint32_t instance_id, int resource)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r || resource < RESOURCE_MIN || resource >= RESOURCE_MAX) return 0;
    if (!r->resources[resource].export_enabled) return 0;
    if (r->resources[resource].export_limit <= 0) return 9999; /* no limit */
    int remaining = r->resources[resource].export_limit - r->resources[resource].exported_this_year;
    return remaining > 0 ? remaining : 0;
}

int mp_trade_route_import_remaining(uint32_t instance_id, int resource)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r || resource < RESOURCE_MIN || resource >= RESOURCE_MAX) return 0;
    if (!r->resources[resource].import_enabled) return 0;
    if (r->resources[resource].import_limit <= 0) return 9999;
    int remaining = r->resources[resource].import_limit - r->resources[resource].imported_this_year;
    return remaining > 0 ? remaining : 0;
}

/* ---- Trade execution ---- */

int mp_trade_route_record_export(uint32_t instance_id, int resource, int amount)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r || resource < RESOURCE_MIN || resource >= RESOURCE_MAX || amount <= 0) return 0;
    r->resources[resource].exported_this_year += amount;
    r->total_exported += amount;
    r->total_transactions++;
    r->state_version++;
    return 1;
}

int mp_trade_route_record_import(uint32_t instance_id, int resource, int amount)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r || resource < RESOURCE_MIN || resource >= RESOURCE_MAX || amount <= 0) return 0;
    r->resources[resource].imported_this_year += amount;
    r->total_imported += amount;
    r->total_transactions++;
    r->state_version++;
    return 1;
}

int mp_trade_route_rollback_export(uint32_t instance_id, int resource, int amount)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r || resource < RESOURCE_MIN || resource >= RESOURCE_MAX || amount <= 0) return 0;
    r->resources[resource].exported_this_year -= amount;
    if (r->resources[resource].exported_this_year < 0) {
        r->resources[resource].exported_this_year = 0;
    }
    r->total_exported -= amount;
    if (r->total_exported < 0) {
        r->total_exported = 0;
    }
    r->state_version++;
    return 1;
}

int mp_trade_route_rollback_import(uint32_t instance_id, int resource, int amount)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (!r || resource < RESOURCE_MIN || resource >= RESOURCE_MAX || amount <= 0) return 0;
    r->resources[resource].imported_this_year -= amount;
    if (r->resources[resource].imported_this_year < 0) {
        r->resources[resource].imported_this_year = 0;
    }
    r->total_imported -= amount;
    if (r->total_imported < 0) {
        r->total_imported = 0;
    }
    r->state_version++;
    return 1;
}

/* ---- Annual reset ---- */

void mp_trade_route_reset_annual_counters(void)
{
    for (int i = 0; i < MP_TRADE_ROUTE_MAX; i++) {
        mp_trade_route_instance *r = &route_data.routes[i];
        if (!r->in_use) continue;
        for (int res = 0; res < RESOURCE_MAX; res++) {
            r->resources[res].exported_this_year = 0;
            r->resources[res].imported_this_year = 0;
        }
        r->state_version++;
    }
    MP_LOG_INFO("MP_TRADE_ROUTE", "Annual counters reset for all routes");
}

/* ---- Version tracking ---- */

uint32_t mp_trade_route_get_version(uint32_t instance_id)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    return r ? r->state_version : 0;
}

void mp_trade_route_increment_version(uint32_t instance_id)
{
    mp_trade_route_instance *r = mp_trade_route_get(instance_id);
    if (r) r->state_version++;
}

/* ---- Serialization ---- */

void mp_trade_route_serialize(uint8_t *buffer, uint32_t *out_size, uint32_t max_size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, max_size);

    /* Header */
    net_write_u32(&s, route_data.next_instance_id);
    net_write_u16(&s, (uint16_t)route_data.count);

    /* Each active route */
    for (int i = 0; i < MP_TRADE_ROUTE_MAX; i++) {
        mp_trade_route_instance *r = &route_data.routes[i];
        if (!r->in_use) continue;

        net_write_u32(&s, r->instance_id);
        net_write_u32(&s, r->network_route_id);
        net_write_u8(&s, r->origin_player_id);
        net_write_i32(&s, r->origin_city_id);
        net_write_u8(&s, r->dest_player_id);
        net_write_i32(&s, r->dest_city_id);
        net_write_i32(&s, r->claudius_route_id);
        net_write_u8(&s, r->is_player_to_player);
        net_write_u8(&s, (uint8_t)r->transport);
        net_write_u8(&s, (uint8_t)r->status);
        net_write_u32(&s, r->created_tick);
        net_write_u32(&s, r->last_trade_tick);
        net_write_u32(&s, r->state_version);
        net_write_i32(&s, r->total_exported);
        net_write_i32(&s, r->total_imported);
        net_write_i32(&s, r->total_transactions);

        /* Per-resource data */
        for (int res = RESOURCE_MIN; res < RESOURCE_MAX; res++) {
            mp_trade_route_resource *rr = &r->resources[res];
            net_write_i32(&s, rr->export_limit);
            net_write_i32(&s, rr->import_limit);
            net_write_i32(&s, rr->exported_this_year);
            net_write_i32(&s, rr->imported_this_year);
            net_write_u8(&s, rr->export_enabled);
            net_write_u8(&s, rr->import_enabled);
        }
    }

    *out_size = (uint32_t)net_serializer_position(&s);
}

void mp_trade_route_deserialize(const uint8_t *buffer, uint32_t size)
{
    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    mp_trade_route_clear();

    route_data.next_instance_id = net_read_u32(&s);
    int count = net_read_u16(&s);

    for (int i = 0; i < count && !net_serializer_has_overflow(&s); i++) {
        mp_trade_route_instance *r = alloc_route();
        if (!r) {
            MP_LOG_ERROR("MP_TRADE_ROUTE", "Route table full during deserialize at entry %d", i);
            break;
        }

        memset(r, 0, sizeof(*r));
        r->in_use = 1;
        r->instance_id = net_read_u32(&s);
        r->network_route_id = net_read_u32(&s);
        r->origin_player_id = net_read_u8(&s);
        r->origin_city_id = net_read_i32(&s);
        r->dest_player_id = net_read_u8(&s);
        r->dest_city_id = net_read_i32(&s);
        r->claudius_route_id = net_read_i32(&s);
        r->is_player_to_player = net_read_u8(&s);
        r->transport = (mp_trade_route_transport)net_read_u8(&s);
        r->status = (mp_trade_route_status)net_read_u8(&s);
        r->created_tick = net_read_u32(&s);
        r->last_trade_tick = net_read_u32(&s);
        r->state_version = net_read_u32(&s);
        r->total_exported = net_read_i32(&s);
        r->total_imported = net_read_i32(&s);
        r->total_transactions = net_read_i32(&s);

        for (int res = RESOURCE_MIN; res < RESOURCE_MAX; res++) {
            mp_trade_route_resource *rr = &r->resources[res];
            rr->export_limit = net_read_i32(&s);
            rr->import_limit = net_read_i32(&s);
            rr->exported_this_year = net_read_i32(&s);
            rr->imported_this_year = net_read_i32(&s);
            rr->export_enabled = net_read_u8(&s);
            rr->import_enabled = net_read_u8(&s);
        }

        route_data.count++;
    }

    MP_LOG_INFO("MP_TRADE_ROUTE", "Deserialized %d routes (next_id=%u)",
                route_data.count, route_data.next_instance_id);
}

#endif /* ENABLE_MULTIPLAYER */
