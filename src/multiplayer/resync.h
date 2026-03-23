#ifndef MULTIPLAYER_RESYNC_H
#define MULTIPLAYER_RESYNC_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Resync system: when a client detects or is told it's out of sync,
 * it can request a full snapshot from the host to recover.
 */

void mp_resync_init(void);

/* Client: request a full resync from host */
void mp_resync_request_full_snapshot(void);

/* Host: handle a resync request from a client */
void multiplayer_resync_handle_request(uint8_t player_id);

/* Client: apply a full resync snapshot */
void multiplayer_resync_apply_full_snapshot(const uint8_t *data, uint32_t size);

/* Query resync state */
int mp_resync_is_in_progress(void);
int mp_resync_get_attempt_count(void);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_RESYNC_H */
