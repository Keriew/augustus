#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "map/building.h"
#include "map/grid.h"
#include "map/terrain.h"
#include "scenario/data.h"

static int l_map_width(lua_State *L)
{
    lua_pushinteger(L, scenario.map.width);
    return 1;
}

static int l_map_height(lua_State *L)
{
    lua_pushinteger(L, scenario.map.height);
    return 1;
}

static int l_map_terrain_at(lua_State *L)
{
    int grid_offset = (int) luaL_checkinteger(L, 1);
    lua_pushinteger(L, map_terrain_get(grid_offset));
    return 1;
}

static int l_map_building_at(lua_State *L)
{
    int grid_offset = (int) luaL_checkinteger(L, 1);
    lua_pushinteger(L, map_building_at(grid_offset));
    return 1;
}

static int l_map_grid_offset(lua_State *L)
{
    int x = (int) luaL_checkinteger(L, 1);
    int y = (int) luaL_checkinteger(L, 2);
    lua_pushinteger(L, map_grid_offset(x, y));
    return 1;
}

static const luaL_Reg map_funcs[] = {
    {"width", l_map_width},
    {"height", l_map_height},
    {"terrain_at", l_map_terrain_at},
    {"building_at", l_map_building_at},
    {"grid_offset", l_map_grid_offset},
    {NULL, NULL}
};

void lua_api_map_register(lua_State *L)
{
    luaL_newlib(L, map_funcs);
    lua_setglobal(L, "map");
}
