---@meta augustus
-- Augustus Lua Scripting API - Empire API
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

----------------------------------------------------------------
-- empire.*  –  Empire map, trade routes, and trade prices
----------------------------------------------------------------

---@class empire_api
empire = {}

----------------------------------------------------------------
-- City queries
----------------------------------------------------------------

--- Get the city type of an empire city
---@param city_id integer
---@return integer|nil type empire.CITY_TYPE constant, or nil if invalid
function empire.city_type(city_id) end

--- Check whether an empire city's trade route is open
---@param city_id integer
---@return boolean is_open
function empire.city_is_open(city_id) end

--- Get the trade route id for an empire city
---@param city_id integer
---@return integer route_id
function empire.city_route_id(city_id) end

--- Check if an empire city sells a specific resource
---@param city_id integer
---@param resource integer RESOURCE constant
---@return boolean
function empire.city_sells_resource(city_id, resource) end

--- Check if an empire city buys a specific resource
---@param city_id integer
---@param resource integer RESOURCE constant
---@return boolean
function empire.city_buys_resource(city_id, resource) end

--- Check whether a trade route is open by route id
---@param route_id integer
---@return boolean
function empire.is_route_open(route_id) end

--- Check whether a trade route is a sea trade route
---@param route_id integer
---@return boolean
function empire.is_sea_trade(route_id) end

--- Get the cost to open a trade route
---@param route_id integer
---@return integer cost
function empire.trade_route_cost(route_id) end

--- Set the cost to open a trade route
---@param route_id integer
---@param cost integer New cost value
function empire.set_trade_route_cost(route_id, cost) end

--- Open a trade route with an empire city
---@param city_id integer
---@param apply_cost? boolean Whether to deduct the cost from treasury (default false)
function empire.open_trade(city_id, apply_cost) end

--- Count the number of trade routes matching criteria
---@param is_sea boolean Count sea routes (false = land routes)
---@param is_open boolean Count only open routes (false = closed routes)
---@return integer count
function empire.count_trade_routes(is_sea, is_open) end

--- Count the number of wine sources available in the empire
---@return integer count
function empire.count_wine_sources() end

--- Check whether a resource can currently be imported (any open route)
---@param resource integer RESOURCE constant
---@return boolean
function empire.can_import(resource) end

--- Check whether a resource can currently be exported (any open route)
---@param resource integer RESOURCE constant
---@return boolean
function empire.can_export(resource) end

--- Check whether a resource can be imported from a specific city
---@param city_id integer
---@param resource integer RESOURCE constant
---@return boolean
function empire.can_import_from_city(city_id, resource) end

--- Check whether a resource can be exported to a specific city
---@param city_id integer
---@param resource integer RESOURCE constant
---@return boolean
function empire.can_export_to_city(city_id, resource) end

----------------------------------------------------------------
-- City attribute setters
----------------------------------------------------------------

--- Set the type of an empire city
---@param city_id integer
---@param type integer empire.CITY_TYPE constant
function empire.set_city_type(city_id, type) end

--- Mark an empire city as vulnerable roman
---@param city_id integer
function empire.set_city_vulnerable(city_id) end

--- Mark an empire city as distant foreign
---@param city_id integer
function empire.set_city_foreign(city_id) end

--- Expand the empire: converts FUTURE_TRADE cities to TRADE, FUTURE_ROMAN to DISTANT_ROMAN
function empire.expand_empire() end

--- Toggle whether a city sells a specific resource
---@param city_id integer
---@param resource integer RESOURCE constant
---@param enabled boolean true to enable selling, false to disable
function empire.set_city_sells(city_id, resource, enabled) end

--- Toggle whether a city buys a specific resource
---@param city_id integer
---@param resource integer RESOURCE constant
---@param enabled boolean true to enable buying, false to disable
function empire.set_city_buys(city_id, resource, enabled) end

--- Make all resources available from our city
---@return boolean success
function empire.unlock_all_resources() end

--- Toggle resource availability for our own city
---@param resource integer RESOURCE constant
---@param available boolean true to make available, false to remove
---@return boolean success
function empire.set_own_resource(resource, available) end

----------------------------------------------------------------
-- Trade route quantities
----------------------------------------------------------------

--- Get the trade limit for a resource on a route
---@param route_id integer
---@param resource integer RESOURCE constant
---@return integer limit
function empire.route_limit(route_id, resource) end

--- Get the amount traded so far this year for a resource on a route
---@param route_id integer
---@param resource integer RESOURCE constant
---@return integer traded
function empire.route_traded(route_id, resource) end

--- Set the trade limit for a resource on a route
---@param route_id integer
---@param resource integer RESOURCE constant
---@param amount integer New limit value
function empire.set_route_limit(route_id, resource, amount) end

--- Check if the trade limit has been reached for a resource on a route
---@param route_id integer
---@param resource integer RESOURCE constant
---@return boolean
function empire.route_limit_reached(route_id, resource) end

----------------------------------------------------------------
-- Trade prices
----------------------------------------------------------------

--- Get the buy price for a resource (with trade modifiers applied)
---@param resource integer RESOURCE constant
---@param is_land? boolean True for land trader price, false/nil for sea (default false)
---@return integer price
function empire.buy_price(resource, is_land) end

--- Get the sell price for a resource (with trade modifiers applied)
---@param resource integer RESOURCE constant
---@param is_land? boolean True for land trader price, false/nil for sea (default false)
---@return integer price
function empire.sell_price(resource, is_land) end

--- Get the base buy price for a resource (no modifiers)
---@param resource integer RESOURCE constant
---@return integer price
function empire.base_buy_price(resource) end

