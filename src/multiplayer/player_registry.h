#ifndef MULTIPLAYER_PLAYER_REGISTRY_H
#define MULTIPLAYER_PLAYER_REGISTRY_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

#define MP_MAX_PLAYERS 8

typedef enum {
    MP_PLAYER_STATUS_NONE = 0,
    MP_PLAYER_STATUS_LOBBY,
    MP_PLAYER_STATUS_READY,
    MP_PLAYER_STATUS_IN_GAME,
    MP_PLAYER_STATUS_DISCONNECTED,
    MP_PLAYER_STATUS_DESYNCED
} mp_player_status;

typedef struct {
    int active;
    uint8_t player_id;
    char name[32];
    uint8_t peer_id;
    mp_player_status status;
    int empire_city_id;       /* City ID in the empire map */
    int scenario_city_slot;   /* Slot in the scenario/XML */
    int is_local;             /* 1 if this is the local player */
    int is_host;              /* 1 if this player is the host */
    uint32_t last_checksum;
    uint32_t last_checksum_tick;
} mp_player;

void mp_player_registry_init(void);
void mp_player_registry_clear(void);

int mp_player_registry_add(uint8_t player_id, const char *name, int is_local, int is_host);
void mp_player_registry_remove(uint8_t player_id);

mp_player *mp_player_registry_get(uint8_t player_id);
mp_player *mp_player_registry_get_local(void);
mp_player *mp_player_registry_get_host(void);
int mp_player_registry_get_count(void);

void mp_player_registry_set_status(uint8_t player_id, mp_player_status status);
void mp_player_registry_set_city(uint8_t player_id, int empire_city_id);
void mp_player_registry_set_slot(uint8_t player_id, int scenario_city_slot);

/* Iteration */
int mp_player_registry_get_first_active_id(void);
int mp_player_registry_get_next_active_id(int after_id);

/* Serialization for snapshots */
void mp_player_registry_serialize(uint8_t *buffer, uint32_t *size);
void mp_player_registry_deserialize(const uint8_t *buffer, uint32_t size);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_PLAYER_REGISTRY_H */
