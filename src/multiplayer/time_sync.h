#ifndef MULTIPLAYER_TIME_SYNC_H
#define MULTIPLAYER_TIME_SYNC_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Multiplayer time synchronization.
 * The host controls the authoritative tick. Clients follow.
 */

void mp_time_sync_init(void);

/* Check if the simulation can advance a tick */
int mp_time_sync_can_advance_tick(void);

/* Called after a tick is advanced (host only) */
void mp_time_sync_on_tick_advanced(void);

/* Get the authoritative tick from the host */
uint32_t mp_time_sync_get_authoritative_tick(void);

/* Set the local confirmed tick (client updates this from host messages) */
void mp_time_sync_set_confirmed_tick(uint32_t tick);

/* Speed/pause control */
int mp_time_sync_is_paused(void);
uint8_t mp_time_sync_get_speed(void);

/* Serialization */
void mp_time_sync_serialize(uint8_t *buffer, uint32_t *size);
void mp_time_sync_deserialize(const uint8_t *buffer, uint32_t size);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_TIME_SYNC_H */
