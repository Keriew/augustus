---@meta augustus
-- Augustus Lua Scripting API - Actions

----------------------------------------------------------------
-- action.*  –  Execute built-in scenario actions with named parameters
----------------------------------------------------------------

---@class action
action = {}

--- Adjust Caesar's favor by amount (positive or negative)
---@param amount integer
---@return boolean success
function action.adjust_favor(amount) end

--- Add or subtract denarii from the treasury
---@param amount integer Positive to add, negative to subtract
---@return boolean success
function action.adjust_money(amount) end

--- Add or subtract from emperor's personal savings
---@param amount integer
---@return boolean success
function action.adjust_savings(amount) end

--- Adjust or set city health
---@param amount integer Health value
---@param hard_set? boolean If true, sets health to amount; if false/nil, adjusts by amount
---@return boolean success
function action.adjust_city_health(amount, hard_set) end

--- Trigger a gladiator revolt in the city
---@return boolean success
function action.gladiator_revolt() end

--- Allow or disallow a building type
---@param building_id integer Building type (see BUILDING constants)
---@param allowed boolean True to allow, false to disallow
---@return boolean success
function action.change_allowed_building(building_id, allowed) end

--- Enable or disable production of a resource
---@param resource_type integer Resource id
---@param enabled boolean True to enable, false to disable
---@return boolean success
function action.change_resource_produced(resource_type, enabled) end

--- Add or remove resource stockpiles
---@param resource_type integer Resource id
---@param amount integer Positive to add, negative to remove
---@param storage_type? integer 0=all, 1=granaries, 2=warehouses (default 0)
---@param respect_settings? boolean Respect storage limits (default false)
---@return boolean success
function action.change_resource_stockpiles(resource_type, amount, storage_type, respect_settings) end

--- Send a predefined standard message (see MESSAGE constants)
---@param text_id integer Message type id
---@return boolean success
function action.send_standard_message(text_id) end

--- Show a custom scenario message
---@param message_id integer Custom message id
---@return boolean success
function action.show_custom_message(message_id) end

--- Set the city tax rate
---@param rate integer Tax rate percentage
---@return boolean success
---@deprecated
function action.tax_rate_set(rate) end

--- Trigger a blessing from a god
---@param god_id integer God id (1=Ceres, 2=Neptune, 3=Mercury, 4=Mars, 5=Venus)
---@return boolean success
function action.cause_blessing(god_id) end

--- Trigger a minor curse from a god
---@param god_id integer God id (1=Ceres, 2=Neptune, 3=Mercury, 4=Mars, 5=Venus)
---@return boolean success
function action.cause_minor_curse(god_id) end

--- Trigger a major curse from a god
---@param god_id integer God id (1=Ceres, 2=Neptune, 3=Mercury, 4=Mars, 5=Venus)
---@return boolean success
function action.cause_major_curse(god_id) end

--- Change the map climate
---@param climate_type integer See CLIMATE constants (0=central, 1=northern, 2=desert)
---@return boolean success
function action.change_climate(climate_type) end

--- Change a god's sentiment/happiness
---@param god_id integer God id (1=Ceres, 2=Neptune, 3=Mercury, 4=Mars, 5=Venus)
---@param amount integer Sentiment value
---@param hard_set? boolean If true, sets sentiment; if false/nil, adjusts by amount
---@return boolean success
function action.god_sentiment_change(god_id, amount, hard_set) end

--- Change population sentiment/happiness
---@param amount integer Sentiment value
---@param hard_set? boolean If true, sets sentiment; if false/nil, adjusts by amount
---@return boolean success
function action.pop_sentiment_change(amount, hard_set) end

--- Force a scenario victory
---@return boolean success
function action.win() end

--- Force a scenario defeat
---@return boolean success
function action.lose() end

--- Change the player's rank
---@param rank integer New rank value
---@return boolean success
function action.change_rank(rank) end

--- Change production rate for a resource
---@param resource_type integer Resource id
---@param rate integer Production rate per month
---@param hard_set? boolean If true, sets rate; if false/nil, adjusts by rate
---@return boolean success
function action.change_production_rate(resource_type, rate, hard_set) end

--- Trigger an immediate invasion
---@param attack_type integer Type of invasion/attack
---@param size integer Number of enemy units
---@param invasion_point? integer Map invasion point (default 0)
---@param target_type? integer Target type (default 0)
---@param enemy_id? integer Enemy identifier (default 0)
---@return boolean success
function action.invasion(attack_type, size, invasion_point, target_type, enemy_id) end

--- Force collapse of buildings in an area
---@param grid_offset1 integer Corner 1 of area
---@param grid_offset2 integer Corner 2 of area
---@param building_type? integer Building type to collapse (default 0 = any)
---@param destroy_all? boolean If true, destroys all buildings regardless of type
---@return boolean success
function action.building_force_collapse(grid_offset1, grid_offset2, building_type, destroy_all) end

