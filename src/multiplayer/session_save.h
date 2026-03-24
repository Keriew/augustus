#ifndef MULTIPLAYER_SESSION_SAVE_H
#define MULTIPLAYER_SESSION_SAVE_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Multiplayer session save/load.
 * Only the host creates authoritative saves.
 * Saves include multiplayer metadata alongside the normal save data.
 *
 * Saved domains:
 *   - Session metadata (seed, protocol, checksums)
 *   - Player registry (UUIDs, slots, reconnect tokens)
 *   - Ownership tables (cities, routes with lifecycle, traders)
 *   - Spawn table (worldgen results)
 *   - Empire sync (trade views)
 *   - Trade sync (route state, trader replicas)
 *   - Time sync (authoritative tick)
 */

#define MP_SAVE_MAGIC       0x4D504C59  /* "MPLY" */
#define MP_SAVE_VERSION     3

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t protocol_version;
    uint32_t session_id;
    uint32_t session_seed;
    uint8_t host_player_id;
    uint8_t player_count;
    uint32_t snapshot_tick;
    uint32_t checksum;
    /* Domain sizes */
    uint32_t player_registry_size;
    uint32_t ownership_size;
    uint32_t worldgen_size;
    uint32_t empire_sync_size;
    uint32_t trade_sync_routes_size;
    uint32_t trade_sync_traders_size;
    uint32_t time_sync_size;
    uint32_t next_command_sequence_id;
} mp_save_header;

/* Save multiplayer metadata to buffer */
int mp_session_save_to_buffer(uint8_t *buffer, uint32_t buffer_size, uint32_t *out_size);

/* Load multiplayer metadata from buffer */
int mp_session_load_from_buffer(const uint8_t *buffer, uint32_t size);

/* Check if a save file contains multiplayer data */
int mp_session_save_is_multiplayer(const uint8_t *buffer, uint32_t size);

/* Get save header for display */
int mp_session_save_read_header(const uint8_t *buffer, uint32_t size, mp_save_header *header);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_SESSION_SAVE_H */
