#ifndef MULTIPLAYER_CHECKSUM_H
#define MULTIPLAYER_CHECKSUM_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Checksum system for desync detection.
 * Periodically hashes replicated state and compares between host and clients.
 */

void mp_checksum_init(void);

/**
 * Compute checksum of current replicated state.
 * Hashes: tick, empire city ownership, route limits, route traded amounts,
 *         trade open/closed, active traders, city trade views.
 */
uint32_t mp_checksum_compute(void);

/**
 * Host: request checksums from all clients at given tick.
 */
void mp_checksum_request_from_clients(uint32_t tick);

/**
 * Host: called when a client sends a checksum response.
 */
void multiplayer_checksum_receive_response(uint8_t player_id,
                                            const uint8_t *data, uint32_t size);

/**
 * Client: called when host requests a checksum.
 */
void multiplayer_checksum_handle_request(const uint8_t *data, uint32_t size);

/**
 * Check if should request checksums this tick.
 */
int mp_checksum_should_check(uint32_t current_tick);

/**
 * Get desync info for UI.
 */
int mp_checksum_has_desync(void);
uint8_t mp_checksum_desynced_player(void);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_CHECKSUM_H */
