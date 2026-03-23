#ifndef MULTIPLAYER_WORLDGEN_H
#define MULTIPLAYER_WORLDGEN_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Deterministic world generation for multiplayer player spawns.
 *
 * The host generates spawn positions using a session seed. The spawn_table
 * is persisted in save files and replicated to clients in snapshots.
 *
 * Design constraints:
 * - Same seed + same player count + same empire = same spawn positions
 * - Minimum distance between player cities enforced
 * - Minimum distance from AI cities enforced
 * - Connectivity guarantee: each player city has at least one valid trade route possibility
 * - Fairness: balanced land/sea access, balanced route opening costs
 * - If XML defines explicit multiplayer slots, those are used as hard constraints
 */

#define MP_WORLDGEN_MAX_SPAWNS   8
#define MP_WORLDGEN_MAX_CANDIDATES 64

typedef struct {
    int valid;
    uint8_t slot_id;            /* Player slot (0..MP_MAX_PLAYERS-1) */
    int empire_city_id;         /* Assigned empire_city index */
    int empire_object_id;       /* Corresponding empire_object */
    int x;                      /* Empire map X position */
    int y;                      /* Empire map Y position */
    int is_sea_trade;           /* Has sea trade access */
    int is_land_trade;          /* Has land trade access */
    int nearest_ai_distance;    /* Distance to nearest AI trade city */
    int nearest_player_distance;/* Distance to nearest other player spawn */
    int fairness_score;         /* Lower is more fair (0 = perfect) */
} mp_spawn_entry;

typedef struct {
    uint32_t session_seed;
    uint8_t player_count;
    uint8_t spawn_count;        /* May be <= player_count if slots were constrained */
    mp_spawn_entry spawns[MP_WORLDGEN_MAX_SPAWNS];
    uint32_t generation_tick;   /* Tick when this table was generated */
    int locked;                 /* 1 after game start, no rerolls allowed */
} mp_spawn_table;

/**
 * Generate player spawn positions on the empire map.
 * Host-only. The result is deterministic for a given seed and empire state.
 *
 * @param session_seed   Seed for deterministic generation
 * @param player_count   Number of player slots to generate (2..8)
 * @param use_xml_slots  1 = prefer XML <multiplayer> slot definitions as hard constraints
 * @return 1 on success, 0 if no valid configuration found
 */
int mp_worldgen_generate_player_spawns(uint32_t session_seed, int player_count,
                                        int use_xml_slots);

/**
 * Get the current spawn table (may be empty if not yet generated).
 */
const mp_spawn_table *mp_worldgen_get_spawn_table(void);

/**
 * Get a mutable spawn table pointer (for session seed assignment on client).
 */
mp_spawn_table *mp_worldgen_get_spawn_table_mutable(void);

/**
 * Reroll spawn positions with a new seed. Only allowed before game start (while unlocked).
 */
int mp_worldgen_reroll(uint32_t new_seed, int player_count);

/**
 * Lock the spawn table. Called when the game starts. No more rerolls after this.
 */
void mp_worldgen_lock(void);

/**
 * Get spawn entry for a specific slot.
 */
const mp_spawn_entry *mp_worldgen_get_spawn_for_slot(uint8_t slot_id);

/**
 * Apply spawn table to empire: create/configure empire cities for player slots.
 * Host calls this once after generation and lock.
 */
int mp_worldgen_apply_spawns(void);

/**
 * Serialization for save/load and snapshot replication.
 */
void mp_worldgen_serialize(uint8_t *buffer, uint32_t *size);
void mp_worldgen_deserialize(const uint8_t *buffer, uint32_t size);

/**
 * Init/clear worldgen state.
 */
void mp_worldgen_init(void);
void mp_worldgen_clear(void);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_WORLDGEN_H */
