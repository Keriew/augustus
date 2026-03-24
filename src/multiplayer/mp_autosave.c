#include "mp_autosave.h"

#ifdef ENABLE_MULTIPLAYER

#include "mp_safe_file.h"
#include "mp_debug_log.h"
#include "session_save.h"
#include "time_sync.h"
#include "network/session.h"
#include "core/log.h"
#include "core/dir.h"
#include "game/file.h"
#include "scenario/empire.h"
#include "platform/file_manager.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define MP_SESSION_SAVE_BUFFER_SIZE 262144  /* 256KB */

static struct {
    int initialized;
    int enabled;
    int interval_seconds;
    int max_slots;
    int current_slot;

    uint32_t dirty_flags;
    int save_in_progress;

    time_t last_save_time;
    uint32_t last_save_tick;
    uint32_t total_saves;
    uint32_t total_autosaves;
} autosave;

void mp_autosave_init(void)
{
    memset(&autosave, 0, sizeof(autosave));
    autosave.enabled = 1;
    autosave.interval_seconds = MP_AUTOSAVE_DEFAULT_INTERVAL;
    autosave.max_slots = MP_AUTOSAVE_DEFAULT_SLOTS;
    autosave.current_slot = 0;
    autosave.last_save_time = time(NULL);
    autosave.initialized = 1;

    MP_LOG_INFO("AUTOSAVE", "Autosave system initialized (interval=%ds, slots=%d)",
                autosave.interval_seconds, autosave.max_slots);
}

void mp_autosave_reset(void)
{
    memset(&autosave, 0, sizeof(autosave));
    MP_LOG_INFO("AUTOSAVE", "Autosave system reset");
}

void mp_autosave_mark_dirty(uint32_t reason)
{
    if (!net_session_is_host()) {
        return;
    }
    autosave.dirty_flags |= reason;
}

int mp_autosave_is_dirty(void)
{
    return autosave.dirty_flags != 0;
}

static int perform_save_to_buffer(uint8_t *buffer, uint32_t buffer_size, uint32_t *out_size)
{
    if (!net_session_is_host()) {
        log_error("mp_autosave: only host can save", 0, 0);
        return 0;
    }

    /* Use the game's normal save path which includes both base game and MP data */
    const char *save_path = mp_safe_file_get_save_path(MP_SAVE_CURRENT_FILENAME);
    if (!save_path) {
        log_error("mp_autosave: cannot resolve save path", 0, 0);
        return 0;
    }

    /* Perform a full game save using the engine's save system */
    int result = game_file_write_saved_game(save_path);
    if (result) {
        *out_size = 1; /* Signal success; actual size managed by game_file */
    }
    return result;
}

static int do_full_save(const char *target, const char *backup)
{
    if (!net_session_is_host()) {
        return 0;
    }

    if (autosave.save_in_progress) {
        MP_LOG_WARN("AUTOSAVE", "Save already in progress, skipping");
        return 0;
    }

    autosave.save_in_progress = 1;

    /* The game's save system writes the complete game state (base + MP).
     * We use the standard game_file_write_saved_game which internally calls
     * mp_session_save_to_buffer for the MP portion. */
    const char *save_path = target ? target :
        mp_safe_file_get_save_path(MP_SAVE_CURRENT_FILENAME);

    if (!save_path) {
        autosave.save_in_progress = 0;
        return 0;
    }

    /* For atomic write, we write to .tmp, validate, then rename */
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", save_path);

    int result = game_file_write_saved_game(tmp_path);
    if (!result) {
        MP_LOG_ERROR("AUTOSAVE", "Failed to write save to tmp file");
        platform_file_manager_remove_file(tmp_path);
        autosave.save_in_progress = 0;
        return 0;
    }

    /* Move existing save to backup if requested */
    if (backup) {
        FILE *existing = platform_file_manager_open_file(save_path, "rb");
        if (existing) {
            fclose(existing);
            platform_file_manager_remove_file(backup);
            rename(save_path, backup);
        }
    }

    /* Atomic rename: tmp -> target */
    platform_file_manager_remove_file(save_path);
    if (rename(tmp_path, save_path) != 0) {
        MP_LOG_ERROR("AUTOSAVE", "Failed to rename tmp to final save");
        autosave.save_in_progress = 0;
        return 0;
    }

    autosave.dirty_flags = 0;
    autosave.last_save_time = time(NULL);
    autosave.last_save_tick = mp_time_sync_get_authoritative_tick();
    autosave.total_saves++;
    autosave.save_in_progress = 0;

    MP_LOG_INFO("AUTOSAVE", "Save completed successfully (tick=%u, total_saves=%u)",
                autosave.last_save_tick, autosave.total_saves);
    return 1;
}

