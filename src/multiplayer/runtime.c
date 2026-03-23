#include "runtime.h"

#ifdef ENABLE_MULTIPLAYER

#include "mp_debug_log.h"
#include "network/session.h"
#include "network/discovery_lan.h"
#include "core/log.h"

static int runtime_initialized;

void multiplayer_runtime_init(void)
{
    if (runtime_initialized) {
        return;
    }
    runtime_initialized = 1;
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

    /* 1. Session I/O (TCP handshake, data, heartbeats, timeouts) */
    if (net_session_is_active()) {
        net_session_update();

        /* Host: keep the announced player count in sync with reality */
        if (net_session_is_host()) {
            int peer_count = net_session_get_peer_count();
            net_discovery_update_announcing((uint8_t)(peer_count + 1)); /* +1 for host */
        }
    }

    /* 2. Discovery I/O (UDP announce/listen) — runs unconditionally.
     * net_discovery_update() is a no-op if its socket is not open. */
    net_discovery_update();
}

void multiplayer_runtime_shutdown(void)
{
    if (!runtime_initialized) {
        return;
    }
    runtime_initialized = 0;
    MP_LOG_INFO("SESSION", "Multiplayer runtime shutdown");
}

#endif /* ENABLE_MULTIPLAYER */
