#include "trade_route.h"

#include "core/array.h"
#include "core/log.h"
#include "empire/city.h"
#include "game/save_version.h"

#include <string.h>

#define MAX_TRADE_HISTORY_YEARS 7

typedef struct {
    int limit[RESOURCE_MAX];
    int traded[RESOURCE_MAX];
} route_resource;

typedef struct {
    route_resource buys;
    route_resource sells;
    unsigned char open; // needed for ledger
} trade_route;

typedef array(trade_route) trade_route_array;
static trade_route_array routes;
static trade_route_array trade_history[MAX_TRADE_HISTORY_YEARS];
static unsigned char years_stored = 0;

static int init_trade_route_array(trade_route_array *route_array, int routes_to_load)
{
    return array_init(*route_array, LEGACY_MAX_ROUTES, 0, 0) && array_expand(*route_array, routes_to_load);
}

static int trade_route_array_buffer_size(const trade_route_array *route_array)
{
    return sizeof(int32_t) + (int) route_array->size * (int) (sizeof(int32_t) * RESOURCE_MAX * 4 + sizeof(uint8_t));
}

static int get_trade_history_buffer_size(void)
{
    int buf_size = sizeof(uint8_t);
    for (int i = 0; i < MAX_TRADE_HISTORY_YEARS; i++) {
        buf_size += trade_route_array_buffer_size(&trade_history[i]);
    }
    return buf_size;
}

static void save_trade_route_array_state(buffer *buf, const trade_route_array *route_array)
{
    buffer_write_i32(buf, route_array->size);

    trade_route *route;
    array_foreach(*route_array, route)
    {
        for (int i = 0; i < 2; i++) {
            for (resource_type r = 0; r < RESOURCE_MAX; r++) {
                buffer_write_i32(buf, i ? route->buys.limit[r] : route->sells.limit[r]);
                buffer_write_i32(buf, i ? route->buys.traded[r] : route->sells.traded[r]);
            }
        }
        buffer_write_u8(buf, route->open);
    }
}

static int load_trade_route_array_state(buffer *buf, trade_route_array *route_array, int version, int default_open_from_city)
{
    int routes_to_load = buffer_read_i32(buf);
    if (!init_trade_route_array(route_array, routes_to_load)) {
        return 0;
    }

    for (int i = 0; i < routes_to_load; i++) {
        trade_route *route = array_next(*route_array);
        for (int j = 0; j < 2; j++) {
            for (int r = 0; r < resource_total_mapped(); r++) {
                resource_type remapped = resource_remap(r);
                if (j) {
                    route->buys.limit[remapped] = buffer_read_i32(buf);
                    route->buys.traded[remapped] = buffer_read_i32(buf);
                } else {
                    route->sells.limit[remapped] = buffer_read_i32(buf);
                    route->sells.traded[remapped] = buffer_read_i32(buf);
                }
            }
        }
        if (version > SAVE_GAME_LAST_NO_LEDGER) {
            route->open = buffer_read_u8(buf);
        } else if (default_open_from_city) {
            route->open = empire_city_is_trade_route_open(i);
        }
    }
    return 1;
}

int trade_route_init(void)
{
    if (!array_init(routes, LEGACY_MAX_ROUTES, 0, 0)) {
        log_error("Unable to create memory for trade routes. The game will now crash.", 0, 0);
        return 0;
    }

    // Discard route 0
    array_advance(routes);

    for (int i = 0; i < MAX_TRADE_HISTORY_YEARS; i++) {
        if (!array_init(trade_history[i], LEGACY_MAX_ROUTES, 0, 0)) {
            log_error("Unable to create memory for trade route history. The game will now crash.", 0, 0);
            return 0;
        }
    }
    years_stored = 0;
    return 1;
}

int trade_route_set_open(int route_id)
{
    array_item(routes, route_id)->open = 1;
    return array_item(routes, route_id)->open;
}

