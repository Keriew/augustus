#include "lua/lua.h"
#include "lua/lauxlib.h"

#include "core/log.h"
#include "core/string.h"
#include "empire/city.h"
#include "empire/empire.h"
#include "empire/object.h"
#include "empire/trade_prices.h"
#include "empire/trade_route.h"
#include "empire/type.h"

// ---------------------------------------------------------------------------
// Empire city queries
// ---------------------------------------------------------------------------

// empire.city_type(city_id) -> integer
static int l_empire_city_type(lua_State *L)
{
    int city_id = (int) luaL_checkinteger(L, 1);
    empire_city *city = empire_city_get(city_id);
    if (!city || !city->in_use) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushinteger(L, city->type);
    return 1;
}

// empire.city_is_open(city_id) -> boolean
static int l_empire_city_is_open(lua_State *L)
{
    int city_id = (int) luaL_checkinteger(L, 1);
    empire_city *city = empire_city_get(city_id);
    if (!city || !city->in_use) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, city->is_open);
    return 1;
}

// empire.city_route_id(city_id) -> integer
static int l_empire_city_route_id(lua_State *L)
{
    int city_id = (int) luaL_checkinteger(L, 1);
    lua_pushinteger(L, empire_city_get_route_id(city_id));
    return 1;
}

// empire.city_sells_resource(city_id, resource) -> boolean
static int l_empire_city_sells_resource(lua_State *L)
{
    int city_id = (int) luaL_checkinteger(L, 1);
    int resource = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, empire_city_sells_resource(city_id, resource));
    return 1;
}

// empire.city_buys_resource(city_id, resource) -> boolean
static int l_empire_city_buys_resource(lua_State *L)
{
    int city_id = (int) luaL_checkinteger(L, 1);
    int resource = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, empire_city_buys_resource(city_id, resource));
    return 1;
}

// empire.is_route_open(route_id) -> boolean
static int l_empire_is_route_open(lua_State *L)
{
    int route_id = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, empire_city_is_trade_route_open(route_id));
    return 1;
}

// empire.is_sea_trade(route_id) -> boolean
static int l_empire_is_sea_trade(lua_State *L)
{
    int route_id = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, empire_city_is_trade_route_sea(route_id));
    return 1;
}

// empire.trade_route_cost(route_id) -> integer
static int l_empire_trade_route_cost(lua_State *L)
{
    int route_id = (int) luaL_checkinteger(L, 1);
    lua_pushinteger(L, empire_city_get_trade_route_cost(route_id));
    return 1;
}

// empire.set_trade_route_cost(route_id, cost)
static int l_empire_set_trade_route_cost(lua_State *L)
{
    int route_id = (int) luaL_checkinteger(L, 1);
    int cost = (int) luaL_checkinteger(L, 2);
    empire_city_set_trade_route_cost(route_id, cost);
    return 0;
}

// empire.open_trade(city_id, apply_cost?)
static int l_empire_open_trade(lua_State *L)
{
    int city_id = (int) luaL_checkinteger(L, 1);
    int apply_cost = lua_toboolean(L, 2);
    empire_city_open_trade(city_id, apply_cost);
    return 0;
}

// empire.count_trade_routes(is_sea, is_open) -> integer
static int l_empire_count_trade_routes(lua_State *L)
{
    int is_sea = lua_toboolean(L, 1);
    int is_open = lua_toboolean(L, 2);
    lua_pushinteger(L, empire_city_get_trade_routes_count(is_sea, is_open));
    return 1;
}

// empire.count_wine_sources() -> integer
static int l_empire_count_wine_sources(lua_State *L)
{
    lua_pushinteger(L, empire_city_count_wine_sources());
    return 1;
}

// empire.can_import(resource) -> boolean
static int l_empire_can_import(lua_State *L)
{
    int resource = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, empire_can_import_resource(resource));
    return 1;
}

// empire.can_export(resource) -> boolean
static int l_empire_can_export(lua_State *L)
{
    int resource = (int) luaL_checkinteger(L, 1);
    lua_pushboolean(L, empire_can_export_resource(resource));
    return 1;
}

// empire.can_import_from_city(city_id, resource) -> boolean
static int l_empire_can_import_from_city(lua_State *L)
{
    int city_id = (int) luaL_checkinteger(L, 1);
    int resource = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, empire_can_import_resource_from_city(city_id, resource));
    return 1;
}

// empire.can_export_to_city(city_id, resource) -> boolean
static int l_empire_can_export_to_city(lua_State *L)
{
    int city_id = (int) luaL_checkinteger(L, 1);
    int resource = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, empire_can_export_resource_to_city(city_id, resource));
    return 1;
}

// ---------------------------------------------------------------------------
// Trade routes
// ---------------------------------------------------------------------------

