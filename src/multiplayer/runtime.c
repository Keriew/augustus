#include "runtime.h"

#ifdef ENABLE_MULTIPLAYER

#include "bootstrap.h"
#include "mp_autosave.h"
#include "mp_debug_log.h"
#include "network/session.h"
#include "network/discovery_lan.h"
#include "scenario/empire.h"
#include "core/log.h"
#include "window/main_menu.h"

static int runtime_initialized;
static int was_in_game;

void multiplayer_runtime_init(void)
{
    if (runtime_initialized) {
        return;
    }
    runtime_initialized = 1;
    was_in_game = 0;
    MP_LOG_INFO("SESSION", "Multiplayer runtime initialized (per-frame processing enabled)");
}

void multiplayer_runtime_update(void)
{
    /* IMPORTANT: discovery and session are updated independently.
     *
     * Discovery must run even when there is no active session, because:
     * - The connect window opens the listener to find LAN hosts BEFORE joining
     * - The host announces BEFORE a client has connected
     *
     * Session update only runs when there is an active session (host or client).
     * Discovery update always runs — it self-checks whether its socket is open. */

    int was_active = net_session_is_active();
    int was_playing = was_in_game;

    /* 1. Session I/O (TCP handshake, data, heartbeats, timeouts) */
    if (was_active) {
        net_session_update();

        /* Host: keep the announced player count in sync with reality */
        if (net_session_is_host()) {
            int peer_count = net_session_get_peer_count();
            net_discovery_update_announcing((uint8_t)(peer_count + 1)); /* +1 for host */
        }
    }

    /* Track in-game state for disconnect detection */
    if (net_session_is_in_game()) {
        was_in_game = 1;
    }

    /* Detect session drop while in-game: gracefully return to main menu */
    if (was_playing && !net_session_is_active()) {
        MP_LOG_INFO("SESSION", "Session lost while in-game — returning to main menu");

        /* Attempt final save before cleaning up (host only) */
        if (scenario_empire_is_multiplayer_mode()) {
            mp_autosave_final_save();
        }

        mp_bootstrap_reset();
        was_in_game = 0;
        window_main_menu_show(1);
    }

    /* 2. Discovery I/O (UDP announce/listen) — runs unconditionally.
     * net_discovery_update() is a no-op if its socket is not open. */
    net_discovery_update();

    /* 3. Autosave timer — runs per-frame, only meaningful on host in-game.
     * mp_autosave_update() checks all preconditions internally. */
    mp_autosave_update();
}

void multiplayer_runtime_shutdown(void)
{
    if (!runtime_initialized) {
        return;
    }

    /* Perform final save on clean shutdown (host only) */
    if (net_session_is_active() && net_session_is_host() &&
        scenario_empire_is_multiplayer_mode()) {
        MP_LOG_INFO("SESSION", "Performing final save before shutdown");
        mp_autosave_final_save();
    }

    mp_bootstrap_reset();
    was_in_game = 0;
    runtime_initialized = 0;
    MP_LOG_INFO("SESSION", "Multiplayer runtime shutdown");
}

#endif /* ENABLE_MULTIPLAYER */
