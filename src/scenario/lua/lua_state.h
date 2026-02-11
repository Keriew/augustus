#ifndef SCENARIO_LUA_STATE_H
#define SCENARIO_LUA_STATE_H

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

#endif // SCENARIO_LUA_STATE_H
