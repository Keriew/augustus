#ifndef MULTIPLAYER_TIME_SYNC_H
#define MULTIPLAYER_TIME_SYNC_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Multiplayer time synchronization.
 * The host controls the authoritative tick. Clients follow.
 */

void mp_time_sync_init(void);

/**
 * Initialize time sync from a saved session.
 * Preserves the authoritative tick from the save instead of resetting to zero.
 * Call after mp_time_sync_deserialize() to ensure ticks are unified.
 */
void mp_time_sync_init_from_save(void);

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
void mp_time_sync_set_paused(int paused);
void mp_time_sync_set_speed(uint8_t speed);

/* Serialization */
void mp_time_sync_serialize(uint8_t *buffer, uint32_t *size);
void mp_time_sync_deserialize(const uint8_t *buffer, uint32_t size);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_TIME_SYNC_H */
