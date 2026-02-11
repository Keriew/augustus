#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "building/building.h"
#include "building/count.h"
#include "building/destruction.h"

// building.count_active(type)
static int l_building_count_active(lua_State *L)
{
    building_type type = (building_type) luaL_checkinteger(L, 1);
    lua_pushinteger(L, building_count_active(type));
    return 1;
}

// building.count_in_area(type, x1, y1, x2, y2)
static int l_building_count_in_area(lua_State *L)
{
    building_type type = (building_type) luaL_checkinteger(L, 1);
    int x1 = (int) luaL_checkinteger(L, 2);
    int y1 = (int) luaL_checkinteger(L, 3);
    int x2 = (int) luaL_checkinteger(L, 4);
    int y2 = (int) luaL_checkinteger(L, 5);
    lua_pushinteger(L, building_count_in_area(type, x1, y1, x2, y2));
    return 1;
}

// building.count_upgraded(type)
static int l_building_count_upgraded(lua_State *L)
{
    building_type type = (building_type) luaL_checkinteger(L, 1);
    lua_pushinteger(L, building_count_upgraded(type));
    return 1;
}

// building.get_type(building_id)
static int l_building_get_type(lua_State *L)
{
    int id = (int) luaL_checkinteger(L, 1);
    building *b = building_get(id);
    if (!b || b->state == BUILDING_STATE_UNUSED) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, b->type);
    return 1;
}

// building.get_state(building_id)
static int l_building_get_state(lua_State *L)
{
    int id = (int) luaL_checkinteger(L, 1);
    building *b = building_get(id);
    if (!b || b->state == BUILDING_STATE_UNUSED) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, b->state);
    return 1;
}

// building.set_fire(building_id)
static int l_building_set_fire(lua_State *L)
{
    int id = (int) luaL_checkinteger(L, 1);
    building *b = building_get(id);
    if (!b || b->state == BUILDING_STATE_UNUSED) {
        lua_pushboolean(L, 0);
        return 1;
    }
    building_destroy_by_fire(b);
    lua_pushboolean(L, 1);
    return 1;
}

// building.destroy(building_id)
static int l_building_destroy(lua_State *L)
{
    int id = (int) luaL_checkinteger(L, 1);
    building *b = building_get(id);
    if (!b || b->state == BUILDING_STATE_UNUSED) {
        lua_pushboolean(L, 0);
        return 1;
    }
    building_destroy_by_collapse(b);
    lua_pushboolean(L, 1);
    return 1;
}

static const luaL_Reg building_funcs[] = {
    {"count_active", l_building_count_active},
    {"count_in_area", l_building_count_in_area},
    {"count_upgraded", l_building_count_upgraded},
    {"get_type", l_building_get_type},
    {"get_state", l_building_get_state},
    {"set_fire", l_building_set_fire},
    {"destroy", l_building_destroy},
    {NULL, NULL}
};

void lua_api_building_register(lua_State *L)
{
    luaL_newlib(L, building_funcs);

    // BUILDING_STATE constants
    lua_newtable(L);
    lua_pushinteger(L, BUILDING_STATE_IN_USE);
    lua_setfield(L, -2, "IN_USE");
    lua_pushinteger(L, BUILDING_STATE_RUBBLE);
    lua_setfield(L, -2, "RUBBLE");
    lua_pushinteger(L, BUILDING_STATE_DELETED_BY_GAME);
    lua_setfield(L, -2, "DELETED_BY_GAME");
    lua_pushinteger(L, BUILDING_STATE_DELETED_BY_PLAYER);
    lua_setfield(L, -2, "DELETED_BY_PLAYER");
    lua_pushinteger(L, BUILDING_STATE_MOTHBALLED);
    lua_setfield(L, -2, "MOTHBALLED");
    lua_pushinteger(L, BUILDING_STATE_CREATED);
    lua_setfield(L, -2, "CREATED");
    lua_setfield(L, -2, "STATE");

    lua_setglobal(L, "building");
}
