#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "city/health.h"
#include "city/population.h"
#include "city/ratings.h"
#include "city/sentiment.h"

static int l_city_population(lua_State *L)
{
    lua_pushinteger(L, city_population());
    return 1;
}

static int l_city_health(lua_State *L)
{
    lua_pushinteger(L, city_health());
    return 1;
}

static int l_city_change_health(lua_State *L)
{
    int amount = (int) luaL_checkinteger(L, 1);
    city_health_change(amount);
    return 0;
}

static int l_city_sentiment(lua_State *L)
{
    lua_pushinteger(L, city_sentiment());
    return 1;
}

static int l_city_change_sentiment(lua_State *L)
{
    int amount = (int) luaL_checkinteger(L, 1);
    city_sentiment_change_happiness(amount);
    return 0;
}

static int l_city_rating_culture(lua_State *L)
{
    lua_pushinteger(L, city_rating_culture());
    return 1;
}

static int l_city_rating_prosperity(lua_State *L)
{
    lua_pushinteger(L, city_rating_prosperity());
    return 1;
}

static int l_city_rating_peace(lua_State *L)
{
    lua_pushinteger(L, city_rating_peace());
    return 1;
}

static int l_city_rating_favor(lua_State *L)
{
    lua_pushinteger(L, city_rating_favor());
    return 1;
}

static int l_city_change_favor(lua_State *L)
{
    int amount = (int) luaL_checkinteger(L, 1);
    city_ratings_change_favor(amount);
    return 0;
}

static const luaL_Reg city_funcs[] = {
    {"population", l_city_population},
    {"health", l_city_health},
    {"change_health", l_city_change_health},
    {"sentiment", l_city_sentiment},
    {"change_sentiment", l_city_change_sentiment},
    {"rating_culture", l_city_rating_culture},
    {"rating_prosperity", l_city_rating_prosperity},
    {"rating_peace", l_city_rating_peace},
    {"rating_favor", l_city_rating_favor},
    {"change_favor", l_city_change_favor},
    {NULL, NULL}
};

void lua_api_city_register(lua_State *L)
{
    luaL_newlib(L, city_funcs);
    lua_setglobal(L, "city");
}
