#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "city/victory.h"
#include "game/settings.h"
#include "game/time.h"
#include "scenario/lua/lua_state.h"

static int l_game_year(lua_State *L)
{
    lua_pushinteger(L, game_time_year());
    return 1;
}

static int l_game_month(lua_State *L)
{
    lua_pushinteger(L, game_time_month());
    return 1;
}

static int l_game_day(lua_State *L)
{
    lua_pushinteger(L, game_time_day());
    return 1;
}

static int l_game_tick(lua_State *L)
{
    lua_pushinteger(L, game_time_tick());
    return 1;
}

static int l_game_speed(lua_State *L)
{
    lua_pushinteger(L, setting_game_speed());
    return 1;
}

static int l_game_set_speed(lua_State *L)
{
    int speed = (int) luaL_checkinteger(L, 1);
    setting_set_game_speed(speed);
    return 0;
}

static int l_game_win(lua_State *L)
{
    city_victory_force_win();
    return 0;
}

static int l_game_lose(lua_State *L)
{
    city_victory_force_lose();
    return 0;
}

static int l_game_api_version(lua_State *L)
{
    lua_pushinteger(L, LUA_API_VERSION);
    return 1;
}

static const luaL_Reg game_funcs[] = {
    {"year", l_game_year},
    {"month", l_game_month},
    {"day", l_game_day},
    {"tick", l_game_tick},
    {"speed", l_game_speed},
    {"set_speed", l_game_set_speed},
    {"win", l_game_win},
    {"lose", l_game_lose},
    {"api_version", l_game_api_version},
    {NULL, NULL}
};

void lua_api_game_register(lua_State *L)
{
    luaL_newlib(L, game_funcs);
    lua_setglobal(L, "game");
}