static int do_autosave(void)
{
    if (!net_session_is_host()) {
        return 0;
    }

    if (autosave.save_in_progress) {
        return 0;
    }

    autosave.save_in_progress = 1;

    /* Build the autosave filename for current slot */
    char filename[64];
    snprintf(filename, sizeof(filename), "mp_autosave_%02d.sav", autosave.current_slot);

    const char *save_path = mp_safe_file_get_save_path(filename);
    if (!save_path) {
        autosave.save_in_progress = 0;
        return 0;
    }

    /* Write to tmp, then rename for atomicity */
    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", save_path);

    int result = game_file_write_saved_game(tmp_path);
    if (!result) {
        MP_LOG_ERROR("AUTOSAVE", "Failed to write autosave slot %d", autosave.current_slot);
        platform_file_manager_remove_file(tmp_path);
        autosave.save_in_progress = 0;
        return 0;
    }

    /* Atomic rename */
    platform_file_manager_remove_file(save_path);
    if (rename(tmp_path, save_path) != 0) {
        MP_LOG_ERROR("AUTOSAVE", "Failed to rename autosave tmp");
        autosave.save_in_progress = 0;
        return 0;
    }

    autosave.dirty_flags = 0;
    autosave.last_save_time = time(NULL);
    autosave.last_save_tick = mp_time_sync_get_authoritative_tick();
    autosave.total_autosaves++;

    MP_LOG_INFO("AUTOSAVE", "Autosave slot %d completed (tick=%u, total=%u)",
                autosave.current_slot, autosave.last_save_tick, autosave.total_autosaves);

    /* Rotate to next slot */
    autosave.current_slot = (autosave.current_slot + 1) % autosave.max_slots;

    autosave.save_in_progress = 0;
    return 1;
}

void mp_autosave_update(void)
{
    if (!autosave.initialized || !autosave.enabled) {
        return;
    }

    /* Only host saves */
    if (!net_session_is_host()) {
        return;
    }

    /* Only save when in-game */
    if (!net_session_is_in_game()) {
        return;
    }

    /* Don't save if another save is in progress */
    if (autosave.save_in_progress) {
        return;
    }

    /* Check if enough real time has elapsed */
    time_t now = time(NULL);
    double elapsed = difftime(now, autosave.last_save_time);
    if (elapsed < (double)autosave.interval_seconds) {
        return;
    }

    /* Only save if session is dirty */
    if (!mp_autosave_is_dirty()) {
        /* Reset timer even if not dirty to avoid checking too frequently */
        autosave.last_save_time = now;
        return;
    }

    /* Perform autosave */
    MP_LOG_INFO("AUTOSAVE", "Triggering autosave (elapsed=%.0fs, dirty=0x%04x)",
                elapsed, autosave.dirty_flags);
    do_autosave();
}

int mp_autosave_manual_save(void)
{
    if (!net_session_is_host()) {
        log_error("mp_autosave: only host can manual save", 0, 0);
        return 0;
    }

    if (!scenario_empire_is_multiplayer_mode()) {
        return 0;
    }

    /* Copy paths because mp_safe_file_get_save_path uses a static buffer */
    char target[512], backup[512];
    const char *t = mp_safe_file_get_save_path(MP_SAVE_CURRENT_FILENAME);
    if (!t) return 0;
    strncpy(target, t, sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';

    const char *b = mp_safe_file_get_save_path(MP_SAVE_PREVIOUS_FILENAME);
    if (!b) return 0;
    strncpy(backup, b, sizeof(backup) - 1);
    backup[sizeof(backup) - 1] = '\0';

    MP_LOG_INFO("AUTOSAVE", "Manual save requested");
    return do_full_save(target, backup);
}

int mp_autosave_final_save(void)
{
    if (!net_session_is_host()) {
        return 0;
    }

    if (!scenario_empire_is_multiplayer_mode()) {
        return 0;
    }

    MP_LOG_INFO("AUTOSAVE", "Final save on shutdown requested");

    /* Copy paths because mp_safe_file_get_save_path uses a static buffer */
    char target[512], backup[512];
    const char *t = mp_safe_file_get_save_path(MP_SAVE_CURRENT_FILENAME);
    if (!t) return 0;
    strncpy(target, t, sizeof(target) - 1);
    target[sizeof(target) - 1] = '\0';

    const char *b = mp_safe_file_get_save_path(MP_SAVE_PREVIOUS_FILENAME);
    if (!b) return 0;
    strncpy(backup, b, sizeof(backup) - 1);
    backup[sizeof(backup) - 1] = '\0';

    int result = do_full_save(target, backup);
    if (!result) {
        MP_LOG_ERROR("AUTOSAVE", "Final save FAILED — last autosave may still be valid");
    }
    return result;
}

int mp_autosave_is_save_in_progress(void)
{
    return autosave.save_in_progress;
}

void mp_autosave_set_interval(int seconds)
{
    if (seconds < 30) {
        seconds = 30;
    }
    if (seconds > 3600) {
        seconds = 3600;
    }
    autosave.interval_seconds = seconds;
}

void mp_autosave_set_max_slots(int slots)
{
    if (slots < 1) {
        slots = 1;
    }
    if (slots > MP_AUTOSAVE_MAX_SLOTS) {
        slots = MP_AUTOSAVE_MAX_SLOTS;
    }
    autosave.max_slots = slots;
    if (autosave.current_slot >= slots) {
        autosave.current_slot = 0;
    }
}

void mp_autosave_set_enabled(int enabled)
{
    autosave.enabled = enabled ? 1 : 0;
}

int mp_autosave_get_interval(void)
{
    return autosave.interval_seconds;
}

int mp_autosave_get_slot_count(void)
{
    return autosave.max_slots;
}

uint32_t mp_autosave_get_last_save_tick(void)
{
    return autosave.last_save_tick;
}

#endif /* ENABLE_MULTIPLAYER */
