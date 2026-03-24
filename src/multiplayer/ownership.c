#include "ownership.h"

#ifdef ENABLE_MULTIPLAYER

#include "player_registry.h"
#include "network/serialize.h"
#include "network/session.h"
#include "core/log.h"

#include <string.h>

#define MAX_OWNED_CITIES  200
#define MAX_OWNED_ROUTES  200
#define MAX_OWNED_TRADERS 600

typedef struct {
    int in_use;
    int city_id;
    mp_owner_type owner_type;
    uint8_t player_id;
    int is_online;
} city_ownership;

typedef struct {
    int in_use;
    int route_id;
    mp_route_owner_mode mode;
    mp_route_state state;
    uint8_t origin_player_id;
    uint8_t dest_player_id;
    uint32_t network_route_id;
    uint32_t route_state_version;
    uint32_t authoritative_open_tick;
} route_ownership;

typedef struct {
    int in_use;
    int figure_id;
    uint8_t owner_player_id;
    int origin_city_id;
    int dest_city_id;
    int route_id;
    uint32_t network_entity_id;
} trader_ownership;

static struct {
    city_ownership cities[MAX_OWNED_CITIES];
    route_ownership routes[MAX_OWNED_ROUTES];
    trader_ownership traders[MAX_OWNED_TRADERS];
    uint32_t next_network_route_id;
    uint32_t next_network_entity_id;
} ownership_data;

void mp_ownership_init(void)
{
    memset(&ownership_data, 0, sizeof(ownership_data));
    ownership_data.next_network_route_id = 1;
    ownership_data.next_network_entity_id = 1;
}

void mp_ownership_clear(void)
{
    mp_ownership_init();
}

/* ---- City Ownership ---- */

static city_ownership *find_city_entry(int city_id)
{
    for (int i = 0; i < MAX_OWNED_CITIES; i++) {
        if (ownership_data.cities[i].in_use && ownership_data.cities[i].city_id == city_id) {
            return &ownership_data.cities[i];
        }
    }
    return 0;
}

static city_ownership *alloc_city_entry(int city_id)
{
    city_ownership *existing = find_city_entry(city_id);
    if (existing) {
        return existing;
    }
    for (int i = 0; i < MAX_OWNED_CITIES; i++) {
        if (!ownership_data.cities[i].in_use) {
            ownership_data.cities[i].in_use = 1;
            ownership_data.cities[i].city_id = city_id;
            ownership_data.cities[i].is_online = 1;
            return &ownership_data.cities[i];
        }
    }
    log_error("City ownership table full", 0, city_id);
    return 0;
}

void mp_ownership_set_city(int city_id, mp_owner_type owner_type, uint8_t player_id)
{
    city_ownership *entry = alloc_city_entry(city_id);
    if (entry) {
        entry->owner_type = owner_type;
        entry->player_id = player_id;
    }
}

mp_owner_type mp_ownership_get_city_owner_type(int city_id)
{
    city_ownership *entry = find_city_entry(city_id);
    return entry ? entry->owner_type : MP_OWNER_AI;
}

uint8_t mp_ownership_get_city_player_id(int city_id)
{
    city_ownership *entry = find_city_entry(city_id);
    return entry ? entry->player_id : 0;
}

int mp_ownership_is_city_local(int city_id)
{
    city_ownership *entry = find_city_entry(city_id);
    if (!entry) {
        return 0;
    }
    if (entry->owner_type == MP_OWNER_LOCAL_PLAYER) {
        return 1;
    }
    if (entry->owner_type == MP_OWNER_REMOTE_PLAYER) {
        mp_player *local = mp_player_registry_get_local();
        return local && local->player_id == entry->player_id;
    }
    return 0;
}

int mp_ownership_is_city_remote_player(int city_id)
{
    city_ownership *entry = find_city_entry(city_id);
    if (!entry) {
        return 0;
    }
    if (entry->owner_type != MP_OWNER_REMOTE_PLAYER &&
        entry->owner_type != MP_OWNER_LOCAL_PLAYER) {
        return 0;
    }
    mp_player *local = mp_player_registry_get_local();
    if (!local) {
        return entry->owner_type == MP_OWNER_REMOTE_PLAYER;
    }
    return local->player_id != entry->player_id;
}

