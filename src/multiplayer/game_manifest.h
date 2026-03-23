#ifndef MULTIPLAYER_GAME_MANIFEST_H
#define MULTIPLAYER_GAME_MANIFEST_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Game manifest: formal description of the multiplayer session's game.
 * Created by the host when selecting a scenario/save. Sent to clients
 * in GAME_PREPARE so they can validate compatibility before loading.
 */

#define MP_MANIFEST_SCENARIO_NAME_MAX 64

typedef enum {
    MP_GAME_MODE_SCENARIO = 0, /* Fresh scenario start */
    MP_GAME_MODE_SAVED_GAME    /* Resume from save */
} mp_game_mode;

typedef struct {
    mp_game_mode mode;
    char scenario_name[MP_MANIFEST_SCENARIO_NAME_MAX]; /* Filename without extension */
    uint32_t map_hash;        /* CRC32 of the map data for compatibility */
    uint32_t scenario_hash;   /* CRC32 of the scenario data */
    uint32_t save_version;    /* Savegame format version (for compat check) */
    uint32_t session_seed;    /* Deterministic seed for worldgen */
    uint8_t max_players;      /* Capacity (2..8) */
    uint8_t player_count;     /* Currently joined players */
    uint32_t feature_flags;   /* Bitmask of enabled game features */
    int valid;                /* 1 if the manifest has been set */
} mp_game_manifest;

/* Initialize / clear the manifest */
void mp_game_manifest_init(void);
void mp_game_manifest_clear(void);

/* Host: set the manifest after scenario selection */
void mp_game_manifest_set(mp_game_mode mode, const char *scenario_name,
                          uint32_t map_hash, uint32_t scenario_hash,
                          uint32_t save_version, uint32_t session_seed,
                          uint8_t max_players);

/* Get the current manifest (const) */
const mp_game_manifest *mp_game_manifest_get(void);

/* Get mutable manifest (for client-side population from network) */
mp_game_manifest *mp_game_manifest_get_mutable(void);

/* Update player count */
void mp_game_manifest_set_player_count(uint8_t count);

/* Serialization for network transmission */
void mp_game_manifest_serialize(uint8_t *buffer, uint32_t *out_size);
int mp_game_manifest_deserialize(const uint8_t *buffer, uint32_t size);

/* Validate that local game data matches the manifest */
int mp_game_manifest_validate_local(void);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_GAME_MANIFEST_H */
