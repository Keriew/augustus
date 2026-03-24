#ifndef MULTIPLAYER_OWNERSHIP_H
#define MULTIPLAYER_OWNERSHIP_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

typedef enum {
    MP_OWNER_AI = 0,
    MP_OWNER_LOCAL_PLAYER = 1,
    MP_OWNER_REMOTE_PLAYER = 2
} mp_owner_type;

typedef enum {
    MP_ROUTE_AI_TO_AI = 0,
    MP_ROUTE_AI_TO_PLAYER,
    MP_ROUTE_PLAYER_TO_AI,
    MP_ROUTE_PLAYER_TO_PLAYER
} mp_route_owner_mode;

typedef enum {
    MP_ROUTE_STATE_INACTIVE = 0,
    MP_ROUTE_STATE_PENDING,      /* Created by host, awaiting confirmation broadcast */
    MP_ROUTE_STATE_ACTIVE,       /* Fully operational */
    MP_ROUTE_STATE_DISABLED,     /* Temporarily paused by owner */
    MP_ROUTE_STATE_OFFLINE,      /* Destination player disconnected */
    MP_ROUTE_STATE_DELETED       /* Marked for cleanup */
} mp_route_state;

/**
 * Ownership is the SINGLE SOURCE OF TRUTH for who owns what.
 * No other module should duplicate this logic.
 *
 * - Cities: which player owns which empire city
 * - Routes: full lifecycle with origin/dest players, state, versioning
 * - Traders: which figure belongs to which player and route
 */

void mp_ownership_init(void);
void mp_ownership_clear(void);

/* ---- City ownership ---- */

void mp_ownership_set_city(int city_id, mp_owner_type owner_type, uint8_t player_id);
mp_owner_type mp_ownership_get_city_owner_type(int city_id);
uint8_t mp_ownership_get_city_player_id(int city_id);
int mp_ownership_is_city_local(int city_id);
int mp_ownership_is_city_remote_player(int city_id);
int mp_ownership_is_city_player_owned(int city_id);
int mp_ownership_is_city_online(int city_id);
void mp_ownership_set_city_online(int city_id, int online);

/* ---- Route ownership with full lifecycle ---- */

/**
 * Create a route entry in the ownership table. Returns the allocated route slot index,
 * or -1 on failure. The route_id is the Augustus internal trade_route ID.
 */
int mp_ownership_create_route(int route_id, mp_route_owner_mode mode,
                               uint8_t origin_player_id, uint8_t dest_player_id,
                               uint32_t network_route_id);

void mp_ownership_delete_route(int route_id);

void mp_ownership_set_route_state(int route_id, mp_route_state state);
mp_route_state mp_ownership_get_route_state(int route_id);

void mp_ownership_set_route(int route_id, mp_route_owner_mode mode,
                             uint8_t origin_player_id, uint8_t dest_player_id);
mp_route_owner_mode mp_ownership_get_route_mode(int route_id);
uint8_t mp_ownership_get_route_origin_player(int route_id);
uint8_t mp_ownership_get_route_dest_player(int route_id);
int mp_ownership_is_route_player_to_player(int route_id);
int mp_ownership_is_route_active(int route_id);

/* Route state versioning (for conflict detection) */
void mp_ownership_increment_route_version(int route_id);
uint32_t mp_ownership_get_route_version(int route_id);

/* Route tick tracking */
void mp_ownership_set_route_open_tick(int route_id, uint32_t tick);
uint32_t mp_ownership_get_route_open_tick(int route_id);

/* Query: does a route exist between two cities? */
int mp_ownership_find_route_between(int city_a, int city_b);

/* Find the city owned by the local player (works on both host and client) */
int mp_ownership_find_local_city(void);

/* Count active routes for a player */
int mp_ownership_count_player_routes(uint8_t player_id);

/* Disable all routes involving a disconnected player */
void mp_ownership_set_player_routes_offline(uint8_t player_id);

/* Re-enable routes when a player reconnects */
void mp_ownership_set_player_routes_online(uint8_t player_id);

/* ---- Trader ownership ---- */

void mp_ownership_set_trader(int figure_id, uint8_t owner_player_id,
                              int origin_city_id, int dest_city_id, int route_id);
uint8_t mp_ownership_get_trader_owner(int figure_id);
int mp_ownership_get_trader_origin_city(int figure_id);
int mp_ownership_get_trader_dest_city(int figure_id);
int mp_ownership_get_trader_route(int figure_id);
void mp_ownership_clear_trader(int figure_id);
void mp_ownership_clear_route_traders(int route_id);

/* ---- Network IDs for replication ---- */

void mp_ownership_set_network_route_id(int route_id, uint32_t network_id);
uint32_t mp_ownership_get_network_route_id(int route_id);
void mp_ownership_set_network_entity_id(int figure_id, uint32_t network_id);
uint32_t mp_ownership_get_network_entity_id(int figure_id);

/* Next network ID allocation (host only) */
uint32_t mp_ownership_allocate_network_route_id(void);
uint32_t mp_ownership_allocate_network_entity_id(void);

/* ---- Trade permission ---- */

int mp_ownership_can_trade(int city_a, int city_b);

/* ---- Reapply after save load ---- */
void mp_ownership_reapply_city_owners(void);

/* ---- Serialization ---- */

void mp_ownership_serialize(uint8_t *buffer, uint32_t *size);
void mp_ownership_deserialize(const uint8_t *buffer, uint32_t size);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_OWNERSHIP_H */
