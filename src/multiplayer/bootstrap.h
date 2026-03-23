#ifndef MULTIPLAYER_BOOTSTRAP_H
#define MULTIPLAYER_BOOTSTRAP_H

#ifdef ENABLE_MULTIPLAYER

#include <stdint.h>

/**
 * Multiplayer bootstrap pipeline.
 *
 * Orchestrates the critical path from lobby to gameplay:
 *
 * HOST flow:
 *   1. Host selects scenario in host_setup window
 *   2. mp_bootstrap_set_scenario() records the selection
 *   3. Host clicks Start in lobby → mp_bootstrap_host_start_game()
 *   4. Sends GAME_PREPARE (manifest) to all peers
 *   5. Host loads scenario locally
 *   6. mp_bootstrap_bind_loaded_scenario() initializes MP subsystems
 *   7. Worldgen runs, spawns locked, ownership applied
 *   8. Host broadcasts spawn table
 *   9. Host sends GAME_START_FINAL
 *  10. Transition to WINDOW_CITY
 *
 * CLIENT flow:
 *   1. Receives GAME_PREPARE → stores manifest
 *   2. mp_bootstrap_client_prepare() loads the same scenario locally
 *   3. mp_bootstrap_bind_loaded_scenario() initializes MP subsystems
 *   4. Receives spawn table via HOST_EVENT
 *   5. Receives GAME_START_FINAL → mp_bootstrap_client_enter_game()
 *   6. Transition to WINDOW_CITY
 */

typedef enum {
    MP_BOOT_IDLE = 0,
    MP_BOOT_SCENARIO_SELECTED,  /* Host has chosen a scenario */
    MP_BOOT_PREPARING,          /* GAME_PREPARE sent, loading locally */
    MP_BOOT_LOADED,             /* Scenario loaded, subsystems bound */
    MP_BOOT_IN_GAME             /* Playing */
} mp_boot_state;

/* Initialize bootstrap state (call once on session create) */
void mp_bootstrap_init(void);

/* Reset bootstrap state (call on disconnect) */
void mp_bootstrap_reset(void);

/* Get current bootstrap state */
mp_boot_state mp_bootstrap_get_state(void);

/**
 * HOST: Record the scenario the host has selected.
 * Called from the host setup screen after file selection.
 * @param scenario_name  Scenario filename (without extension)
 */
void mp_bootstrap_set_scenario(const char *scenario_name);

/**
 * HOST: Get the selected scenario name.
 */
const char *mp_bootstrap_get_scenario_name(void);

/**
 * HOST: Initiate the game start sequence.
 * - Sets the manifest with current session info
 * - Sends GAME_PREPARE to all peers
 * - Loads the scenario locally
 * - Binds MP subsystems
 * - Generates and locks worldgen
 * - Broadcasts spawn table
 * - Sends GAME_START_FINAL
 * - Transitions to WINDOW_CITY
 * @return 1 on success, 0 on failure
 */
int mp_bootstrap_host_start_game(void);

/**
 * CLIENT: Handle GAME_PREPARE message from host.
 * - Deserializes manifest
 * - Loads the same scenario locally
 * - Binds MP subsystems
 * @param payload  Serialized manifest data
 * @param size     Size of payload
 * @return 1 on success, 0 on failure
 */
int mp_bootstrap_client_prepare(const uint8_t *payload, uint32_t size);

/**
 * CLIENT: Handle GAME_START_FINAL message from host.
 * - Transitions session state to in-game
 * - Transitions to WINDOW_CITY
 * @return 1 on success, 0 on failure
 */
int mp_bootstrap_client_enter_game(void);

/**
 * COMMON: Bind multiplayer subsystems to the currently loaded scenario.
 * Called AFTER the scenario is loaded via game_file_start_scenario_by_name().
 * Does NOT reinitialize the player registry (preserves lobby roster).
 *
 * Steps:
 *   1. Enable multiplayer mode
 *   2. Init ownership, empire_sync, time_sync, command_bus, checksum, resync
 *   3. Assign empire cities to players based on spawn table
 */
void mp_bootstrap_bind_loaded_scenario(void);

#endif /* ENABLE_MULTIPLAYER */

#endif /* MULTIPLAYER_BOOTSTRAP_H */
