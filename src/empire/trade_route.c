#include "trade_route.h"

#include "core/array.h"
#include "core/log.h"
#include "empire/city.h"
#include "game/save_version.h"

#include <string.h>

typedef struct {
    int limit[RESOURCE_MAX];
    int traded[RESOURCE_MAX];
} route_resource;

typedef struct {
    route_resource buys;
    route_resource sells;
} trade_route;

static array(trade_route) routes;

int trade_route_init(void)
{
    if (!array_init(routes, LEGACY_MAX_ROUTES, 0, 0)) {
        log_error("Unable to create memory for trade routes. The game will now crash.", 0, 0);
        return 0;
    }
    // Discard route 0
    array_advance(routes);
    return 1;
}

int trade_route_new(void)
{
    array_advance(routes);
    return routes.size - 1;
}

int trade_route_count(void)
{
    return routes.size;
}

int trade_route_is_valid(int route_id)
{
    return route_id >= 0 && (unsigned int) route_id < routes.size;
}

void trade_route_set(int route_id, resource_type resource, int limit, int buying)
{
    trade_route *route = array_item(routes, route_id);
    if (buying) {
        route->buys.limit[resource] = limit;
        route->buys.traded[resource] = 0;
    } else {
        route->sells.limit[resource] = limit;
        route->sells.traded[resource] = 0;
    }
}

int trade_route_limit(int route_id, resource_type resource, int buying)
{
    return buying ? array_item(routes, route_id)->buys.limit[resource] :
        array_item(routes, route_id)->sells.limit[resource];
}

int trade_route_traded(int route_id, resource_type resource, int buying)
{
    return buying ? array_item(routes, route_id)->buys.traded[resource] :
        array_item(routes, route_id)->sells.traded[resource];
}

void trade_route_set_limit(int route_id, resource_type resource, int amount, int buying)
{
    if (buying) {
        array_item(routes, route_id)->buys.limit[resource] = amount;
    } else {
        array_item(routes, route_id)->sells.limit[resource] = amount;
    }
}

static route_resource *get_route_resource(int route_id, int buying)
{
    if (buying) {
        return &array_item(routes, route_id)->buys;
    } else {
        return &array_item(routes, route_id)->sells;
    }
}

int trade_route_legacy_increase_limit(int route_id, resource_type resource, int buying)
{
    route_resource *route = get_route_resource(route_id, buying);
    switch (route->limit[resource]) {
        case 0: route->limit[resource] = 15; break;
        case 15: route->limit[resource] = 25; break;
        case 25: route->limit[resource] = 40; break;
    }
    return route->limit[resource];
}

int trade_route_legacy_decrease_limit(int route_id, resource_type resource, int buying)
{
    route_resource *route = get_route_resource(route_id, buying);
    if (buying) {
        route = &array_item(routes, route_id)->buys;
    } else {
        route = &array_item(routes, route_id)->sells;
    }
    switch (route->limit[resource]) {
        case 40: route->limit[resource] = 25; break;
        case 25: route->limit[resource] = 15; break;
        case 15: route->limit[resource] = 0; break;
    }
    return route->limit[resource];
}

void trade_route_increase_traded(int route_id, resource_type resource, int buying)
{
    if (buying) {
        array_item(routes, route_id)->buys.traded[resource]++;
    } else {
        array_item(routes, route_id)->sells.traded[resource]++;
    }
}

void trade_route_reset_traded(int route_id)
{
    trade_route *route = array_item(routes, route_id);
    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        route->buys.traded[r] = route->sells.traded[r] = 0;
    }
}

int trade_route_limit_reached(int route_id, resource_type resource, int buying)
{
    route_resource *route = get_route_resource(route_id, buying);
    return route->traded[resource] >= route->limit[resource];
}

void trade_routes_save_state(buffer *trade_routes)
{
    int buf_size = sizeof(int32_t) * RESOURCE_MAX * 2 * routes.size * 2;
    uint8_t *buf_data = malloc(buf_size + sizeof(int32_t));
    buffer_init(trade_routes, buf_data, buf_size + sizeof(int32_t));
    buffer_write_i32(trade_routes, routes.size);

    trade_route *route;
    array_foreach(routes, route) {
        for (int i = 0; i < 2; i++) {
            for (resource_type r = 0; r < RESOURCE_MAX; r++) {
                buffer_write_i32(trade_routes, i ? route->buys.limit[r] : route->sells.limit[r]);
                buffer_write_i32(trade_routes, i ? route->buys.traded[r] : route->sells.traded[r]);
            }
        }
    }
}

void trade_routes_load_state(buffer *trade_routes)
{
    int routes_to_load = buffer_read_i32(trade_routes);
    if (!array_init(routes, LEGACY_MAX_ROUTES, 0, 0) || !array_expand(routes, routes_to_load)) {
        log_error("Unable to create memory for trade routes. The game will now crash.", 0, 0);
        return;
    }
    for (int i = 0; i < routes_to_load; i++) {
        trade_route *route = array_next(routes);
        for (int i = 0; i < 2; i++) {
            for (int r = 0; r < resource_total_mapped(); r++) {
                resource_type remapped = resource_remap(r);
                if (i) {
                    route->buys.limit[remapped] = buffer_read_i32(trade_routes);
                    route->buys.traded[remapped] = buffer_read_i32(trade_routes);
                } else {
                    route->sells.limit[remapped] = buffer_read_i32(trade_routes);
                    route->sells.traded[remapped] = buffer_read_i32(trade_routes);
                }
            }
        }
    }
}

