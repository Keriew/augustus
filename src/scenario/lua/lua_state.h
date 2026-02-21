#ifndef SCENARIO_LUA_STATE_H
#define SCENARIO_LUA_STATE_H

#include "core/buffer.h"
#include "lua/lua.h"

#define LUA_API_VERSION 1

/**
 * Load and execute a Lua script derived from the scenario filename.
 * Replaces the .map/.mapx extension with .lua and looks for it in the scenario directory.
 * Creates a new Lua state, registers all API bindings, and runs the script.
 * @param scenario_file The scenario filename (e.g. "Lugdunum.map")
 * @return 1 on success, 0 if no script or on error (errors are logged, not fatal)
 */
int scenario_lua_load_script(const char *scenario_file);

/**
 * Close and destroy the current Lua state.
 * Called during clear_scenario_data() or when loading a new scenario.
 */
void scenario_lua_shutdown(void);

/**
 * Returns 1 if a Lua state is active and a script has been loaded.
 */
int scenario_lua_is_active(void);

/**
 * Internal: get the Lua state for hook calls and API bindings.
 */
lua_State *scenario_lua_get_state(void);

/**
 * Ensure a Lua state exists. If no script was loaded, creates a bare state
 * with all API bindings registered so the terminal can execute commands.
 * @return 1 if a Lua state is available, 0 on failure
 */
int scenario_lua_ensure_state(void);

/**
 * Load and execute a Lua script from in-memory source (used when restoring from savegame).
 * Creates a new Lua state, registers all API bindings, and executes the source.
 * Also stores the source so it can be re-saved.
 * @return 1 on success, 0 on error
 */
int scenario_lua_load_script_from_source(const char *source, int length);

/**
 * Serialize the Lua script source and global variables to a buffer for savegame persistence.
 */
void scenario_lua_save_state(buffer *buf);

/**
 * Restore the Lua script and global variables from a savegame buffer.
 * Creates a new Lua state, executes the embedded script, and restores globals.
 */
void scenario_lua_load_state(buffer *buf);

#endif // SCENARIO_LUA_STATE_H
