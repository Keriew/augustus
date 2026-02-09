---@meta augustus
-- Augustus Lua Scripting API - Conditions
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

----------------------------------------------------------------
-- condition.*  –  Evaluate built-in scenario conditions
-- All functions return boolean. Use COMPARE constants for comparison parameter.
----------------------------------------------------------------

---@class condition
condition = {}

--- Check if enough game time has passed
---@param comparison integer COMPARE constant
---@param months integer Total months to compare against
---@return boolean met
function condition.time_passed(comparison, months) end

--- Check the current difficulty setting
---@param comparison integer COMPARE constant
---@param value integer Difficulty value (0=very easy, 1=easy, 2=normal, 3=hard, 4=very hard)
---@return boolean met
function condition.difficulty(comparison, value) end

--- Check treasury balance
---@param comparison integer COMPARE constant
---@param value integer Amount of denarii
---@return boolean met
function condition.money(comparison, value) end

--- Check emperor's personal savings
---@param comparison integer COMPARE constant
---@param value integer Amount of denarii
---@return boolean met
function condition.savings(comparison, value) end

--- Check Caesar's favor rating
---@param comparison integer COMPARE constant
---@param value integer Favor value
---@return boolean met
function condition.favor(comparison, value) end

--- Check prosperity rating
---@param comparison integer COMPARE constant
---@param value integer Prosperity value
---@return boolean met
function condition.prosperity(comparison, value) end

--- Check culture rating
---@param comparison integer COMPARE constant
---@param value integer Culture value
---@return boolean met
function condition.culture(comparison, value) end

--- Check peace rating
---@param comparison integer COMPARE constant
---@param value integer Peace value
---@return boolean met
function condition.peace(comparison, value) end

--- Check city health value
---@param comparison integer COMPARE constant
---@param value integer Health value
---@return boolean met
function condition.city_health(comparison, value) end

--- Check Rome's wages
---@param comparison integer COMPARE constant
---@param value integer Wage value
---@return boolean met
function condition.rome_wages(comparison, value) end

--- Check current tax rate
---@param comparison integer COMPARE constant
---@param value integer Tax rate percentage
---@return boolean met
function condition.tax_rate(comparison, value) end

--- Check city population
---@param comparison integer COMPARE constant
---@param value integer Population value
---@param pop_class? integer POP_CLASS constant (default POP_CLASS.ALL)
---@return boolean met
function condition.city_population(comparison, value, pop_class) end

--- Check unemployment
---@param use_percentage boolean If true, compares percentage; if false, compares absolute count
---@param comparison integer COMPARE constant
---@param value integer Unemployment value
---@return boolean met
function condition.unemployment(use_percentage, comparison, value) end

--- Check trade sell price of a resource
---@param resource_type integer Resource id
---@param comparison integer COMPARE constant
---@param value integer Price value
---@return boolean met
function condition.trade_sell_price(resource_type, comparison, value) end

--- Check count of active (working) buildings of a type
---@param comparison integer COMPARE constant
---@param value integer Count to compare against
---@param building_type integer Building type (see BUILDING constants)
---@return boolean met
function condition.building_count_active(comparison, value, building_type) end

--- Check count of any buildings of a type (including non-working)
---@param comparison integer COMPARE constant
---@param value integer Count to compare against
---@param building_type integer Building type (see BUILDING constants)
---@return boolean met
function condition.building_count_any(comparison, value, building_type) end

--- Check count of buildings of a type within an area
---@param grid_offset1 integer Corner 1 of area
---@param grid_offset2 integer Corner 2 of area
---@param building_type integer Building type
---@param comparison integer COMPARE constant
---@param value integer Count to compare against
---@return boolean met
function condition.building_count_area(grid_offset1, grid_offset2, building_type, comparison, value) end

--- Check count of own troops
---@param comparison integer COMPARE constant
---@param value integer Soldier count
---@param in_city_only? boolean Only count soldiers in city (default false)
---@return boolean met
function condition.count_own_troops(comparison, value, in_city_only) end