void trade_routes_migrate_to_buys_sells(buffer *limit, buffer *traded, int version)
{
    int routes_to_load = version <= SAVE_GAME_LAST_STATIC_SCENARIO_OBJECTS ? LEGACY_MAX_ROUTES : buffer_read_i32(limit);
    if (!array_init(routes, LEGACY_MAX_ROUTES, 0, 0) || !array_expand(routes, routes_to_load)) {
        log_error("Unable to create memory for trade routes. The game will now crash.", 0, 0);
        return;
    }
    for (int i = 0; i < routes_to_load; i++) {
        trade_route *route = array_next(routes);
        int city_id = empire_city_get_for_trade_route(i);
        if (city_id < 0) {
            continue;
        }
        for (int r = 0; r < resource_total_mapped(); r++) {
            resource_type remapped = resource_remap(r);
            int limit_amount = buffer_read_i32(limit);
            int traded_amount = buffer_read_i32(traded);
            if (empire_city_buys_resource(city_id, remapped)) {
                route->buys.limit[remapped] = limit_amount;
                route->buys.traded[remapped] = traded_amount;
                route->sells.limit[remapped] = route->sells.traded[remapped] = 0;
            } else if (empire_city_sells_resource(city_id, remapped)) {
                route->sells.limit[remapped] = limit_amount;
                route->sells.traded[remapped] = traded_amount;
                route->buys.limit[remapped] = route->buys.traded[remapped] = 0;
            } else {
                route->sells.limit[remapped] = route->sells.traded[remapped] =
                    route->buys.limit[remapped] = route->buys.traded[remapped] = 0;
            }
        }
    }
}

#ifdef ENABLE_MULTIPLAYER

#include <string.h>

#define MAX_ROUTE_BINDINGS 200

typedef struct {
    int in_use;
    int route_id;
    int origin_player_id;
    int destination_player_id;
    trade_route_player_mode mode;
    int network_route_id;
} route_player_binding;

static route_player_binding route_bindings[MAX_ROUTE_BINDINGS];

static route_player_binding *find_binding(int route_id)
{
    for (int i = 0; i < MAX_ROUTE_BINDINGS; i++) {
        if (route_bindings[i].in_use && route_bindings[i].route_id == route_id) {
            return &route_bindings[i];
        }
    }
    return NULL;
}

static route_player_binding *alloc_binding(int route_id)
{
    route_player_binding *existing = find_binding(route_id);
    if (existing) {
        return existing;
    }
    for (int i = 0; i < MAX_ROUTE_BINDINGS; i++) {
        if (!route_bindings[i].in_use) {
            memset(&route_bindings[i], 0, sizeof(route_player_binding));
            route_bindings[i].in_use = 1;
            route_bindings[i].route_id = route_id;
            return &route_bindings[i];
        }
    }
    return NULL;
}

int trade_route_is_player_to_player(int route_id)
{
    route_player_binding *b = find_binding(route_id);
    return b && b->mode == ROUTE_MODE_PLAYER_TO_PLAYER;
}

void trade_route_bind_players(int route_id, int origin_player_id, int destination_player_id)
{
    route_player_binding *b = alloc_binding(route_id);
    if (!b) {
        return;
    }
    b->origin_player_id = origin_player_id;
    b->destination_player_id = destination_player_id;

    if (origin_player_id >= 0 && destination_player_id >= 0) {
        b->mode = ROUTE_MODE_PLAYER_TO_PLAYER;
    } else if (origin_player_id >= 0) {
        b->mode = ROUTE_MODE_PLAYER_TO_AI;
    } else {
        b->mode = ROUTE_MODE_AI_TO_PLAYER;
    }
}

int trade_route_get_network_id(int route_id)
{
    route_player_binding *b = find_binding(route_id);
    return b ? b->network_route_id : 0;
}

void trade_route_set_network_id(int route_id, int network_route_id)
{
    route_player_binding *b = alloc_binding(route_id);
    if (b) {
        b->network_route_id = network_route_id;
    }
}

int trade_route_get_origin_player(int route_id)
{
    route_player_binding *b = find_binding(route_id);
    return b ? b->origin_player_id : -1;
}

int trade_route_get_dest_player(int route_id)
{
    route_player_binding *b = find_binding(route_id);
    return b ? b->destination_player_id : -1;
}

trade_route_player_mode trade_route_get_player_mode(int route_id)
{
    route_player_binding *b = find_binding(route_id);
    return b ? b->mode : ROUTE_MODE_AI_TO_PLAYER;
}

#endif /* ENABLE_MULTIPLAYER */
