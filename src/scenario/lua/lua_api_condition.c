#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "scenario/event/condition_handler.h"
#include "scenario/event/data.h"

static int check(lua_State *L, scenario_condition_t *c)
{
    lua_pushboolean(L, scenario_condition_type_is_met(c));
    return 1;
}

// condition.time_passed(comparison, months)
static int l_condition_time_passed(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_TIME_PASSED;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    int months = (int) luaL_checkinteger(L, 2);
    c.parameter4 = months; // time_met reads parameter4 directly
    return check(L, &c);
}

// condition.difficulty(comparison, value)
static int l_condition_difficulty(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_DIFFICULTY;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    return check(L, &c);
}

// condition.money(comparison, value)
static int l_condition_money(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_MONEY;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    return check(L, &c);
}

// condition.savings(comparison, value)
static int l_condition_savings(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_SAVINGS;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    return check(L, &c);
}

// condition.favor(comparison, value)
static int l_condition_favor(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_STATS_FAVOR;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    return check(L, &c);
}

// condition.prosperity(comparison, value)
static int l_condition_prosperity(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_STATS_PROSPERITY;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    return check(L, &c);
}

// condition.culture(comparison, value)
static int l_condition_culture(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_STATS_CULTURE;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    return check(L, &c);
}

// condition.peace(comparison, value)
static int l_condition_peace(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_STATS_PEACE;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    return check(L, &c);
}

// condition.city_health(comparison, value)
static int l_condition_city_health(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_STATS_CITY_HEALTH;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    return check(L, &c);
}

// condition.rome_wages(comparison, value)
static int l_condition_rome_wages(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_ROME_WAGES;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    return check(L, &c);
}

// condition.tax_rate(comparison, value)
static int l_condition_tax_rate(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_TAX_RATE;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    return check(L, &c);
}

// condition.city_population(comparison, value, pop_class)
static int l_condition_city_population(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_CITY_POPULATION;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = (int) luaL_optinteger(L, 3, POP_CLASS_ALL);
    return check(L, &c);
}