--- Get the base sell price for a resource (no modifiers)
---@param resource integer RESOURCE constant
---@return integer price
function empire.base_sell_price(resource) end

--- Change the trade price for a resource by a given amount
---@param resource integer RESOURCE constant
---@param amount integer Can be positive or negative
---@return boolean success Whether the price was changed
function empire.change_price(resource, amount) end

--- Set the buy price of a resource to a specific value
---@param resource integer RESOURCE constant
---@param price integer New buy price
---@return boolean success Whether the price was changed
function empire.set_buy_price(resource, price) end

--- Set the sell price of a resource to a specific value
---@param resource integer RESOURCE constant
---@param price integer New sell price
---@return boolean success Whether the price was changed
function empire.set_sell_price(resource, price) end

----------------------------------------------------------------
-- Empire objects – queries
----------------------------------------------------------------

--- Get the total number of empire objects
---@return integer count
function empire.object_count() end

--- Get the id of the currently selected empire object
---@return integer object_id
function empire.selected_object() end

--- Get the type of an empire object
---@param object_id integer
---@return integer|nil type empire.OBJECT_TYPE constant, or nil if invalid
function empire.object_type(object_id) end

--- Get the position of an empire object
---@param object_id integer
---@return integer|nil x X coordinate, or nil if invalid
---@return integer|nil y Y coordinate (only returned when x is non-nil)
function empire.object_position(object_id) end

--- Get the invasion path id of an empire object
---@param object_id integer
---@return integer|nil path_id Invasion path id, or nil if invalid
function empire.object_invasion_path(object_id) end

----------------------------------------------------------------
-- Empire object creation
----------------------------------------------------------------

--- Create a new empire object on the empire map.
--- Accepts a table with the following fields:
---   type                         (integer) — empire.OBJECT_TYPE constant (required)
---   x                            (integer) — x position on empire map (default 0)
---   y                            (integer) — y position on empire map (default 0)
---   width                        (integer) — object width (default 0)
---   height                       (integer) — object height (default 0)
---   image_id                     (integer) — image id (default 0, auto-set for some types)
---
--- Type-specific fields:
---   BORDER:
---     density                    (integer) — spacing between edge flags (default 50)
---     edges                      (table)   — array of {x, y, hidden?} edge definitions
---   BATTLE_ICON:
---     invasion_path_id           (integer) — invasion path this battle belongs to
---     invasion_years             (integer) — year offset in invasion sequence
---   ROMAN_ARMY / ENEMY_ARMY:
---     distant_battle_travel_months (integer) — travel time in months
---   LAND_TRADE_ROUTE / SEA_TRADE_ROUTE:
---     trade_route_id             (integer) — associated trade route id
---   BORDER_EDGE:
---     hidden                     (boolean) — if true, border flag is invisible
---
--- Returns the empire object id of the newly created object, or nil on failure.
---@param opts table
---@return integer|nil object_id
function empire.create_object(opts) end

----------------------------------------------------------------
-- Invasion path creation
----------------------------------------------------------------

--- Create a new invasion path with a sequence of battle icons.
--- Each entry in the array defines a battle location: { x = integer, y = integer }
--- Battle icons are created with invasion_years set automatically (first = farthest, last = 1).
--- Returns the invasion path id, or nil on failure.
---@param battles {x: integer, y: integer}[]
---@return integer|nil path_id
function empire.create_invasion_path(battles) end

----------------------------------------------------------------
-- City creation
----------------------------------------------------------------

--- Create a new city on the empire map.
--- Accepts a table with the following fields:
---   name             (string)  — city name (required)
---   x                (integer) — x position on empire map (required)
---   y                (integer) — y position on empire map (required)
---   type             (integer) — empire.CITY_TYPE constant (default TRADE)
---   trade_route_type (string)  — "land" or "sea" (default "land")
---   trade_route_cost (integer) — cost to open the trade route (default 500)
---   buys             (table)   — array of RESOURCE constants the city buys
---   sells            (table)   — array of RESOURCE constants the city sells
--- Returns the empire object id of the newly created city, or nil on failure.
---@param opts {name: string, x: integer, y: integer, type?: integer, trade_route_type?: string, trade_route_cost?: integer, buys?: integer[], sells?: integer[]}
---@return integer|nil object_id
function empire.create_city(opts) end

----------------------------------------------------------------
-- Constants
----------------------------------------------------------------

--- Empire city type constants (accessible as empire.CITY_TYPE.TRADE etc.)
---@class empire_CITY_TYPE
empire.CITY_TYPE = {}

---@enum EMPIRE_CITY_TYPE
EMPIRE_CITY_TYPE = {
    DISTANT_ROMAN    = 0,
    OURS             = 1,
    TRADE            = 2,
    FUTURE_TRADE     = 3,
    DISTANT_FOREIGN  = 4,
    VULNERABLE_ROMAN = 5,
    FUTURE_ROMAN     = 6,
}

--- Empire object type constants (accessible as empire.OBJECT_TYPE.CITY etc.)
---@class empire_OBJECT_TYPE
empire.OBJECT_TYPE = {}

---@enum EMPIRE_OBJECT_TYPE
EMPIRE_OBJECT_TYPE = {
    ORNAMENT         = 0,
    CITY             = 1,
    BATTLE_ICON      = 3,
    LAND_TRADE_ROUTE = 4,
    SEA_TRADE_ROUTE  = 5,
    ROMAN_ARMY       = 6,
    ENEMY_ARMY       = 7,
    TRADE_WAYPOINT   = 8,
    BORDER           = 9,
    BORDER_EDGE      = 10,
}
