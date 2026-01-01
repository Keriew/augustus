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
                if (i) {
                    route->buys.limit[resource_remap(r)] = buffer_read_i32(trade_routes);
                    route->buys.traded[resource_remap(r)] = buffer_read_i32(trade_routes);
                } else {
                    route->sells.limit[resource_remap(r)] = buffer_read_i32(trade_routes);
                    route->sells.traded[resource_remap(r)] = buffer_read_i32(trade_routes);
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
            if (empire_city_buys_resource(city_id, resource_remap(r))) {
                route->buys.limit[resource_remap(r)] = buffer_read_i32(limit);
                route->buys.traded[resource_remap(r)] = buffer_read_i32(traded);
                route->sells.limit[resource_remap(r)] = route->sells.traded[resource_remap(r)] = 0;
            } else if (empire_city_sells_resource(city_id, resource_remap(r))) {
                route->sells.limit[resource_remap(r)] = buffer_read_i32(limit);
                route->sells.traded[resource_remap(r)] = buffer_read_i32(traded);
                route->buys.limit[resource_remap(r)] = route->buys.traded[resource_remap(r)] = 0;
            } else {
                route->sells.limit[resource_remap(r)] = route->sells.traded[resource_remap(r)] =
                    route->buys.limit[resource_remap(r)] = route->buys.traded[resource_remap(r)] = 0;
            }
        }
    }
}