int trade_route_was_open(int route_id, int year)
{
    if (year < 0 || year >= MAX_TRADE_HISTORY_YEARS) {
        return 0;
    }
    return array_item(trade_history[year], route_id)->open;
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

int trade_route_get_history_years_stored(void)
{
    return years_stored;
}

int trade_route_history_limit(int route_id, resource_type resource, int buying, int year)
{
    if (year < 0 || year >= MAX_TRADE_HISTORY_YEARS) {
        return 0;
    }
    if (buying) {
        return array_item(trade_history[year], route_id)->buys.limit[resource];
    } else {
        return array_item(trade_history[year], route_id)->sells.limit[resource];
    }
}

int trade_route_history_traded(int route_id, resource_type resource, int buying, int year)
{
    if (year < 0 || year >= MAX_TRADE_HISTORY_YEARS) {
        return 0;
    }
    if (array_item(trade_history[year], route_id)->open == 0) {
        return -1;
    }
    if (buying) {
        return array_item(trade_history[year], route_id)->buys.traded[resource];
    } else {
        return array_item(trade_history[year], route_id)->sells.traded[resource];
    }
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

static int copy_trade_route_array(trade_route_array *dest, const trade_route_array *src)
{
    if (!array_expand(*dest, src->size)) {
        return 0;
    }

    dest->size = src->size;

    for (unsigned int i = 0; i < src->size; i++) {
        trade_route *src_route = array_item(*src, i);
        trade_route *dest_route = array_item(*dest, i);

        *dest_route = *src_route;
    }

    return 1;
}

void trade_route_save_history(void)
{
    // Shift older years towards the end:
    // [5] -> [6], [4] -> [5], ..., [0] -> [1]
    for (int i = MAX_TRADE_HISTORY_YEARS - 1; i > 0; i--) {
        if (!copy_trade_route_array(&trade_history[i], &trade_history[i - 1])) {
            log_error("Unable to expand trade route history.", 0, 0);
            return;
        }
    }

    // move current routes as the [0] history entry
    if (!copy_trade_route_array(&trade_history[0], &routes)) {
        log_error("Unable to expand trade route history.", 0, 0);
    }
    years_stored = years_stored < MAX_TRADE_HISTORY_YEARS ? years_stored + 1 : MAX_TRADE_HISTORY_YEARS;
}

void trade_routes_save_state(buffer *trade_routes)
{
    uint8_t *buf_data = malloc(trade_route_array_buffer_size(&routes));
    buffer_init(trade_routes, buf_data, trade_route_array_buffer_size(&routes));
    save_trade_route_array_state(trade_routes, &routes);
}

void trade_routes_load_state(buffer *trade_routes, int version)
{
    if (!load_trade_route_array_state(trade_routes, &routes, version, 1)) {
        log_error("Unable to create memory for trade routes. The game will now crash.", 0, 0);
    }
}

void trade_history_save_state(buffer *buf)
{
    uint8_t *buf_data = malloc(get_trade_history_buffer_size());
    buffer_init(buf, buf_data, get_trade_history_buffer_size());

    buffer_write_u8(buf, years_stored);

    for (int i = 0; i < MAX_TRADE_HISTORY_YEARS; i++) {
        save_trade_route_array_state(buf, &trade_history[i]);
    }
}

void trade_history_clear_state(void)
{
    years_stored = 0;
    for (int i = 0; i < MAX_TRADE_HISTORY_YEARS; i++) {
        if (!init_trade_route_array(&trade_history[i], 0)) {
            log_error("Unable to create memory for trade route history. The game will now crash.", 0, 0);
            return;
        }
    }
}

void trade_history_load_state(buffer *buf, int version)
{
    years_stored = buffer_read_u8(buf);
    if (years_stored > MAX_TRADE_HISTORY_YEARS) {
        years_stored = MAX_TRADE_HISTORY_YEARS;
    }

    for (int i = 0; i < MAX_TRADE_HISTORY_YEARS; i++) {
        if (!load_trade_route_array_state(buf, &trade_history[i], version, 0)) {
            log_error("Unable to create memory for trade route history. The game will now crash.", 0, 0);
            return;
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
        route->open = empire_city_is_trade_route_open(i);
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
