#ifndef EMPIRE_TRADE_ROUTE_H
#define EMPIRE_TRADE_ROUTE_H

#include "core/buffer.h"
#include "game/resource.h"

typedef enum {
    RESOURCE_BUYS = -1,
    RESOURCE_SELLS = 1,
    RESOURCE_NOT_TRADED = 0
} city_resource_state;

#define LEGACY_MAX_ROUTES 20

int trade_route_init(void);

int trade_route_new(void);

int trade_route_count(void);

int trade_route_is_valid(int route_id);

int trade_route_set_open(int route_id);

int trade_route_was_open(int route_id, int year);

void trade_route_set(int route_id, resource_type resource, int limit, int buying);

int trade_route_limit(int route_id, resource_type resource, int buying);

int trade_route_traded(int route_id, resource_type resource, int buying);

int trade_route_get_history_years_stored(void);

int trade_route_history_limit(int route_id, resource_type resource, int buying, int year);

int trade_route_history_traded(int route_id, resource_type resource, int buying, int year);

void trade_route_set_limit(int route_id, resource_type resource, int amount, int buying);

/**
 * Increases the trade limit of the resource
 * @param route_id Trade route
 * @param resource Resource
 * @return True on success, false if the limit couldn't be increased
 */
int trade_route_legacy_increase_limit(int route_id, resource_type resource, int buying);

/**
 * Decreases the trade limit of the resource
 * @param route_id Trade route
 * @param resource Resource
 * @return True on success, false if the limit couldn't be decreased
 */
int trade_route_legacy_decrease_limit(int route_id, resource_type resource, int buying);

void trade_route_increase_traded(int route_id, resource_type resource, int buying);

void trade_route_reset_traded(int route_id);

int trade_route_limit_reached(int route_id, resource_type resource, int buying);

void trade_route_save_history(void);

void trade_routes_save_state(buffer *trade_routes);

void trade_routes_load_state(buffer *trade_routes, int version);

void trade_history_save_state(buffer *trade_history);

void trade_history_load_state(buffer *trade_history, int version);

void trade_history_clear_state(void);

void trade_routes_migrate_to_buys_sells(buffer *limit, buffer *traded, int version);

#endif // EMPIRE_TRADE_ROUTE_H