// condition.unemployment(use_percentage, comparison, value)
static int l_condition_unemployment(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_POPS_UNEMPLOYMENT;
    c.parameter1 = lua_toboolean(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = (int) luaL_checkinteger(L, 3);
    return check(L, &c);
}

// condition.trade_sell_price(resource_type, comparison, value)
static int l_condition_trade_sell_price(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_TRADE_SELL_PRICE;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = (int) luaL_checkinteger(L, 3);
    return check(L, &c);
}

// condition.building_count_active(comparison, value, building_type)
static int l_condition_building_count_active(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_BUILDING_COUNT_ACTIVE;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = (int) luaL_checkinteger(L, 3);
    return check(L, &c);
}

// condition.building_count_any(comparison, value, building_type)
static int l_condition_building_count_any(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_BUILDING_COUNT_ANY;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = (int) luaL_checkinteger(L, 3);
    return check(L, &c);
}

// condition.building_count_area(grid_offset1, grid_offset2, building_type, comparison, value)
static int l_condition_building_count_area(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_BUILDING_COUNT_AREA;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = (int) luaL_checkinteger(L, 3);
    c.parameter4 = (int) luaL_checkinteger(L, 4);
    c.parameter5 = (int) luaL_checkinteger(L, 5);
    return check(L, &c);
}

// condition.count_own_troops(comparison, value, in_city_only)
static int l_condition_count_own_troops(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_COUNT_OWN_TROOPS;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = lua_toboolean(L, 3);
    return check(L, &c);
}

// condition.custom_variable_check(variable_id, comparison, value)
static int l_condition_custom_variable_check(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_CUSTOM_VARIABLE_CHECK;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = (int) luaL_checkinteger(L, 3);
    return check(L, &c);
}

// condition.request_is_ongoing(request_id, check_for_ongoing)
static int l_condition_request_is_ongoing(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_REQUEST_IS_ONGOING;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = lua_toboolean(L, 2);
    return check(L, &c);
}

// condition.trade_route_open(route_id, check_for_open)
static int l_condition_trade_route_open(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_TRADE_ROUTE_OPEN;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = lua_toboolean(L, 2);
    return check(L, &c);
}

// condition.trade_route_price(route_id, comparison, value)
static int l_condition_trade_route_price(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_TRADE_ROUTE_PRICE;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = (int) luaL_checkinteger(L, 3);
    return check(L, &c);
}

// condition.resource_stored_count(resource_type, comparison, value, storage_type)
static int l_condition_resource_stored_count(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_RESOURCE_STORED_COUNT;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = (int) luaL_checkinteger(L, 3);
    c.parameter4 = (int) luaL_optinteger(L, 4, 0);
    return check(L, &c);
}

// condition.resource_storage_available(resource_type, comparison, value, storage_type)
static int l_condition_resource_storage_available(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_RESOURCE_STORAGE_AVAILABLE;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = (int) luaL_checkinteger(L, 3);
    c.parameter4 = (int) luaL_optinteger(L, 4, 0);
    return check(L, &c);
}

// condition.terrain_in_area(grid_offset1, grid_offset2, terrain_type, comparison, value)
static int l_condition_terrain_in_area(lua_State *L)
{
    scenario_condition_t c = {0};
    c.type = CONDITION_TYPE_TERRAIN_IN_AREA;
    c.parameter1 = (int) luaL_checkinteger(L, 1);
    c.parameter2 = (int) luaL_checkinteger(L, 2);
    c.parameter3 = (int) luaL_checkinteger(L, 3);
    c.parameter4 = (int) luaL_checkinteger(L, 4);
    c.parameter5 = (int) luaL_checkinteger(L, 5);
    return check(L, &c);
}

static const luaL_Reg condition_funcs[] = {
    {"time_passed", l_condition_time_passed},
    {"difficulty", l_condition_difficulty},
    {"money", l_condition_money},
    {"savings", l_condition_savings},
    {"favor", l_condition_favor},
    {"prosperity", l_condition_prosperity},
    {"culture", l_condition_culture},
    {"peace", l_condition_peace},
    {"city_health", l_condition_city_health},
    {"rome_wages", l_condition_rome_wages},
    {"tax_rate", l_condition_tax_rate},
    {"city_population", l_condition_city_population},
    {"unemployment", l_condition_unemployment},
    {"trade_sell_price", l_condition_trade_sell_price},
    {"building_count_active", l_condition_building_count_active},
    {"building_count_any", l_condition_building_count_any},
    {"building_count_area", l_condition_building_count_area},
    {"count_own_troops", l_condition_count_own_troops},
    {"custom_variable_check", l_condition_custom_variable_check},
    {"request_is_ongoing", l_condition_request_is_ongoing},
    {"trade_route_open", l_condition_trade_route_open},
    {"trade_route_price", l_condition_trade_route_price},
    {"resource_stored_count", l_condition_resource_stored_count},
    {"resource_storage_available", l_condition_resource_storage_available},
    {"terrain_in_area", l_condition_terrain_in_area},
    {NULL, NULL}
};

static void register_int_constant(lua_State *L, const char *table, const char *name, int value)
{
    lua_getglobal(L, table);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setglobal(L, table);
        lua_getglobal(L, table);
    }
    lua_pushinteger(L, value);
    lua_setfield(L, -2, name);
    lua_pop(L, 1);
}

void lua_api_condition_register(lua_State *L)
{
    luaL_newlib(L, condition_funcs);
    lua_setglobal(L, "condition");

    // COMPARE constants
    register_int_constant(L, "COMPARE", "EQUAL", COMPARISON_TYPE_EQUAL);
    register_int_constant(L, "COMPARE", "EQUAL_OR_LESS", COMPARISON_TYPE_EQUAL_OR_LESS);
    register_int_constant(L, "COMPARE", "EQUAL_OR_MORE", COMPARISON_TYPE_EQUAL_OR_MORE);
    register_int_constant(L, "COMPARE", "NOT_EQUAL", COMPARISON_TYPE_NOT_EQUAL);
    register_int_constant(L, "COMPARE", "LESS_THAN", COMPARISON_TYPE_LESS_THAN);
    register_int_constant(L, "COMPARE", "GREATER_THAN", COMPARISON_TYPE_GREATER_THAN);

    // POP_CLASS constants
    register_int_constant(L, "POP_CLASS", "ALL", POP_CLASS_ALL);
    register_int_constant(L, "POP_CLASS", "PATRICIAN", POP_CLASS_PATRICIAN);
    register_int_constant(L, "POP_CLASS", "PLEBEIAN", POP_CLASS_PLEBEIAN);
    register_int_constant(L, "POP_CLASS", "SLUMS", POP_CLASS_SLUMS);
}
