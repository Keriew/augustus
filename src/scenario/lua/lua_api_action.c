#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "scenario/event/action_handler.h"
#include "scenario/event/data.h"

static int execute_simple(scenario_action_t *action)
{
    return scenario_action_type_execute(action);
}

// action.adjust_favor(amount)
static int l_action_adjust_favor(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_ADJUST_FAVOR;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.adjust_money(amount)
static int l_action_adjust_money(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_ADJUST_MONEY;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.adjust_savings(amount)
static int l_action_adjust_savings(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_ADJUST_SAVINGS;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.adjust_city_health(amount, hard_set)
static int l_action_adjust_city_health(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_ADJUST_CITY_HEALTH;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = lua_toboolean(L, 2);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.adjust_rome_wages(amount, hard_set)
static int l_action_adjust_rome_wages(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_ADJUST_ROME_WAGES;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = lua_toboolean(L, 2);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.gladiator_revolt()
static int l_action_gladiator_revolt(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_GLADIATOR_REVOLT;
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.change_allowed_building(building_id, allowed)
static int l_action_change_allowed_building(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_CHANGE_ALLOWED_BUILDINGS;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = lua_toboolean(L, 2);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.change_city_rating(rating_type, value, hard_set)
static int l_action_change_city_rating(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_CHANGE_CITY_RATING;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    action.parameter3 = lua_toboolean(L, 3);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.change_resource_produced(resource_type, enabled)
static int l_action_change_resource_produced(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_CHANGE_RESOURCE_PRODUCED;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = lua_toboolean(L, 2);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.change_resource_stockpiles(resource_type, amount, storage_type, respect_settings)
static int l_action_change_resource_stockpiles(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_CHANGE_RESOURCE_STOCKPILES;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    action.parameter3 = (int) luaL_optinteger(L, 3, 0);
    action.parameter4 = lua_toboolean(L, 4);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.send_standard_message(text_id)
static int l_action_send_standard_message(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_SEND_STANDARD_MESSAGE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.show_custom_message(message_id)
static int l_action_show_custom_message(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_SHOW_CUSTOM_MESSAGE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.tax_rate_set(rate)
static int l_action_tax_rate_set(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_TAX_RATE_SET;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.cause_blessing(god_id)
static int l_action_cause_blessing(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_CAUSE_BLESSING;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.cause_minor_curse(god_id)
static int l_action_cause_minor_curse(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_CAUSE_MINOR_CURSE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.cause_major_curse(god_id)
static int l_action_cause_major_curse(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_CAUSE_MAJOR_CURSE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.change_climate(climate_type)
static int l_action_change_climate(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_CHANGE_CLIMATE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.god_sentiment_change(god_id, amount, hard_set)
static int l_action_god_sentiment_change(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_GOD_SENTIMENT_CHANGE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    action.parameter3 = lua_toboolean(L, 3);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.pop_sentiment_change(amount, hard_set)
static int l_action_pop_sentiment_change(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_POP_SENTIMENT_CHANGE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = lua_toboolean(L, 2);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.win()
static int l_action_win(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_WIN;
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.lose()
static int l_action_lose(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_LOSE;
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.change_rank(rank)
static int l_action_change_rank(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_CHANGE_RANK;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.change_production_rate(resource_type, rate, hard_set)
static int l_action_change_production_rate(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_CHANGE_PRODUCTION_RATE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    action.parameter3 = lua_toboolean(L, 3);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.invasion(attack_type, size, invasion_point, target_type, enemy_id)
static int l_action_invasion(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_INVASION_IMMEDIATE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    action.parameter3 = (int) luaL_optinteger(L, 3, 0);
    action.parameter4 = (int) luaL_optinteger(L, 4, 0);
    action.parameter5 = (int) luaL_optinteger(L, 5, 0);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.building_force_collapse(grid_offset1, grid_offset2, building_type, destroy_all)
static int l_action_building_force_collapse(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_BUILDING_FORCE_COLLAPSE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    action.parameter3 = (int) luaL_optinteger(L, 3, 0);
    action.parameter4 = lua_toboolean(L, 4);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.trade_adjust_price(resource_type, amount, show_message)
static int l_action_trade_adjust_price(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_TRADE_ADJUST_PRICE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    action.parameter3 = lua_toboolean(L, 3);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.trade_set_price(resource_type, price)
static int l_action_trade_set_price(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_TRADE_SET_PRICE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.trade_set_buy_price(resource_type, price)
static int l_action_trade_set_buy_price(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_TRADE_SET_BUY_PRICE_ONLY;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.trade_set_sell_price(resource_type, price)
static int l_action_trade_set_sell_price(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_TRADE_SET_SELL_PRICE_ONLY;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.trade_route_set_open(route_id, apply_cost)
static int l_action_trade_route_set_open(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_TRADE_ROUTE_SET_OPEN;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = lua_toboolean(L, 2);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.trade_adjust_route_amount(route_id, resource_type, amount)
static int l_action_trade_adjust_route_amount(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_TRADE_ADJUST_ROUTE_AMOUNT;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    action.parameter3 = (int) luaL_checkinteger(L, 3);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.trade_route_add_resource(route_id, resource_type)
static int l_action_trade_route_add_resource(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_TRADE_ROUTE_ADD_NEW_RESOURCE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.trade_adjust_route_open_price(route_id, amount)
static int l_action_trade_adjust_route_open_price(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_TRADE_ADJUST_ROUTE_OPEN_PRICE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    action.parameter2 = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.trade_problem_land()
static int l_action_trade_problem_land(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_TRADE_PROBLEM_LAND;
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.trade_problem_sea()
static int l_action_trade_problem_sea(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_TRADE_PROBLEM_SEA;
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.lock_trade_route(route_id)
static int l_action_lock_trade_route(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_LOCK_TRADE_ROUTE;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.empire_map_convert_future_trade_city(city_id)
static int l_action_empire_map_convert_future_trade_city(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_EMPIRE_MAP_CONVERT_FUTURE_TRADE_CITY;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

// action.request_immediately_start(request_id)
static int l_action_request_immediately_start(lua_State *L)
{
    scenario_action_t action = {0};
    action.type = ACTION_TYPE_REQUEST_IMMEDIATELY_START;
    action.parameter1 = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, execute_simple(&action));
    return 1;
}

static const luaL_Reg action_funcs[] = {
    {"adjust_favor", l_action_adjust_favor},
    {"adjust_money", l_action_adjust_money},
    {"adjust_savings", l_action_adjust_savings},
    {"adjust_city_health", l_action_adjust_city_health},
    {"adjust_rome_wages", l_action_adjust_rome_wages},
    {"gladiator_revolt", l_action_gladiator_revolt},
    {"change_allowed_building", l_action_change_allowed_building},
    {"change_city_rating", l_action_change_city_rating},
    {"change_resource_produced", l_action_change_resource_produced},
    {"change_resource_stockpiles", l_action_change_resource_stockpiles},
    {"send_standard_message", l_action_send_standard_message},
    {"show_custom_message", l_action_show_custom_message},
    {"tax_rate_set", l_action_tax_rate_set},
    {"cause_blessing", l_action_cause_blessing},
    {"cause_minor_curse", l_action_cause_minor_curse},
    {"cause_major_curse", l_action_cause_major_curse},
    {"change_climate", l_action_change_climate},
    {"god_sentiment_change", l_action_god_sentiment_change},
    {"pop_sentiment_change", l_action_pop_sentiment_change},
    {"win", l_action_win},
    {"lose", l_action_lose},
    {"change_rank", l_action_change_rank},
    {"change_production_rate", l_action_change_production_rate},
    {"invasion", l_action_invasion},
    {"building_force_collapse", l_action_building_force_collapse},
    {"trade_adjust_price", l_action_trade_adjust_price},
    {"trade_set_price", l_action_trade_set_price},
    {"trade_set_buy_price", l_action_trade_set_buy_price},
    {"trade_set_sell_price", l_action_trade_set_sell_price},
    {"trade_route_set_open", l_action_trade_route_set_open},
    {"trade_adjust_route_amount", l_action_trade_adjust_route_amount},
    {"trade_route_add_resource", l_action_trade_route_add_resource},
    {"trade_adjust_route_open_price", l_action_trade_adjust_route_open_price},
    {"trade_problem_land", l_action_trade_problem_land},
    {"trade_problem_sea", l_action_trade_problem_sea},
    {"lock_trade_route", l_action_lock_trade_route},
    {"empire_map_convert_future_trade_city", l_action_empire_map_convert_future_trade_city},
    {"request_immediately_start", l_action_request_immediately_start},
    {NULL, NULL}
};

void lua_api_action_register(lua_State *L)
{
    luaL_newlib(L, action_funcs);
    lua_setglobal(L, "action");
}