int mp_ownership_is_city_player_owned(int city_id)
{
    city_ownership *entry = find_city_entry(city_id);
    if (!entry) {
        return 0;
    }
    return entry->owner_type == MP_OWNER_LOCAL_PLAYER ||
           entry->owner_type == MP_OWNER_REMOTE_PLAYER;
}

int mp_ownership_is_city_online(int city_id)
{
    city_ownership *entry = find_city_entry(city_id);
    if (!entry) {
        return 0;
    }
    return entry->is_online;
}

void mp_ownership_set_city_online(int city_id, int online)
{
    city_ownership *entry = find_city_entry(city_id);
    if (entry) {
        entry->is_online = online;
    }
}

/* ---- Route Ownership ---- */

static route_ownership *find_route_entry(int route_id)
{
    for (int i = 0; i < MAX_OWNED_ROUTES; i++) {
        if (ownership_data.routes[i].in_use && ownership_data.routes[i].route_id == route_id) {
            return &ownership_data.routes[i];
        }
    }
    return 0;
}

static route_ownership *alloc_route_entry(int route_id)
{
    route_ownership *existing = find_route_entry(route_id);
    if (existing) {
        return existing;
    }
    for (int i = 0; i < MAX_OWNED_ROUTES; i++) {
        if (!ownership_data.routes[i].in_use) {
            ownership_data.routes[i].in_use = 1;
            ownership_data.routes[i].route_id = route_id;
            return &ownership_data.routes[i];
        }
    }
    log_error("Route ownership table full", 0, route_id);
    return 0;
}

int mp_ownership_create_route(int route_id, mp_route_owner_mode mode,
                               uint8_t origin_player_id, uint8_t dest_player_id,
                               uint32_t network_route_id)
{
    route_ownership *entry = alloc_route_entry(route_id);
    if (!entry) {
        return -1;
    }
    entry->mode = mode;
    entry->state = MP_ROUTE_STATE_PENDING;
    entry->origin_player_id = origin_player_id;
    entry->dest_player_id = dest_player_id;
    entry->network_route_id = network_route_id;
    entry->route_state_version = 1;
    entry->authoritative_open_tick = 0;
    return 1;
}

void mp_ownership_delete_route(int route_id)
{
    route_ownership *entry = find_route_entry(route_id);
    if (entry) {
        entry->state = MP_ROUTE_STATE_DELETED;
        /* Don't clear in_use yet — let the cleanup cycle handle it */
    }
}

void mp_ownership_set_route_state(int route_id, mp_route_state state)
{
    route_ownership *entry = find_route_entry(route_id);
    if (entry) {
        entry->state = state;
        entry->route_state_version++;
    }
}

mp_route_state mp_ownership_get_route_state(int route_id)
{
    route_ownership *entry = find_route_entry(route_id);
    return entry ? entry->state : MP_ROUTE_STATE_INACTIVE;
}

void mp_ownership_set_route(int route_id, mp_route_owner_mode mode,
                             uint8_t origin_player_id, uint8_t dest_player_id)
{
    route_ownership *entry = alloc_route_entry(route_id);
    if (entry) {
        entry->mode = mode;
        entry->origin_player_id = origin_player_id;
        entry->dest_player_id = dest_player_id;
    }
}

mp_route_owner_mode mp_ownership_get_route_mode(int route_id)
{
    route_ownership *entry = find_route_entry(route_id);
    return entry ? entry->mode : MP_ROUTE_AI_TO_AI;
}

uint8_t mp_ownership_get_route_origin_player(int route_id)
{
    route_ownership *entry = find_route_entry(route_id);
    return entry ? entry->origin_player_id : 0;
}

uint8_t mp_ownership_get_route_dest_player(int route_id)
{
    route_ownership *entry = find_route_entry(route_id);
    return entry ? entry->dest_player_id : 0;
}

int mp_ownership_is_route_player_to_player(int route_id)
{
    route_ownership *entry = find_route_entry(route_id);
    return entry && entry->mode == MP_ROUTE_PLAYER_TO_PLAYER;
}

int mp_ownership_is_route_active(int route_id)
{
    route_ownership *entry = find_route_entry(route_id);
    return entry && entry->state == MP_ROUTE_STATE_ACTIVE;
}

void mp_ownership_increment_route_version(int route_id)
{
    route_ownership *entry = find_route_entry(route_id);
    if (entry) {
        entry->route_state_version++;
    }
}

