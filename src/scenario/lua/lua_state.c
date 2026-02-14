#include "scenario/lua/lua_state.h"

#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "lua/lualib.h"

#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"

#include <string.h>

// Forward declarations for API registration
void lua_api_game_register(lua_State *L);
void lua_api_city_register(lua_State *L);
void lua_api_finance_register(lua_State *L);
void lua_api_scenario_register(lua_State *L);
void lua_api_map_register(lua_State *L);
void lua_api_ui_register(lua_State *L);
void lua_api_building_register(lua_State *L);
void lua_api_resource_register(lua_State *L);
void lua_api_population_register(lua_State *L);
void lua_api_sound_register(lua_State *L);
void lua_api_empire_register(lua_State *L);
// Forward declaration for clearing custom handlers
void lua_api_scenario_clear_handlers(void);

static lua_State *global_lua_state = 0;

static void register_all_apis(lua_State *L)
{
    lua_api_game_register(L);
    lua_api_city_register(L);
    lua_api_finance_register(L);
    lua_api_scenario_register(L);
    lua_api_map_register(L);
    lua_api_ui_register(L);
    lua_api_building_register(L);
    lua_api_resource_register(L);
    lua_api_population_register(L);
    lua_api_sound_register(L);
    lua_api_empire_register(L);
}

int scenario_lua_load_script(const char *scenario_file)
{
    scenario_lua_shutdown();

    if (!scenario_file || !*scenario_file) {
        return 0;
    }

    // Derive Lua script name from scenario filename: "map_name.map" -> "map_name.lua"
    char lua_filename[FILE_NAME_MAX];
    strncpy(lua_filename, scenario_file, FILE_NAME_MAX - 1);
    lua_filename[FILE_NAME_MAX - 1] = '\0';
    file_remove_extension(lua_filename);
    file_append_extension(lua_filename, "lua", FILE_NAME_MAX);

    const char *full_path = dir_get_file_at_location(lua_filename, PATH_LOCATION_SCENARIO);
    if (!full_path) {
        // No .lua file for this scenario - that's normal, not an error
        log_info("No Lua script found for scenario:", lua_filename, 0);
        return 0;
    }
    log_info("Lua script found:", full_path, 0);

    global_lua_state = luaL_newstate();
    if (!global_lua_state) {
        log_error("Failed to create Lua state", 0, 0);
        return 0;
    }

    // Open safe standard libraries (no io, os, debug for security)
    luaL_requiref(global_lua_state, "_G", luaopen_base, 1);
    lua_pop(global_lua_state, 1);
    luaL_requiref(global_lua_state, "table", luaopen_table, 1);
    lua_pop(global_lua_state, 1);
    luaL_requiref(global_lua_state, "string", luaopen_string, 1);
    lua_pop(global_lua_state, 1);
    luaL_requiref(global_lua_state, "math", luaopen_math, 1);
    lua_pop(global_lua_state, 1);
    luaL_requiref(global_lua_state, "utf8", luaopen_utf8, 1);
    lua_pop(global_lua_state, 1);
    luaL_requiref(global_lua_state, "coroutine", luaopen_coroutine, 1);
    lua_pop(global_lua_state, 1);

    // Remove dangerous base functions
    lua_pushnil(global_lua_state);
    lua_setglobal(global_lua_state, "dofile");
    lua_pushnil(global_lua_state);
    lua_setglobal(global_lua_state, "loadfile");

    register_all_apis(global_lua_state);

    if (luaL_loadfile(global_lua_state, full_path) != LUA_OK) {
        log_error("Lua load error:", lua_tostring(global_lua_state, -1), 0);
        lua_pop(global_lua_state, 1);
        scenario_lua_shutdown();
        return 0;
    }

    if (lua_pcall(global_lua_state, 0, 0, 0) != LUA_OK) {
        log_error("Lua exec error:", lua_tostring(global_lua_state, -1), 0);
        lua_pop(global_lua_state, 1);
        scenario_lua_shutdown();
        return 0;
    }

    log_info("Lua script loaded:", full_path, 0);
    return 1;
}

void scenario_lua_shutdown(void)
{
    if (global_lua_state) {
        lua_close(global_lua_state);
        global_lua_state = 0;
    }
    lua_api_scenario_clear_handlers();
}

int scenario_lua_is_active(void)
{
    return global_lua_state != 0;
}

lua_State *scenario_lua_get_state(void)
{
    return global_lua_state;
}
