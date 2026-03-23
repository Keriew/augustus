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
    if (!net_session_is_active()) {
        return;
    }

    /* Process network I/O every frame — this is the critical fix.
     * In lobby/connect windows, simulation ticks don't run, but
     * net_session_update() must still be called so that:
     *   - Host accepts new TCP connections
     *   - Host/client process incoming packets (HELLO, JOIN_ACCEPT, etc.)
     *   - Heartbeats are sent/received
     *   - Timeout detection works
     *   - DISCONNECTING state transitions to IDLE
     */
    net_session_update();

    /* Update LAN discovery: send announcements (host) and receive them (client).
     * This runs globally rather than being tied to a specific window. */
    net_discovery_update();

    /* Host: keep the announced player count in sync with reality */
    if (net_session_is_host()) {
        int peer_count = net_session_get_peer_count();
        net_discovery_update_announcing((uint8_t)(peer_count + 1)); /* +1 for host */
    }
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