uint32_t mp_ownership_get_route_version(int route_id)
{
    route_ownership *entry = find_route_entry(route_id);
    return entry ? entry->route_state_version : 0;
}

void mp_ownership_set_route_open_tick(int route_id, uint32_t tick)
{
    route_ownership *entry = find_route_entry(route_id);
    if (entry) {
        entry->authoritative_open_tick = tick;
    }
}

uint32_t mp_ownership_get_route_open_tick(int route_id)
{
    route_ownership *entry = find_route_entry(route_id);
    return entry ? entry->authoritative_open_tick : 0;
}

int mp_ownership_find_route_between(int city_a, int city_b)
{
    for (int i = 0; i < MAX_OWNED_ROUTES; i++) {
        route_ownership *r = &ownership_data.routes[i];
        if (!r->in_use || r->state == MP_ROUTE_STATE_DELETED) {
            continue;
        }
        /* Check city ownership to find the route */
        city_ownership *ca = find_city_entry(city_a);
        city_ownership *cb = find_city_entry(city_b);
        if (!ca || !cb) {
            continue;
        }
        if ((r->origin_player_id == ca->player_id && r->dest_player_id == cb->player_id) ||
            (r->origin_player_id == cb->player_id && r->dest_player_id == ca->player_id)) {
            return r->route_id;
        }
    }
    return -1;
}

int mp_ownership_find_local_city(void)
{
    mp_player *local = mp_player_registry_get_local();
    if (!local) {
        return -1;
    }
    for (int i = 0; i < MAX_OWNED_CITIES; i++) {
        city_ownership *c = &ownership_data.cities[i];
        if (!c->in_use) {
            continue;
        }
        if (c->player_id == local->player_id &&
            (c->owner_type == MP_OWNER_LOCAL_PLAYER ||
             c->owner_type == MP_OWNER_REMOTE_PLAYER)) {
            return c->city_id;
        }
    }
    return -1;
}

int mp_ownership_count_player_routes(uint8_t player_id)
{
    int count = 0;
    for (int i = 0; i < MAX_OWNED_ROUTES; i++) {
        route_ownership *r = &ownership_data.routes[i];
        if (!r->in_use || r->state == MP_ROUTE_STATE_DELETED) {
            continue;
        }
        if (r->origin_player_id == player_id || r->dest_player_id == player_id) {
            count++;
        }
    }
    return count;
}

void mp_ownership_set_player_routes_offline(uint8_t player_id)
{
    for (int i = 0; i < MAX_OWNED_ROUTES; i++) {
        route_ownership *r = &ownership_data.routes[i];
        if (!r->in_use) {
            continue;
        }
        if (r->state != MP_ROUTE_STATE_ACTIVE && r->state != MP_ROUTE_STATE_PENDING) {
            continue;
        }
        if (r->origin_player_id == player_id || r->dest_player_id == player_id) {
            r->state = MP_ROUTE_STATE_OFFLINE;
            r->route_state_version++;
            log_info("Route set offline due to player disconnect", 0, r->route_id);
        }
    }
}

void mp_ownership_set_player_routes_online(uint8_t player_id)
{
    for (int i = 0; i < MAX_OWNED_ROUTES; i++) {
        route_ownership *r = &ownership_data.routes[i];
        if (!r->in_use || r->state != MP_ROUTE_STATE_OFFLINE) {
            continue;
        }
        if (r->origin_player_id == player_id || r->dest_player_id == player_id) {
            r->state = MP_ROUTE_STATE_ACTIVE;
            r->route_state_version++;
            log_info("Route restored after player reconnect", 0, r->route_id);
        }
    }
}

/* ---- Trader Ownership ---- */

static trader_ownership *find_trader_entry(int figure_id)
{
    for (int i = 0; i < MAX_OWNED_TRADERS; i++) {
        if (ownership_data.traders[i].in_use &&
            ownership_data.traders[i].figure_id == figure_id) {
            return &ownership_data.traders[i];
        }
    }
    return 0;
}

static trader_ownership *alloc_trader_entry(int figure_id)
{
    trader_ownership *existing = find_trader_entry(figure_id);
    if (existing) {
        return existing;
    }
    for (int i = 0; i < MAX_OWNED_TRADERS; i++) {
        if (!ownership_data.traders[i].in_use) {
            ownership_data.traders[i].in_use = 1;
            ownership_data.traders[i].figure_id = figure_id;
            return &ownership_data.traders[i];
        }
    }
    log_error("Trader ownership table full", 0, figure_id);
    return 0;
}