// empire.route_limit(route_id, resource) -> integer
static int l_empire_route_limit(lua_State *L)
{
    int route_id = (int) luaL_checkinteger(L, 1);
    int resource = (int) luaL_checkinteger(L, 2);
    lua_pushinteger(L, trade_route_limit(route_id, resource));
    return 1;
}

// empire.route_traded(route_id, resource) -> integer
static int l_empire_route_traded(lua_State *L)
{
    int route_id = (int) luaL_checkinteger(L, 1);
    int resource = (int) luaL_checkinteger(L, 2);
    lua_pushinteger(L, trade_route_traded(route_id, resource));
    return 1;
}

// empire.set_route_limit(route_id, resource, amount)
static int l_empire_set_route_limit(lua_State *L)
{
    int route_id = (int) luaL_checkinteger(L, 1);
    int resource = (int) luaL_checkinteger(L, 2);
    int amount = (int) luaL_checkinteger(L, 3);
    trade_route_set_limit(route_id, resource, amount);
    return 0;
}

// empire.route_limit_reached(route_id, resource) -> boolean
static int l_empire_route_limit_reached(lua_State *L)
{
    int route_id = (int) luaL_checkinteger(L, 1);
    int resource = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, trade_route_limit_reached(route_id, resource));
    return 1;
}

// ---------------------------------------------------------------------------
// Trade prices
// ---------------------------------------------------------------------------

// empire.buy_price(resource, is_land?) -> integer
static int l_empire_buy_price(lua_State *L)
{
    int resource = (int) luaL_checkinteger(L, 1);
    int land_trader = lua_toboolean(L, 2);
    lua_pushinteger(L, trade_price_buy(resource, land_trader));
    return 1;
}

// empire.sell_price(resource, is_land?) -> integer
static int l_empire_sell_price(lua_State *L)
{
    int resource = (int) luaL_checkinteger(L, 1);
    int land_trader = lua_toboolean(L, 2);
    lua_pushinteger(L, trade_price_sell(resource, land_trader));
    return 1;
}

// empire.base_buy_price(resource) -> integer
static int l_empire_base_buy_price(lua_State *L)
{
    int resource = (int) luaL_checkinteger(L, 1);
    lua_pushinteger(L, trade_price_base_buy(resource));
    return 1;
}

// empire.base_sell_price(resource) -> integer
static int l_empire_base_sell_price(lua_State *L)
{
    int resource = (int) luaL_checkinteger(L, 1);
    lua_pushinteger(L, trade_price_base_sell(resource));
    return 1;
}

// empire.change_price(resource, amount) -> boolean
static int l_empire_change_price(lua_State *L)
{
    int resource = (int) luaL_checkinteger(L, 1);
    int amount = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, trade_price_change(resource, amount));
    return 1;
}

// empire.set_buy_price(resource, price) -> boolean
static int l_empire_set_buy_price(lua_State *L)
{
    int resource = (int) luaL_checkinteger(L, 1);
    int price = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, trade_price_set_buy(resource, price));
    return 1;
}

// empire.set_sell_price(resource, price) -> boolean
static int l_empire_set_sell_price(lua_State *L)
{
    int resource = (int) luaL_checkinteger(L, 1);
    int price = (int) luaL_checkinteger(L, 2);
    lua_pushboolean(L, trade_price_set_sell(resource, price));
    return 1;
}

// ---------------------------------------------------------------------------
// Empire objects
// ---------------------------------------------------------------------------

// empire.object_count() -> integer
static int l_empire_object_count(lua_State *L)
{
    lua_pushinteger(L, empire_object_count());
    return 1;
}

// empire.selected_object() -> integer
static int l_empire_selected_object(lua_State *L)
{
    lua_pushinteger(L, empire_selected_object());
    return 1;
}

// ---------------------------------------------------------------------------
// City creation
// ---------------------------------------------------------------------------

