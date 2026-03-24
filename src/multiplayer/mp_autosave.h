#ifndef MULTIPLAYER_MP_AUTOSAVE_H
#define MULTIPLAYER_MP_AUTOSAVE_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Multiplayer autosave system.
 *
 * Runs ONLY on the host. Based on real elapsed time (not game ticks).
 *
 * Features:
 *   - Configurable interval (default 300 seconds / 5 minutes)
 *   - Dirty flag: only saves when session state has changed
 *   - Rotating autosave slots (default 3)
 *   - Final save on normal host shutdown
 *   - Manual save support
 *   - Save barrier: blocks new commands briefly during save
 *
 * Save file layout:
 *   mp_session_current.sav   - Latest manual/final save
 *   mp_session_previous.sav  - Previous version of current save
 *   mp_autosave_00.sav       - Autosave slot 0
 *   mp_autosave_01.sav       - Autosave slot 1
 *   mp_autosave_02.sav       - Autosave slot 2
 */

#define MP_AUTOSAVE_DEFAULT_INTERVAL    300   /* seconds */
#define MP_AUTOSAVE_MAX_SLOTS           5
#define MP_AUTOSAVE_DEFAULT_SLOTS       3

#define MP_SAVE_CURRENT_FILENAME    "mp_session_current.sav"
#define MP_SAVE_PREVIOUS_FILENAME   "mp_session_previous.sav"

/* Reasons the session dirty flag gets set */
typedef enum {
    MP_DIRTY_CONSTRUCTION     = 0x0001,
    MP_DIRTY_DEMOLITION       = 0x0002,
    MP_DIRTY_STOCK_CHANGE     = 0x0004,
    MP_DIRTY_TRADE_EXECUTED   = 0x0008,
    MP_DIRTY_ROUTE_CHANGED    = 0x0010,
    MP_DIRTY_POLICY_CHANGED   = 0x0020,
    MP_DIRTY_OWNERSHIP_CHANGE = 0x0040,
    MP_DIRTY_TIME_ADVANCE     = 0x0080,
    MP_DIRTY_PLAYER_JOIN      = 0x0100,
    MP_DIRTY_PLAYER_LEAVE     = 0x0200,
    MP_DIRTY_RECONNECT        = 0x0400,
    MP_DIRTY_CITY_ASSIGNED    = 0x0800,
    MP_DIRTY_WORLD_EVENT      = 0x1000
} mp_dirty_reason;

/**
 * Initialize the autosave system. Call once when session starts.
 */
void mp_autosave_init(void);

/**
 * Reset the autosave system. Call on session end.
 */
void mp_autosave_reset(void);

/**
 * Mark the session as dirty (something changed that needs saving).
 * Can be called from any subsystem. Only meaningful on host.
 */
void mp_autosave_mark_dirty(uint32_t reason);

/**
 * Check if the session is dirty (has unsaved changes).
 */
int mp_autosave_is_dirty(void);

/**
 * Update the autosave timer. Called once per frame from runtime.
 * Triggers autosave when interval has elapsed and session is dirty.
 * Only runs on host, only when in-game.
 */
void mp_autosave_update(void);

/**
 * Perform a manual save of the current multiplayer session.
 * Uses atomic write to mp_session_current.sav with backup.
 * Only the host can call this.
 *
 * @return 1 on success, 0 on failure
 */
int mp_autosave_manual_save(void);

/**
 * Perform the final save before host shutdown.
 * Synchronous, blocks until complete.
 *
 * @return 1 on success, 0 on failure
 */
int mp_autosave_final_save(void);

/**
 * Check if a save operation is currently in progress.
 */
int mp_autosave_is_save_in_progress(void);

/**
 * Configure autosave settings.
 */
void mp_autosave_set_interval(int seconds);
void mp_autosave_set_max_slots(int slots);
void mp_autosave_set_enabled(int enabled);

/**
 * Get autosave status info for UI display.
 */
int mp_autosave_get_interval(void);
int mp_autosave_get_slot_count(void);
uint32_t mp_autosave_get_last_save_tick(void);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_MP_AUTOSAVE_H */