void mp_ownership_set_trader(int figure_id, uint8_t owner_player_id,
                              int origin_city_id, int dest_city_id, int route_id)
{
    trader_ownership *entry = alloc_trader_entry(figure_id);
    if (entry) {
        entry->owner_player_id = owner_player_id;
        entry->origin_city_id = origin_city_id;
        entry->dest_city_id = dest_city_id;
        entry->route_id = route_id;
    }
}

uint8_t mp_ownership_get_trader_owner(int figure_id)
{
    trader_ownership *entry = find_trader_entry(figure_id);
    return entry ? entry->owner_player_id : 0;
}

int mp_ownership_get_trader_origin_city(int figure_id)
{
    trader_ownership *entry = find_trader_entry(figure_id);
    return entry ? entry->origin_city_id : -1;
}

int mp_ownership_get_trader_dest_city(int figure_id)
{
    trader_ownership *entry = find_trader_entry(figure_id);
    return entry ? entry->dest_city_id : -1;
}

int mp_ownership_get_trader_route(int figure_id)
{
    trader_ownership *entry = find_trader_entry(figure_id);
    return entry ? entry->route_id : -1;
}

void mp_ownership_clear_trader(int figure_id)
{
    trader_ownership *entry = find_trader_entry(figure_id);
    if (entry) {
        memset(entry, 0, sizeof(trader_ownership));
    }
}

/* ---- Network IDs ---- */

void mp_ownership_set_network_route_id(int route_id, uint32_t network_id)
{
    route_ownership *entry = alloc_route_entry(route_id);
    if (entry) {
        entry->network_route_id = network_id;
    }
}

uint32_t mp_ownership_get_network_route_id(int route_id)
{
    route_ownership *entry = find_route_entry(route_id);
    return entry ? entry->network_route_id : 0;
}

void mp_ownership_set_network_entity_id(int figure_id, uint32_t network_id)
{
    trader_ownership *entry = alloc_trader_entry(figure_id);
    if (entry) {
        entry->network_entity_id = network_id;
    }
}

uint32_t mp_ownership_get_network_entity_id(int figure_id)
{
    trader_ownership *entry = find_trader_entry(figure_id);
    return entry ? entry->network_entity_id : 0;
}

uint32_t mp_ownership_allocate_network_route_id(void)
{
    return ownership_data.next_network_route_id++;
}

uint32_t mp_ownership_allocate_network_entity_id(void)
{
    return ownership_data.next_network_entity_id++;
}

/* ---- Trade Permission ---- */

int mp_ownership_can_trade(int city_a, int city_b)
{
    int a_player = mp_ownership_is_city_player_owned(city_a);
    int b_player = mp_ownership_is_city_player_owned(city_b);

    /* AI to AI is always allowed */
    if (!a_player && !b_player) {
        return 1;
    }

    /* Player to AI or AI to Player is always allowed */
    if (a_player != b_player) {
        return 1;
    }

    /* Player to Player: both must be online */
    if (!mp_ownership_is_city_online(city_a)) {
        return 0;
    }
    if (!mp_ownership_is_city_online(city_b)) {
        return 0;
    }

    /* Check if a route exists and is active */
    int route_id = mp_ownership_find_route_between(city_a, city_b);
    if (route_id < 0) {
        return 0;
    }
    return mp_ownership_is_route_active(route_id);
}

/* ---- Serialization ---- */