// empire.create_city({ name, x, y, type, trade_route_type, trade_route_cost, buys, sells })
// Returns the empire object id of the newly created city, or nil on failure.
static int l_empire_create_city(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);

    // Required: name
    lua_getfield(L, 1, "name");
    const char *name = luaL_checkstring(L, -1);
    lua_pop(L, 1);

    // Required: x, y (empire map coordinates)
    lua_getfield(L, 1, "x");
    int x = (int) luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    lua_getfield(L, 1, "y");
    int y = (int) luaL_checkinteger(L, -1);
    lua_pop(L, 1);

    // Optional: type (default EMPIRE_CITY_TRADE)
    lua_getfield(L, 1, "type");
    int city_type = lua_isnil(L, -1) ? EMPIRE_CITY_TRADE : (int) lua_tointeger(L, -1);
    lua_pop(L, 1);

    // Optional: trade_route_type "land" or "sea" (default land)
    lua_getfield(L, 1, "trade_route_type");
    int is_sea_route = 0;
    if (lua_isstring(L, -1)) {
        const char *rt = lua_tostring(L, -1);
        if (rt[0] == 's' || rt[0] == 'S') {
            is_sea_route = 1;
        }
    }
    lua_pop(L, 1);

    // Optional: trade_route_cost (default 500)
    lua_getfield(L, 1, "trade_route_cost");
    int trade_route_cost = lua_isnil(L, -1) ? 500 : (int) lua_tointeger(L, -1);
    lua_pop(L, 1);

    // --- Allocate the empire object for the city ---
    full_empire_object *city_obj = empire_object_get_new();
    if (!city_obj) {
        log_error("Lua: create_city failed - out of empire object memory", 0, 0);
        lua_pushnil(L);
        return 1;
    }

    city_obj->in_use = 1;
    city_obj->obj.type = EMPIRE_OBJECT_CITY;
    city_obj->city_type = city_type;
    city_obj->trade_route_cost = trade_route_cost;
    city_obj->obj.x = x;
    city_obj->obj.y = y;
    city_obj->obj.width = 45;
    city_obj->obj.height = 45;

    if (name) {
        string_copy(string_from_ascii(name), city_obj->city_custom_name,
            sizeof(city_obj->city_custom_name));
    }

    int city_object_id = (int) city_obj->obj.id;

    // --- For trade / future-trade cities, create a trade route object ---
    int route_id = -1;
    if (city_type == EMPIRE_CITY_TRADE || city_type == EMPIRE_CITY_FUTURE_TRADE) {
        full_empire_object *route_obj = empire_object_get_new();
        if (!route_obj) {
            log_error("Lua: create_city failed - cannot allocate trade route object", 0, 0);
            lua_pushnil(L);
            return 1;
        }
        route_obj->in_use = 1;
        route_obj->obj.type = is_sea_route
            ? EMPIRE_OBJECT_SEA_TRADE_ROUTE
            : EMPIRE_OBJECT_LAND_TRADE_ROUTE;

        route_id = trade_route_new();
        route_obj->obj.trade_route_id = route_id;

        // Re-fetch city_obj pointer because array may have grown
        city_obj = empire_object_get_full(city_object_id);
    }

    // --- Allocate the runtime empire_city ---
    empire_city *city = empire_city_get_new();
    if (!city) {
        log_error("Lua: create_city failed - out of empire city memory", 0, 0);
        lua_pushnil(L);
        return 1;
    }

    city->in_use = 1;
    city->type = city_type;
    city->cost_to_open = trade_route_cost;
    city->is_sea_trade = is_sea_route;
    city->trader_entry_delay = 4;
    city->trader_figure_ids[0] = 0;
    city->trader_figure_ids[1] = 0;
    city->trader_figure_ids[2] = 0;
    city->empire_object_id = city_object_id;

    if (route_id >= 0) {
        city->route_id = route_id;
        city_obj->obj.trade_route_id = route_id;
    }

    // --- Process buys table ---
    lua_getfield(L, 1, "buys");
    if (lua_istable(L, -1)) {
        int n = (int) lua_rawlen(L, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, -1, i);
            int res = (int) lua_tointeger(L, -1);
            lua_pop(L, 1);
            if (res > 0 && res < RESOURCE_MAX) {
                city->buys_resource[res] = 1;
                city_obj->city_buys_resource[res] = 1;
                if (route_id >= 0) {
                    trade_route_set(route_id, res, RESOURCE_BUYS);
                }
            }
        }
    }
    lua_pop(L, 1);

    // --- Process sells table ---
    lua_getfield(L, 1, "sells");
    if (lua_istable(L, -1)) {
        int n = (int) lua_rawlen(L, -1);
        for (int i = 1; i <= n; i++) {
            lua_rawgeti(L, -1, i);
            int res = (int) lua_tointeger(L, -1);
            lua_pop(L, 1);
            if (res > 0 && res < RESOURCE_MAX) {
                city->sells_resource[res] = 1;
                city_obj->city_sells_resource[res] = 1;
                if (route_id >= 0) {
                    trade_route_set(route_id, res, RESOURCE_SELLS);
                }
            }
        }
    }
    lua_pop(L, 1);

    lua_pushinteger(L, city_object_id);
    return 1;
}

// ---------------------------------------------------------------------------
// Function registration table
// ---------------------------------------------------------------------------

