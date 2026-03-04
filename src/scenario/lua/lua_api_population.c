#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "city/population.h"

static int l_population_school_age(lua_State *L)
{
    lua_pushinteger(L, city_population_school_age());
    return 1;
}

static int l_population_academy_age(lua_State *L)
{
    lua_pushinteger(L, city_population_academy_age());
    return 1;
}

static int l_population_working_age(lua_State *L)
{
    lua_pushinteger(L, city_population_people_of_working_age());
    return 1;
}

static int l_population_retired(lua_State *L)
{
    lua_pushinteger(L, city_population_retired_people());
    return 1;
}

static int l_population_births(lua_State *L)
{
    lua_pushinteger(L, city_population_yearly_births());
    return 1;
}

static int l_population_deaths(lua_State *L)
{
    lua_pushinteger(L, city_population_yearly_deaths());
    return 1;
}

static int l_population_average_age(lua_State *L)
{
    lua_pushinteger(L, city_population_average_age());
    return 1;
}

static const luaL_Reg population_funcs[] = {
    {"school_age", l_population_school_age},
    {"academy_age", l_population_academy_age},
    {"working_age", l_population_working_age},
    {"retired", l_population_retired},
    {"births", l_population_births},
    {"deaths", l_population_deaths},
    {"average_age", l_population_average_age},
    {NULL, NULL}
};

void lua_api_population_register(lua_State *L)
{
    luaL_newlib(L, population_funcs);
    lua_setglobal(L, "population");
}