void mp_ownership_serialize(uint8_t *buffer, uint32_t *size)
{
    net_serializer s;
    net_serializer_init(&s, buffer, 65536);

    /* Global state */
    net_write_u32(&s, ownership_data.next_network_route_id);
    net_write_u32(&s, ownership_data.next_network_entity_id);

    /* Cities */
    uint16_t city_count = 0;
    for (int i = 0; i < MAX_OWNED_CITIES; i++) {
        if (ownership_data.cities[i].in_use) {
            city_count++;
        }
    }
    net_write_u16(&s, city_count);
    for (int i = 0; i < MAX_OWNED_CITIES; i++) {
        city_ownership *c = &ownership_data.cities[i];
        if (!c->in_use) {
            continue;
        }
        net_write_i32(&s, c->city_id);
        net_write_u8(&s, (uint8_t)c->owner_type);
        net_write_u8(&s, c->player_id);
        net_write_u8(&s, (uint8_t)c->is_online);
    }

    /* Routes */
    uint16_t route_count = 0;
    for (int i = 0; i < MAX_OWNED_ROUTES; i++) {
        if (ownership_data.routes[i].in_use) {
            route_count++;
        }
    }
    net_write_u16(&s, route_count);
    for (int i = 0; i < MAX_OWNED_ROUTES; i++) {
        route_ownership *r = &ownership_data.routes[i];
        if (!r->in_use) {
            continue;
        }
        net_write_i32(&s, r->route_id);
        net_write_u8(&s, (uint8_t)r->mode);
        net_write_u8(&s, (uint8_t)r->state);
        net_write_u8(&s, r->origin_player_id);
        net_write_u8(&s, r->dest_player_id);
        net_write_u32(&s, r->network_route_id);
        net_write_u32(&s, r->route_state_version);
        net_write_u32(&s, r->authoritative_open_tick);
    }

    /* Traders */
    uint16_t trader_count = 0;
    for (int i = 0; i < MAX_OWNED_TRADERS; i++) {
        if (ownership_data.traders[i].in_use) {
            trader_count++;
        }
    }
    net_write_u16(&s, trader_count);
    for (int i = 0; i < MAX_OWNED_TRADERS; i++) {
        trader_ownership *t = &ownership_data.traders[i];
        if (!t->in_use) {
            continue;
        }
        net_write_i32(&s, t->figure_id);
        net_write_u8(&s, t->owner_player_id);
        net_write_i32(&s, t->origin_city_id);
        net_write_i32(&s, t->dest_city_id);
        net_write_i32(&s, t->route_id);
        net_write_u32(&s, t->network_entity_id);
    }

    *size = (uint32_t)net_serializer_position(&s);
}

void mp_ownership_deserialize(const uint8_t *buffer, uint32_t size)
{
    mp_ownership_clear();

    net_serializer s;
    net_serializer_init(&s, (uint8_t *)buffer, size);

    /* Global state */
    ownership_data.next_network_route_id = net_read_u32(&s);
    ownership_data.next_network_entity_id = net_read_u32(&s);

    /* Cities */
    uint16_t city_count = net_read_u16(&s);
    for (int i = 0; i < city_count && !net_serializer_has_overflow(&s); i++) {
        int city_id = net_read_i32(&s);
        mp_owner_type otype = (mp_owner_type)net_read_u8(&s);
        uint8_t pid = net_read_u8(&s);
        uint8_t online = net_read_u8(&s);
        mp_ownership_set_city(city_id, otype, pid);
        mp_ownership_set_city_online(city_id, online);
    }

    /* Routes */
    uint16_t route_count = net_read_u16(&s);
    for (int i = 0; i < route_count && !net_serializer_has_overflow(&s); i++) {
        int route_id = net_read_i32(&s);
        mp_route_owner_mode mode = (mp_route_owner_mode)net_read_u8(&s);
        mp_route_state state = (mp_route_state)net_read_u8(&s);
        uint8_t origin = net_read_u8(&s);
        uint8_t dest = net_read_u8(&s);
        uint32_t net_id = net_read_u32(&s);
        uint32_t version = net_read_u32(&s);
        uint32_t open_tick = net_read_u32(&s);

        mp_ownership_create_route(route_id, mode, origin, dest, net_id);
        mp_ownership_set_route_state(route_id, state);

        route_ownership *entry = find_route_entry(route_id);
        if (entry) {
            entry->route_state_version = version;
            entry->authoritative_open_tick = open_tick;
        }
    }

    /* Traders */
    uint16_t trader_count = net_read_u16(&s);
    for (int i = 0; i < trader_count && !net_serializer_has_overflow(&s); i++) {
        int figure_id = net_read_i32(&s);
        uint8_t owner = net_read_u8(&s);
        int origin = net_read_i32(&s);
        int dest = net_read_i32(&s);
        int route = net_read_i32(&s);
        uint32_t net_id = net_read_u32(&s);
        mp_ownership_set_trader(figure_id, owner, origin, dest, route);
        mp_ownership_set_network_entity_id(figure_id, net_id);
    }
}

#endif /* ENABLE_MULTIPLAYER */