static const luaL_Reg empire_funcs[] = {
    // City queries
    {"city_type",            l_empire_city_type},
    {"city_is_open",         l_empire_city_is_open},
    {"city_route_id",        l_empire_city_route_id},
    {"city_sells_resource",  l_empire_city_sells_resource},
    {"city_buys_resource",   l_empire_city_buys_resource},
    {"is_route_open",        l_empire_is_route_open},
    {"is_sea_trade",         l_empire_is_sea_trade},
    {"trade_route_cost",     l_empire_trade_route_cost},
    {"set_trade_route_cost", l_empire_set_trade_route_cost},
    {"open_trade",           l_empire_open_trade},
    {"count_trade_routes",   l_empire_count_trade_routes},
    {"count_wine_sources",   l_empire_count_wine_sources},
    {"can_import",           l_empire_can_import},
    {"can_export",           l_empire_can_export},
    {"can_import_from_city", l_empire_can_import_from_city},
    {"can_export_to_city",   l_empire_can_export_to_city},
    // Trade routes
    {"route_limit",          l_empire_route_limit},
    {"route_traded",         l_empire_route_traded},
    {"set_route_limit",      l_empire_set_route_limit},
    {"route_limit_reached",  l_empire_route_limit_reached},
    // Trade prices
    {"buy_price",            l_empire_buy_price},
    {"sell_price",           l_empire_sell_price},
    {"base_buy_price",       l_empire_base_buy_price},
    {"base_sell_price",      l_empire_base_sell_price},
    {"change_price",         l_empire_change_price},
    {"set_buy_price",        l_empire_set_buy_price},
    {"set_sell_price",       l_empire_set_sell_price},
    // Objects
    {"object_count",         l_empire_object_count},
    {"selected_object",      l_empire_selected_object},
    // City creation
    {"create_city",          l_empire_create_city},
    {NULL, NULL}
};

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void lua_api_empire_register(lua_State *L)
{
    luaL_newlib(L, empire_funcs);

    // empire.CITY_TYPE.* constants
    lua_newtable(L);
    lua_pushinteger(L, EMPIRE_CITY_DISTANT_ROMAN);
    lua_setfield(L, -2, "DISTANT_ROMAN");
    lua_pushinteger(L, EMPIRE_CITY_OURS);
    lua_setfield(L, -2, "OURS");
    lua_pushinteger(L, EMPIRE_CITY_TRADE);
    lua_setfield(L, -2, "TRADE");
    lua_pushinteger(L, EMPIRE_CITY_FUTURE_TRADE);
    lua_setfield(L, -2, "FUTURE_TRADE");
    lua_pushinteger(L, EMPIRE_CITY_DISTANT_FOREIGN);
    lua_setfield(L, -2, "DISTANT_FOREIGN");
    lua_pushinteger(L, EMPIRE_CITY_VULNERABLE_ROMAN);
    lua_setfield(L, -2, "VULNERABLE_ROMAN");
    lua_pushinteger(L, EMPIRE_CITY_FUTURE_ROMAN);
    lua_setfield(L, -2, "FUTURE_ROMAN");
    lua_setfield(L, -2, "CITY_TYPE");

    // empire.OBJECT_TYPE.* constants
    lua_newtable(L);
    lua_pushinteger(L, EMPIRE_OBJECT_ORNAMENT);
    lua_setfield(L, -2, "ORNAMENT");
    lua_pushinteger(L, EMPIRE_OBJECT_CITY);
    lua_setfield(L, -2, "CITY");
    lua_pushinteger(L, EMPIRE_OBJECT_BATTLE_ICON);
    lua_setfield(L, -2, "BATTLE_ICON");
    lua_pushinteger(L, EMPIRE_OBJECT_LAND_TRADE_ROUTE);
    lua_setfield(L, -2, "LAND_TRADE_ROUTE");
    lua_pushinteger(L, EMPIRE_OBJECT_SEA_TRADE_ROUTE);
    lua_setfield(L, -2, "SEA_TRADE_ROUTE");
    lua_pushinteger(L, EMPIRE_OBJECT_ROMAN_ARMY);
    lua_setfield(L, -2, "ROMAN_ARMY");
    lua_pushinteger(L, EMPIRE_OBJECT_ENEMY_ARMY);
    lua_setfield(L, -2, "ENEMY_ARMY");
    lua_pushinteger(L, EMPIRE_OBJECT_TRADE_WAYPOINT);
    lua_setfield(L, -2, "TRADE_WAYPOINT");
    lua_pushinteger(L, EMPIRE_OBJECT_BORDER);
    lua_setfield(L, -2, "BORDER");
    lua_pushinteger(L, EMPIRE_OBJECT_BORDER_EDGE);
    lua_setfield(L, -2, "BORDER_EDGE");
    lua_setfield(L, -2, "OBJECT_TYPE");

    lua_setglobal(L, "empire");
}
