#include "empire.h"

#include "city/message.h"
#include "empire/city.h"
#include "game/time.h"
#include "scenario/data.h"
#ifdef ENABLE_MULTIPLAYER
#include "multiplayer/player_registry.h"
#include "multiplayer/ownership.h"
#include "multiplayer/empire_sync.h"
#include "multiplayer/time_sync.h"
#include "multiplayer/command_bus.h"
#include "multiplayer/checksum.h"
#include "multiplayer/resync.h"
#include "network/session.h"
#include "core/log.h"
#endif

int scenario_empire_id(void)
{
    return scenario.empire.id;
}

int scenario_empire_is_expanded(void)
{
    return scenario.empire.is_expanded;
}

void scenario_empire_process_expansion(void)
{
    if (scenario.empire.is_expanded || scenario.empire.expansion_year <= 0) {
        return;
    }

    if (scenario_empire_id() == SCENARIO_CUSTOM_EMPIRE) {
        return;
    }

    if (game_time_year() <= scenario.empire.expansion_year + scenario.start_year) {
        return;
    }

    empire_city_expand_empire();

    scenario.empire.is_expanded = 1;
    city_message_post(1, MESSAGE_EMPIRE_HAS_EXPANDED, 0, 0);
}

#ifdef ENABLE_MULTIPLAYER

static int multiplayer_mode_enabled = 0;

int scenario_empire_is_multiplayer_mode(void)
{
    return multiplayer_mode_enabled && net_session_is_active();
}

void scenario_empire_set_multiplayer_mode(int enabled)
{
    multiplayer_mode_enabled = enabled;
}

void scenario_empire_multiplayer_init(void)
{
    if (!multiplayer_mode_enabled) {
        return;
    }

    /* The bootstrap module (mp_bootstrap_bind_loaded_scenario) now handles:
     * - Subsystem initialization (ownership, empire_sync, time_sync, etc.)
     * - City assignment from the spawn table
     *
     * This function is called from game_state_init() during scenario load.
     * In the new bootstrap flow, subsystems are already initialized by
     * mp_bootstrap_bind_loaded_scenario() AFTER the scenario loads,
     * so we only log here. The old init-and-assign logic is replaced
     * by the bootstrap pipeline to avoid destroying the lobby roster. */

    log_info("Multiplayer scenario mode active (bootstrap handles init)", 0, 0);
}

#endif /* ENABLE_MULTIPLAYER */
