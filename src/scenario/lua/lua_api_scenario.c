#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "core/log.h"
#include "scenario/custom_variable.h"
#include "scenario/data.h"
#include "scenario/event/action_handler.h"
#include "scenario/event/controller.h"
#include "scenario/event/data.h"
#include "scenario/event/event.h"
#include "scenario/property.h"
#include "scenario/lua/lua_state.h"

#include <string.h>

#define MAX_LUA_CUSTOM_HANDLERS 64

typedef struct {
    char name[64];
    int lua_ref;
    int active;
} lua_custom_handler_t;

static lua_custom_handler_t lua_conditions[MAX_LUA_CUSTOM_HANDLERS];
static int lua_condition_count;

static lua_custom_handler_t lua_actions[MAX_LUA_CUSTOM_HANDLERS];
static int lua_action_count;

void lua_api_scenario_clear_handlers(void)
{
    memset(lua_conditions, 0, sizeof(lua_conditions));
    lua_condition_count = 0;
    memset(lua_actions, 0, sizeof(lua_actions));
    lua_action_count = 0;
}

static int find_lua_condition_by_name(const char *name)
{
    for (int i = 0; i < lua_condition_count; i++) {
        if (lua_conditions[i].active && strcmp(lua_conditions[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int find_lua_action_by_name(const char *name)
{
    for (int i = 0; i < lua_action_count; i++) {
        if (lua_actions[i].active && strcmp(lua_actions[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

int scenario_lua_evaluate_custom_condition(int index)
{
    if (index < 0 || index >= lua_condition_count) {
        return 0;
    }
    lua_custom_handler_t *h = &lua_conditions[index];
    if (!h->active) {
        return 0;
    }

    lua_State *L = scenario_lua_get_state();
    if (!L) {
        return 0;
    }

    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, h->lua_ref);
    if (lua_pcall(L, 0, 1, 0) != LUA_OK) {
        log_error("Lua condition error:", lua_tostring(L, -1), 0);
        lua_settop(L, top);
        return 0;
    }
    int result = lua_toboolean(L, -1);
    lua_settop(L, top);
    return result;
}

int scenario_lua_execute_custom_action(int index)
{
    if (index < 0 || index >= lua_action_count) {
        return 0;
    }
    lua_custom_handler_t *h = &lua_actions[index];
    if (!h->active) {
        return 0;
    }

    lua_State *L = scenario_lua_get_state();
    if (!L) {
        return 0;
    }

    int top = lua_gettop(L);
    lua_rawgeti(L, LUA_REGISTRYINDEX, h->lua_ref);
    if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
        log_error("Lua action error:", lua_tostring(L, -1), 0);
        lua_settop(L, top);
        return 0;
    }
    lua_settop(L, top);
    return 1;
}

// Lua bindings

static int l_scenario_name(lua_State *L)
{
    lua_pushstring(L, (const char *) scenario_name());
    return 1;
}

static int l_scenario_start_year(lua_State *L)
{
    lua_pushinteger(L, scenario_property_start_year());
    return 1;
}

static int l_scenario_climate(lua_State *L)
{
    lua_pushinteger(L, scenario_property_climate());
    return 1;
}

static int l_scenario_get_variable(lua_State *L)
{
    if (lua_isinteger(L, 1)) {
        unsigned int id = (unsigned int) lua_tointeger(L, 1);
        lua_pushinteger(L, scenario_custom_variable_get_value(id));
    } else {
        const char *name = luaL_checkstring(L, 1);
        unsigned int id = scenario_custom_variable_get_id_by_name((const uint8_t *) name);
        lua_pushinteger(L, scenario_custom_variable_get_value(id));
    }
    return 1;
}

static int l_scenario_set_variable(lua_State *L)
{
    unsigned int id;
    if (lua_isinteger(L, 1)) {
        id = (unsigned int) lua_tointeger(L, 1);
    } else {
        const char *name = luaL_checkstring(L, 1);
        id = scenario_custom_variable_get_id_by_name((const uint8_t *) name);
    }
    int value = (int) luaL_checkinteger(L, 2);
    scenario_custom_variable_set_value(id, value);
    return 0;
}

static int l_scenario_create_variable(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    int initial_value = (int) luaL_optinteger(L, 2, 0);
    unsigned int id = scenario_custom_variable_create((const uint8_t *) name, initial_value);
    lua_pushinteger(L, id);
    return 1;
}

static int l_scenario_register_condition(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if (lua_condition_count >= MAX_LUA_CUSTOM_HANDLERS) {
        return luaL_error(L, "Maximum Lua conditions reached (%d)", MAX_LUA_CUSTOM_HANDLERS);
    }

    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_custom_handler_t *h = &lua_conditions[lua_condition_count];
    strncpy(h->name, name, 63);
    h->name[63] = '\0';
    h->lua_ref = ref;
    h->active = 1;

    lua_pushinteger(L, lua_condition_count);
    lua_condition_count++;
    return 1;
}

static int l_scenario_register_action(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    if (lua_action_count >= MAX_LUA_CUSTOM_HANDLERS) {
        return luaL_error(L, "Maximum Lua actions reached (%d)", MAX_LUA_CUSTOM_HANDLERS);
    }

    lua_pushvalue(L, 2);
    int ref = luaL_ref(L, LUA_REGISTRYINDEX);

    lua_custom_handler_t *h = &lua_actions[lua_action_count];
    strncpy(h->name, name, 63);
    h->name[63] = '\0';
    h->lua_ref = ref;
    h->active = 1;

    lua_pushinteger(L, lua_action_count);
    lua_action_count++;
    return 1;
}

static int l_scenario_event_count(lua_State *L)
{
    lua_pushinteger(L, scenario_events_get_count());
    return 1;
}

static int l_scenario_create_event(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    lua_getfield(L, 1, "repeat_min");
    int repeat_min = (int) luaL_optinteger(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, 1, "repeat_max");
    int repeat_max = (int) luaL_optinteger(L, -1, 0);
    lua_pop(L, 1);

    lua_getfield(L, 1, "max_repeats");
    int max_repeats = (int) luaL_optinteger(L, -1, 0);
    lua_pop(L, 1);

    scenario_event_t *event = scenario_event_create(repeat_min, repeat_max, max_repeats);
    if (!event) {
        lua_pushnil(L);
        return 1;
    }

    // Link condition by name lookup
    lua_getfield(L, 1, "condition");
    if (lua_isstring(L, -1)) {
        const char *cond_name = lua_tostring(L, -1);
        int cond_idx = find_lua_condition_by_name(cond_name);
        if (cond_idx >= 0) {
            scenario_condition_group_t *group = array_first(event->condition_groups);
            scenario_condition_t *condition = scenario_event_condition_create(group, CONDITION_TYPE_LUA_CUSTOM);
            if (condition) {
                condition->parameter1 = cond_idx;
            }
        }
    }
    lua_pop(L, 1);

    // Link action by name lookup
    lua_getfield(L, 1, "action");
    if (lua_isstring(L, -1)) {
        const char *act_name = lua_tostring(L, -1);
        int act_idx = find_lua_action_by_name(act_name);
        if (act_idx >= 0) {
            scenario_action_t *action = scenario_event_action_create(event, ACTION_TYPE_LUA_CUSTOM);
            if (action) {
                action->parameter1 = act_idx;
            }
        }
    }
    lua_pop(L, 1);

    lua_pushinteger(L, event->id);
    return 1;
}

static int l_scenario_execute_action(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = (int) luaL_checkinteger(L, 1);
    action.parameter1 = (int) luaL_optinteger(L, 2, 0);
    action.parameter2 = (int) luaL_optinteger(L, 3, 0);
    action.parameter3 = (int) luaL_optinteger(L, 4, 0);
    action.parameter4 = (int) luaL_optinteger(L, 5, 0);
    action.parameter5 = (int) luaL_optinteger(L, 6, 0);
    int result = scenario_action_type_execute(&action);
    lua_pushboolean(L, result);
    return 1;
}

static const luaL_Reg scenario_funcs[] = {
    {"name", l_scenario_name},
    {"start_year", l_scenario_start_year},
    {"climate", l_scenario_climate},
    {"get_variable", l_scenario_get_variable},
    {"set_variable", l_scenario_set_variable},
    {"create_variable", l_scenario_create_variable},
    {"register_condition", l_scenario_register_condition},
    {"register_action", l_scenario_register_action},
    {"event_count", l_scenario_event_count},
    {"create_event", l_scenario_create_event},
    {"execute_action", l_scenario_execute_action},
    {NULL, NULL}
};

void lua_api_scenario_register(lua_State *L)
{
    luaL_newlib(L, scenario_funcs);
    lua_setglobal(L, "scenario");
}
