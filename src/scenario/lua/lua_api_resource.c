#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "city/resource.h"

// resource.total(type)
static int l_resource_total(lua_State *L)
{
    resource_type type = (resource_type) luaL_checkinteger(L, 1);
    lua_pushinteger(L, city_resource_get_total_amount(type, 0));
    return 1;
}

// resource.food_stored()
static int l_resource_food_stored(lua_State *L)
{
    lua_pushinteger(L, city_resource_food_stored());
    return 1;
}

// resource.food_supply_months()
static int l_resource_food_supply_months(lua_State *L)
{
    lua_pushinteger(L, city_resource_food_supply_months());
    return 1;
}

// resource.food_production_pct()
static int l_resource_food_production_pct(lua_State *L)
{
    lua_pushinteger(L, city_resource_food_percentage_produced());
    return 1;
}

// resource.granary_space(food_type)
static int l_resource_granary_space(lua_State *L)
{
    resource_type type = (resource_type) luaL_checkinteger(L, 1);
    lua_pushinteger(L, city_resource_get_available_empty_space_granaries(type));
    return 1;
}

// resource.warehouse_space(resource_type)
static int l_resource_warehouse_space(lua_State *L)
{
    resource_type type = (resource_type) luaL_checkinteger(L, 1);
    lua_pushinteger(L, city_resource_get_available_empty_space_warehouses(type));
    return 1;
}

static const luaL_Reg resource_funcs[] = {
    {"total", l_resource_total},
    {"food_stored", l_resource_food_stored},
    {"food_supply_months", l_resource_food_supply_months},
    {"food_production_pct", l_resource_food_production_pct},
    {"granary_space", l_resource_granary_space},
    {"warehouse_space", l_resource_warehouse_space},
    {NULL, NULL}
};

void lua_api_resource_register(lua_State *L)
{
    luaL_newlib(L, resource_funcs);
    lua_setglobal(L, "resource");
}