--- Check a custom variable against a value
---@param variable_id integer Custom variable id
---@param comparison integer COMPARE constant
---@param value integer Value to compare against
---@return boolean met
function condition.custom_variable_check(variable_id, comparison, value) end

--- Check if a request is ongoing
---@param request_id integer Request id
---@param check_for_ongoing boolean If true, checks request IS ongoing; if false, checks it is NOT ongoing
---@return boolean met
function condition.request_is_ongoing(request_id, check_for_ongoing) end

--- Check if a trade route is open
---@param route_id integer Trade route id
---@param check_for_open boolean If true, checks route IS open; if false, checks it is NOT open
---@return boolean met
function condition.trade_route_open(route_id, check_for_open) end

--- Check trade route opening price
---@param route_id integer Trade route id
---@param comparison integer COMPARE constant
---@param value integer Price value
---@return boolean met
function condition.trade_route_price(route_id, comparison, value) end

--- Check amount of a resource stored
---@param resource_type integer Resource id
---@param comparison integer COMPARE constant
---@param value integer Amount to compare against
---@param storage_type? integer 0=all, 1=granaries, 2=warehouses (default 0)
---@return boolean met
function condition.resource_stored_count(resource_type, comparison, value, storage_type) end

--- Check available storage space for a resource
---@param resource_type integer Resource id
---@param comparison integer COMPARE constant
---@param value integer Space to compare against
---@param storage_type? integer 0=all, 1=granaries, 2=warehouses (default 0)
---@return boolean met
function condition.resource_storage_available(resource_type, comparison, value, storage_type) end

--- Check count of terrain tiles in an area
---@param grid_offset1 integer Corner 1 of area
---@param grid_offset2 integer Corner 2 of area
---@param terrain_type integer Terrain flag
---@param comparison integer COMPARE constant
---@param value integer Count to compare against
---@return boolean met
function condition.terrain_in_area(grid_offset1, grid_offset2, terrain_type, comparison, value) end

----------------------------------------------------------------
-- Constants: Comparison types (for condition.* functions)
----------------------------------------------------------------

---@enum COMPARE
COMPARE = {
    EQUAL         = 1,
    EQUAL_OR_LESS = 2,
    EQUAL_OR_MORE = 3,
    NOT_EQUAL     = 4,
    LESS_THAN     = 5,
    GREATER_THAN  = 6,
}

----------------------------------------------------------------
-- Constants: Scenario event condition types
----------------------------------------------------------------

---@enum CONDITION
CONDITION = {
    TIME_PASSED                = 1,
    DIFFICULTY                 = 2,
    MONEY                      = 3,
    SAVINGS                    = 4,
    STATS_FAVOR                = 5,
    STATS_PROSPERITY           = 6,
    STATS_CULTURE              = 7,
    STATS_PEACE                = 8,
    TRADE_SELL_PRICE           = 9,
    POPS_UNEMPLOYMENT          = 10,
    ROME_WAGES                 = 11,
    CITY_POPULATION            = 12,
    BUILDING_COUNT_ACTIVE      = 13,
    STATS_CITY_HEALTH          = 14,
    COUNT_OWN_TROOPS           = 15,
    REQUEST_IS_ONGOING         = 16,
    TAX_RATE                   = 17,
    BUILDING_COUNT_ANY         = 18,
    CUSTOM_VARIABLE_CHECK      = 19,
    TRADE_ROUTE_OPEN           = 20,
    TRADE_ROUTE_PRICE          = 21,
    RESOURCE_STORED_COUNT      = 22,
    RESOURCE_STORAGE_AVAILABLE = 23,
    BUILDING_COUNT_AREA        = 24,
    CHECK_FORMULA              = 25,
    TERRAIN_IN_AREA            = 26,
    LUA_CUSTOM                 = 27,
}