--- Adjust a resource's trade price
---@param resource_type integer Resource id
---@param amount integer Price adjustment
---@param show_message? boolean Show price change message (default false)
---@return boolean success
function action.trade_adjust_price(resource_type, amount, show_message) end

--- Set a resource's trade price (buy and sell)
---@param resource_type integer Resource id
---@param price integer New price
---@return boolean success
function action.trade_set_price(resource_type, price) end

--- Set a resource's buy price only
---@param resource_type integer Resource id
---@param price integer New buy price
---@return boolean success
function action.trade_set_buy_price(resource_type, price) end

--- Set a resource's sell price only
---@param resource_type integer Resource id
---@param price integer New sell price
---@return boolean success
function action.trade_set_sell_price(resource_type, price) end

--- Open a trade route
---@param route_id integer Trade route id
---@param apply_cost? boolean Apply opening cost (default false)
---@return boolean success
function action.trade_route_set_open(route_id, apply_cost) end

--- Adjust trade route amount for a resource
---@param route_id integer Trade route id
---@param resource_type integer Resource id
---@param amount integer Amount adjustment
---@return boolean success
function action.trade_adjust_route_amount(route_id, resource_type, amount) end

--- Add a new resource to a trade route
---@param route_id integer Trade route id
---@param resource_type integer Resource id
---@return boolean success
function action.trade_route_add_resource(route_id, resource_type) end

--- Adjust the opening price of a trade route
---@param route_id integer Trade route id
---@param amount integer Price adjustment
---@return boolean success
function action.trade_adjust_route_open_price(route_id, amount) end

--- Cause land trade problems
---@return boolean success
function action.trade_problem_land() end

--- Cause sea trade problems
---@return boolean success
function action.trade_problem_sea() end

--- Lock a trade route
---@param route_id integer Trade route id
---@return boolean success
function action.lock_trade_route(route_id) end

--- Convert a future trade city on the empire map
---@param city_id integer Empire city id
---@return boolean success
function action.empire_map_convert_future_trade_city(city_id) end

--- Start a request immediately
---@param request_id integer Request id
---@return boolean success
function action.request_immediately_start(request_id) end

----------------------------------------------------------------
-- Constants: Scenario event action types
----------------------------------------------------------------

---@enum ACTION
ACTION = {
    ADJUST_FAVOR                         = 1,
    ADJUST_MONEY                         = 2,
    ADJUST_SAVINGS                       = 3,
    TRADE_ADJUST_PRICE                   = 4,
    TRADE_PROBLEM_LAND                   = 5,
    TRADE_PROBLEM_SEA                    = 6,
    TRADE_ADJUST_ROUTE_AMOUNT            = 7,
    ADJUST_ROME_WAGES                    = 8,
    GLADIATOR_REVOLT                     = 9,
    CHANGE_RESOURCE_PRODUCED             = 10,
    CHANGE_ALLOWED_BUILDINGS             = 11,
    SEND_STANDARD_MESSAGE                = 12,
    ADJUST_CITY_HEALTH                   = 13,
    TRADE_SET_PRICE                      = 14,
    EMPIRE_MAP_CONVERT_FUTURE_TRADE_CITY = 15,
    REQUEST_IMMEDIATELY_START            = 16,
    SHOW_CUSTOM_MESSAGE                  = 17,
    TAX_RATE_SET                         = 18,
    CHANGE_CUSTOM_VARIABLE               = 19,
    TRADE_ADJUST_ROUTE_OPEN_PRICE        = 20,
    CHANGE_CITY_RATING                   = 21,
    CHANGE_RESOURCE_STOCKPILES           = 22,
    TRADE_ROUTE_SET_OPEN                 = 23,
    TRADE_ROUTE_ADD_NEW_RESOURCE         = 24,
    TRADE_SET_BUY_PRICE_ONLY             = 25,
    TRADE_SET_SELL_PRICE_ONLY            = 26,
    BUILDING_FORCE_COLLAPSE              = 27,
    INVASION_IMMEDIATE                   = 28,
    CAUSE_BLESSING                       = 29,
    CAUSE_MINOR_CURSE                    = 30,
    CAUSE_MAJOR_CURSE                    = 31,
    CHANGE_CLIMATE                       = 32,
    CHANGE_TERRAIN                       = 33,
    CHANGE_CUSTOM_VARIABLE_VISIBILITY    = 34,
    CUSTOM_VARIABLE_FORMULA              = 35,
    CUSTOM_VARIABLE_CITY_PROPERTY        = 36,
    GOD_SENTIMENT_CHANGE                 = 37,
    POP_SENTIMENT_CHANGE                 = 38,
    WIN                                  = 39,
    LOSE                                 = 40,
    CHANGE_RANK                          = 41,
    CHANGE_MODEL_DATA                    = 42,
    CHANGE_PRODUCTION_RATE               = 43,
    LOCK_TRADE_ROUTE                     = 44,
    LUA_CUSTOM                           = 45,
}
