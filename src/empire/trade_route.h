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

/**
 * Ensure a route entry exists for the given route_id.
 * Advances the internal array as needed. Used by multiplayer clients
 * to mirror host-allocated route IDs.
 * @return 1 if the route_id is now valid, 0 on failure
 */
int trade_route_ensure_id(int route_id);

int trade_route_count(void);

int trade_route_is_valid(int route_id);

void trade_route_set(int route_id, resource_type resource, int limit, int buying);

int trade_route_limit(int route_id, resource_type resource, int buying);

int trade_route_traded(int route_id, resource_type resource, int buying);

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

void trade_routes_save_state(buffer *trade_routes);

void trade_routes_load_state(buffer *trade_routes);

void trade_routes_migrate_to_buys_sells(buffer *limit, buffer *traded, int version);

#ifdef ENABLE_MULTIPLAYER

typedef enum {
    ROUTE_MODE_AI_TO_PLAYER = 0,
    ROUTE_MODE_PLAYER_TO_AI,
    ROUTE_MODE_PLAYER_TO_PLAYER
} trade_route_player_mode;

int trade_route_is_player_to_player(int route_id);
void trade_route_bind_players(int route_id, int origin_player_id, int destination_player_id);
int trade_route_get_network_id(int route_id);
void trade_route_set_network_id(int route_id, int network_route_id);
int trade_route_get_origin_player(int route_id);
int trade_route_get_dest_player(int route_id);
trade_route_player_mode trade_route_get_player_mode(int route_id);

/* Extended lifecycle API for command_bus */
void trade_route_set_player_binding(int route_id, uint8_t origin_player_id, uint8_t dest_player_id);
void trade_route_clear_player_binding(int route_id);
void trade_route_set_export_enabled(int route_id, int resource, int enabled);
void trade_route_set_import_enabled(int route_id, int resource, int enabled);

#endif /* ENABLE_MULTIPLAYER */

#endif // EMPIRE_TRADE_ROUTE_H
