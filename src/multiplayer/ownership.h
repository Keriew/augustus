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

/**
 * Ownership binds empire objects (cities, routes, traders) to players.
 * All ownership queries go through this module, not scattered through game code.
 */

void mp_ownership_init(void);
void mp_ownership_clear(void);

/* City ownership */
void mp_ownership_set_city(int city_id, mp_owner_type owner_type, uint8_t player_id);
mp_owner_type mp_ownership_get_city_owner_type(int city_id);
uint8_t mp_ownership_get_city_player_id(int city_id);
int mp_ownership_is_city_local(int city_id);
int mp_ownership_is_city_remote_player(int city_id);
int mp_ownership_is_city_player_owned(int city_id);

/* Route ownership */
void mp_ownership_set_route(int route_id, mp_route_owner_mode mode,
                            uint8_t origin_player_id, uint8_t dest_player_id);
mp_route_owner_mode mp_ownership_get_route_mode(int route_id);
uint8_t mp_ownership_get_route_origin_player(int route_id);
uint8_t mp_ownership_get_route_dest_player(int route_id);
int mp_ownership_is_route_player_to_player(int route_id);

/* Trader ownership */
void mp_ownership_set_trader(int figure_id, uint8_t owner_player_id,
                             int origin_city_id, int dest_city_id, int route_id);
uint8_t mp_ownership_get_trader_owner(int figure_id);
int mp_ownership_get_trader_origin_city(int figure_id);
int mp_ownership_get_trader_dest_city(int figure_id);
int mp_ownership_get_trader_route(int figure_id);
void mp_ownership_clear_trader(int figure_id);

/* Network IDs for replication */
void mp_ownership_set_network_route_id(int route_id, uint32_t network_id);
uint32_t mp_ownership_get_network_route_id(int route_id);
void mp_ownership_set_network_entity_id(int figure_id, uint32_t network_id);
uint32_t mp_ownership_get_network_entity_id(int figure_id);

/* Trade permission between players */
int mp_ownership_can_trade(int city_a, int city_b);

/* Serialization */
void mp_ownership_serialize(uint8_t *buffer, uint32_t *size);
void mp_ownership_deserialize(const uint8_t *buffer, uint32_t size);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_OWNERSHIP_H */
