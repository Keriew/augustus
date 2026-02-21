#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "city/data_private.h"
#include "city/health.h"
#include "city/labor.h"
#include "city/message.h"
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

static int l_city_change_prosperity(lua_State *L)
{
    int amount = (int) luaL_checkinteger(L, 1);
    city_ratings_change_prosperity(amount);
    return 0;
}

static int l_city_change_peace(lua_State *L)
{
    int amount = (int) luaL_checkinteger(L, 1);
    city_ratings_change_peace(amount);
    return 0;
}

static int l_city_rome_wages(lua_State *L)
{
    lua_pushinteger(L, city_labor_wages_rome());
    return 1;
}

static int l_city_change_rome_wages(lua_State *L)
{
    int amount = (int) luaL_checkinteger(L, 1);
    if (amount == 0) {
        return 0;
    }
    city_data.labor.months_since_last_wage_change = 0;
    city_data.labor.wages_rome += amount;
    if (city_data.labor.wages_rome < 1) {
        city_data.labor.wages_rome = 1;
    }
    if (amount > 0) {
        city_message_post(1, MESSAGE_ROME_RAISES_WAGES, 0, 0);
    } else {
        city_message_post(1, MESSAGE_ROME_LOWERS_WAGES, 0, 0);
    }
    return 0;
}

static int l_city_set_rome_wages(lua_State *L)
{
    int amount = (int) luaL_checkinteger(L, 1);
    int current_wages = city_labor_wages_rome();
    if (amount == current_wages) {
        return 0;
    }
    if (amount < 1) {
        amount = 1;
    }
    city_data.labor.months_since_last_wage_change = 0;
    city_data.labor.wages_rome = amount;
    if (amount > current_wages) {
        city_message_post(1, MESSAGE_ROME_RAISES_WAGES, 0, 0);
    } else {
        city_message_post(1, MESSAGE_ROME_LOWERS_WAGES, 0, 0);
    }
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
    {"change_prosperity", l_city_change_prosperity},
    {"change_peace", l_city_change_peace},
    {"rome_wages", l_city_rome_wages},
    {"change_rome_wages", l_city_change_rome_wages},
    {"set_rome_wages", l_city_set_rome_wages},
    {NULL, NULL}
};

void lua_api_city_register(lua_State *L)
{
    luaL_newlib(L, city_funcs);
    lua_setglobal(L, "city");
}
