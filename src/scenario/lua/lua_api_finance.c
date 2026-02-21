#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "city/finance.h"

static int l_finance_treasury(lua_State *L)
{
    lua_pushinteger(L, city_finance_treasury());
    return 1;
}

static int l_finance_add_treasury(lua_State *L)
{
    int amount = (int) luaL_checkinteger(L, 1);
    city_finance_treasury_add(amount);
    return 0;
}

static int l_finance_tax_percentage(lua_State *L)
{
    lua_pushinteger(L, city_finance_tax_percentage());
    return 1;
}

static int l_finance_set_tax_percentage(lua_State *L)
{
    int rate = (int) luaL_checkinteger(L, 1);
    city_finance_set_tax_percentage(rate);
    return 0;
}

static const luaL_Reg finance_funcs[] = {
    {"treasury", l_finance_treasury},
    {"add_treasury", l_finance_add_treasury},
    {"tax_percentage", l_finance_tax_percentage},
    {"set_tax_percentage", l_finance_set_tax_percentage},
    {NULL, NULL}
};

void lua_api_finance_register(lua_State *L)
{
    luaL_newlib(L, finance_funcs);
    lua_setglobal(L, "finance");
}
