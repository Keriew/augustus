#ifndef MULTIPLAYER_SNAPSHOT_H
#define MULTIPLAYER_SNAPSHOT_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Snapshot system for multiplayer state replication.
 * Snapshots are domain-separated to avoid monolithic packets.
 */

typedef enum {
    MP_SNAPSHOT_SESSION = 0,
    MP_SNAPSHOT_EMPIRE,
    MP_SNAPSHOT_CITY_TRADE_VIEW,
    MP_SNAPSHOT_TRADE_ROUTES,
    MP_SNAPSHOT_TRADER_ENTITIES,
    MP_SNAPSHOT_TIME,
    MP_SNAPSHOT_OWNERSHIP,
    MP_SNAPSHOT_PLAYER_REGISTRY,
    MP_SNAPSHOT_COUNT
} mp_snapshot_domain;

#define MP_SNAPSHOT_MAX_SIZE (256 * 1024)  /* 256KB max per full snapshot */

typedef struct {
    uint32_t tick;
    uint32_t checksum;
    uint32_t domain_offsets[MP_SNAPSHOT_COUNT];
    uint32_t domain_sizes[MP_SNAPSHOT_COUNT];
    uint32_t total_size;
} mp_snapshot_header;

/* Build a full snapshot from current game state (host only) */
int mp_snapshot_build_full(uint8_t *buffer, uint32_t buffer_size, uint32_t *out_size);

/* Apply a full snapshot to local state (client only) */
int mp_snapshot_apply_full(const uint8_t *buffer, uint32_t size);

/* Build a delta snapshot (changes since last snapshot) */
int mp_snapshot_build_delta(uint8_t *buffer, uint32_t buffer_size,
                            uint32_t *out_size, uint32_t since_tick);

/* Apply a delta snapshot */
int mp_snapshot_apply_delta(const uint8_t *buffer, uint32_t size);

/* Get the tick of the last built/applied snapshot */
uint32_t mp_snapshot_get_last_tick(void);

/* Called from session.c when full/delta snapshots arrive */
void multiplayer_snapshot_receive_full(const uint8_t *data, uint32_t size);
void multiplayer_snapshot_receive_delta(const uint8_t *data, uint32_t size);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_SNAPSHOT_H */
