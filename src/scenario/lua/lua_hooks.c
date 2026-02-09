#include "scenario/lua/lua_hooks.h"
#include "scenario/lua/lua_state.h"

#include "lua/lua.h"
#include "lua/lauxlib.h"
#include "core/log.h"

static int call_lua_hook(const char *func_name)
{
    lua_State *L = scenario_lua_get_state();
    if (!L) {
        return 0;
    }

    int top = lua_gettop(L);
    lua_getglobal(L, func_name);
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return 0;
    }
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        log_error("Lua hook error in", func_name, 0);
        log_error("  ", lua_tostring(L, -1), 0);
        lua_settop(L, top);
        return 0;
    }
    lua_settop(L, top);
    return 1;
}

void scenario_lua_hook_on_load(void)
{
    call_lua_hook("on_load");
}

void scenario_lua_hook_on_tick(void)
{
    call_lua_hook("on_tick");
}

void scenario_lua_hook_on_month(void)
{
    call_lua_hook("on_month");
}

void scenario_lua_hook_on_year(void)
{
    call_lua_hook("on_year");
}

void scenario_lua_hook_on_event(const char *event_name)
{
    lua_State *L = scenario_lua_get_state();
    if (!L) {
        return;
    }

    int top = lua_gettop(L);
    lua_getglobal(L, "on_event");
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return;
    }
    lua_pushstring(L, event_name);
    if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
        log_error("Lua on_event error:", lua_tostring(L, -1), 0);
    }
    lua_settop(L, top);
}

void scenario_lua_hook_on_building_placed(int type, int x, int y, int building_id)
{
    lua_State *L = scenario_lua_get_state();
    if (!L) {
        return;
    }

    int top = lua_gettop(L);
    lua_getglobal(L, "on_building_placed");
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return;
    }
    lua_pushinteger(L, type);
    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    lua_pushinteger(L, building_id);
    if (lua_pcall(L, 4, 0, 0) != LUA_OK) {
        log_error("Lua on_building_placed error:", lua_tostring(L, -1), 0);
    }
    lua_settop(L, top);
}

void scenario_lua_hook_on_building_destroyed(int type, int x, int y, int building_id)
{
    lua_State *L = scenario_lua_get_state();
    if (!L) {
        return;
    }

    int top = lua_gettop(L);
    lua_getglobal(L, "on_building_destroyed");
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return;
    }
    lua_pushinteger(L, type);
    lua_pushinteger(L, x);
    lua_pushinteger(L, y);
    lua_pushinteger(L, building_id);
    if (lua_pcall(L, 4, 0, 0) != LUA_OK) {
        log_error("Lua on_building_destroyed error:", lua_tostring(L, -1), 0);
    }
    lua_settop(L, top);
}

void scenario_lua_hook_on_victory(void)
{
    call_lua_hook("on_victory");
}

void scenario_lua_hook_on_defeat(void)
{
    call_lua_hook("on_defeat");
}
