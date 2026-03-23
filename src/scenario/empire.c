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

    /* Initialize all multiplayer subsystems */
    mp_player_registry_init();
    mp_ownership_init();
    mp_empire_sync_init();
    mp_time_sync_init();
    mp_command_bus_init();
    mp_checksum_init();
    mp_resync_init();

    /* Register the local player */
    uint8_t local_id = net_session_get_local_player_id();
    mp_player_registry_add(local_id,
                           net_session_get()->local_player_name,
                           1, /* is_local */
                           net_session_is_host());

    /* Find and register player cities based on empire XML multiplayer slots.
       Cities with replicated_flags containing valid slot data are player slots. */
    int array_size = empire_city_get_array_size();
    for (int i = 0; i < array_size; i++) {
        empire_city *city = empire_city_get(i);
        if (!city || !city->in_use) {
            continue;
        }

        int slot = city->replicated_flags & 0xFF;
        if (slot == 0) {
            continue; /* Not a multiplayer slot */
        }

        int is_host_start = (city->replicated_flags & 0x100) != 0;

        if (net_session_is_host() && is_host_start) {
            /* This is the host's city */
            empire_city_set_owner(i, CITY_OWNER_LOCAL, local_id);
            mp_ownership_set_city(i, MP_OWNER_LOCAL_PLAYER, local_id);
            mp_player_registry_set_city(local_id, i);
            log_info("Host city assigned", 0, i);
        } else if (!net_session_is_host() && slot == local_id) {
            /* This is our city as a client */
            empire_city_set_owner(i, CITY_OWNER_LOCAL, local_id);
            mp_ownership_set_city(i, MP_OWNER_LOCAL_PLAYER, local_id);
            mp_player_registry_set_city(local_id, i);
        }
    }

    log_info("Multiplayer scenario initialized", 0, 0);
}

#endif /* ENABLE_MULTIPLAYER */
