---@meta augustus
-- Augustus Lua Scripting API - Core APIs
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

----------------------------------------------------------------
-- scenario.*  –  Scenario properties, variables, and events
----------------------------------------------------------------

---@class scenario
scenario = {}

--- Get the scenario name
---@return string name
function scenario.name() end

--- Get the scenario start year
---@return integer year
function scenario.start_year() end

--- Get the scenario climate (0=central, 1=northern, 2=desert)
---@return integer climate
function scenario.climate() end

--- Change the scenario climate (reloads terrain graphics)
---@param id integer Climate id: 0=central, 1=northern, 2=desert (see CLIMATE constants)
function scenario.change_climate(id) end

--- Get a custom variable value by id or name
---@param id_or_name integer|string Variable id (integer) or name (string)
---@return integer value
function scenario.get_variable(id_or_name) end

--- Set a custom variable value by id or name
---@param id_or_name integer|string Variable id (integer) or name (string)
---@param value integer
function scenario.set_variable(id_or_name, value) end

--- Create a new custom variable
---@param name string Variable name
---@param initial_value? integer Initial value (default 0)
---@return integer id The new variable id
function scenario.create_variable(name, initial_value) end

--- Register a custom condition function (for use with create_event)
---@param name string Unique condition name
---@param fn fun(): boolean Callback that returns true when condition is met
---@return integer index Condition index
function scenario.register_condition(name, fn) end

--- Register a custom action function (for use with create_event)
---@param name string Unique action name
---@param fn fun() Callback executed when event fires
---@return integer index Action index
function scenario.register_action(name, fn) end

--- Get the total number of scenario events
---@return integer count
function scenario.event_count() end

---@class create_event_params
---@field repeat_min? integer Minimum months between repeats (default 0)
---@field repeat_max? integer Maximum months between repeats (default 0)
---@field max_repeats? integer Maximum number of times event can fire (default 0 = unlimited)
---@field condition? string Name of a registered condition
---@field action? string Name of a registered action

--- Create a new scenario event with a registered condition and action
---@param params create_event_params
---@return integer|nil event_id Event id, or nil on failure
function scenario.create_event(params) end

--- Execute a built-in scenario action directly (see ACTION constants)
---@param action_type integer Action type from ACTION enum
---@param p1? integer Parameter 1 (default 0)
---@param p2? integer Parameter 2 (default 0)
---@param p3? integer Parameter 3 (default 0)
---@param p4? integer Parameter 4 (default 0)
---@param p5? integer Parameter 5 (default 0)
---@return boolean success
function scenario.execute_action(action_type, p1, p2, p3, p4, p5) end

--@enum GODS
GODS = {
    CERES = 1,
    NEPTUNE = 2,
    MERCURY = 3,
    MARS = 4,
    VENUS = 5,
}

----------------------------------------------------------------
-- Constants: Climate
----------------------------------------------------------------

---@enum CLIMATE
CLIMATE = {
    CENTRAL  = 0,
    NORTHERN = 1,
    DESERT   = 2,
}

----------------------------------------------------------------
-- Constants: City message types (for ui.post_message)
----------------------------------------------------------------

---@enum MESSAGE
MESSAGE = {
    POPULATION_500                  = 2,
    POPULATION_1000                 = 3,
    POPULATION_2000                 = 4,
    POPULATION_3000                 = 5,
    POPULATION_5000                 = 6,
    POPULATION_10000                = 7,
    POPULATION_15000                = 8,
    POPULATION_20000                = 9,
    POPULATION_25000                = 10,
    RIOT                            = 11,
    FIRE                            = 12,
    COLLAPSED_BUILDING              = 13,
    DESTROYED_BUILDING              = 14,
    NAVIGATION_IMPOSSIBLE           = 15,
    CITY_IN_DEBT                    = 16,
    CITY_IN_DEBT_AGAIN              = 17,
    CITY_STILL_IN_DEBT              = 18,
    CAESAR_WRATH                    = 19,
    CAESAR_ARMY_CONTINUE            = 20,
    CAESAR_ARMY_RETREAT             = 21,
    LOCAL_UPRISING                  = 22,
    BARBARIAN_ATTACK                = 23,
    CAESAR_ARMY_ATTACK              = 24,
    DISTANT_BATTLE                  = 25,
    ENEMIES_CLOSING                 = 26,
    ENEMIES_AT_THE_DOOR             = 27,
    CAESAR_REQUESTS_GOODS           = 28,
    CAESAR_REQUESTS_MONEY           = 29,
    CAESAR_REQUESTS_ARMY            = 30,
    REQUEST_REMINDER                = 31,
    REQUEST_RECEIVED                = 32,
    REQUEST_REFUSED                 = 33,
    REQUEST_REFUSED_OVERDUE         = 34,
    REQUEST_RECEIVED_LATE           = 35,
    UNEMPLOYMENT                    = 36,
    WORKERS_NEEDED                  = 37,
    SMALL_FESTIVAL                  = 38,
    LARGE_FESTIVAL                  = 39,
    GRAND_FESTIVAL                  = 40,
    WRATH_OF_CERES                  = 41,
    WRATH_OF_NEPTUNE_NO_SEA_TRADE   = 42,
    WRATH_OF_MERCURY                = 43,
    WRATH_OF_MARS_NO_MILITARY       = 44,
    WRATH_OF_VENUS                  = 45,
    PEOPLE_DISGRUNTLED              = 46,
    PEOPLE_UNHAPPY                  = 47,
    PEOPLE_ANGRY                    = 48,
    NOT_ENOUGH_FOOD                 = 49,
    FOOD_NOT_DELIVERED              = 50,
    THEFT                           = 52,
    GODS_UNHAPPY                    = 55,
    EARTHQUAKE                      = 62,
    GLADIATOR_REVOLT                = 63,
    EMPEROR_CHANGE                  = 64,
    LAND_TRADE_DISRUPTED_SANDSTORMS = 65,
    SEA_TRADE_DISRUPTED             = 66,
    LAND_TRADE_DISRUPTED_LANDSLIDES = 67,
    ROME_RAISES_WAGES               = 68,
    ROME_LOWERS_WAGES               = 69,
    CONTAMINATED_WATER              = 70,
    IRON_MINE_COLLAPSED             = 71,
    CLAY_PIT_FLOODED                = 72,
    GLADIATOR_REVOLT_FINISHED       = 73,
    INCREASED_TRADING               = 74,
    DECREASED_TRADING               = 75,
    TRADE_STOPPED                   = 76,
    EMPIRE_HAS_EXPANDED             = 77,
    PRICE_INCREASED                 = 78,
    PRICE_DECREASED                 = 79,
    ROAD_TO_ROME_BLOCKED            = 80,
    WRATH_OF_NEPTUNE                = 81,
    WRATH_OF_MARS                   = 82,
    DISTANT_BATTLE_LOST_NO_TROOPS   = 84,
    DISTANT_BATTLE_LOST_TOO_LATE    = 85,
    DISTANT_BATTLE_LOST_TOO_WEAK    = 86,
    DISTANT_BATTLE_WON              = 87,
    TROOPS_RETURN_FAILED            = 88,
    TROOPS_RETURN_VICTORIOUS        = 89,
    DISTANT_BATTLE_CITY_RETAKEN     = 90,
    CERES_IS_UPSET                  = 91,
    NEPTUNE_IS_UPSET                = 92,
    MERCURY_IS_UPSET                = 93,
    MARS_IS_UPSET                   = 94,
    VENUS_IS_UPSET                  = 95,
    BLESSING_FROM_CERES             = 96,
    BLESSING_FROM_NEPTUNE           = 97,
    BLESSING_FROM_MERCURY           = 98,
    BLESSING_FROM_MARS              = 99,
    BLESSING_FROM_VENUS             = 100,
    GODS_WRATHFUL                   = 101,
    HEALTH_ILLNESS                  = 102,
    HEALTH_DISEASE                  = 103,
    HEALTH_PESTILENCE               = 104,
    SPIRIT_OF_MARS                  = 105,
    CAESAR_RESPECT_1                = 106,
    CAESAR_RESPECT_2                = 107,
    CAESAR_RESPECT_3                = 108,
    WORKING_HIPPODROME              = 109,
    WORKING_COLOSSEUM               = 110,
    EMIGRATION                      = 111,
    FIRED                           = 112,
    ENEMY_ARMY_ATTACK               = 114,
    REQUEST_CAN_COMPLY              = 115,
    ROAD_TO_ROME_OBSTRUCTED         = 116,
    NO_WORKING_DOCK                 = 117,
    FISHING_BOAT_BLOCKED            = 118,
    LOCAL_UPRISING_MARS             = 121,
    SOLDIERS_STARVING               = 122,
    SOLDIERS_STARVING_NO_MESS_HALL  = 123,
    GRAND_TEMPLE_COMPLETE           = 124,
    BLESSING_FROM_MERCURY_ALTERNATE = 125,
    PANTHEON_COMPLETE               = 131,
    LIGHTHOUSE_COMPLETE             = 132,
    BLESSING_FROM_NEPTUNE_ALTERNATE = 133,
    BLESSING_FROM_VENUS_ALTERNATE   = 134,
    COLOSSEUM_COMPLETE              = 135,
    HIPPODROME_COMPLETE             = 136,
    LOOTING                         = 151,
    SICKNESS                        = 155,
    CAESAR_ANGER                    = 156,
    WRATH_OF_MARS_NO_NATIVES        = 157,
    ENEMIES_LEAVING                 = 158,
    ROAD_TO_ROME_WARNING            = 159,
    CUSTOM_MESSAGE                  = 160,
    ROUTE_PRICE_CHANGE              = 161,
    CARAVANSERAI_COMPLETE           = 162,
    GOVERNOR_RANK_CHANGE            = 163,
}

----------------------------------------------------------------
-- Hooks  –  Define these functions in your script
----------------------------------------------------------------

--- Called once when the scenario starts (after script is loaded)
function on_load() end

--- Called every game day
function on_tick() end

--- Called at the start of each month
function on_month() end

--- Called at the start of each year
function on_year() end

--- Called when a scenario event fires
---@param event_name string
function on_event(event_name) end

--- Called when the player places a building
---@param building_type integer
---@param x integer Tile x
---@param y integer Tile y
---@param building_id integer
function on_building_placed(building_type, x, y, building_id) end

--- Called when a building is destroyed
---@param building_type integer
---@param x integer Tile x
---@param y integer Tile y
---@param building_id integer
function on_building_destroyed(building_type, x, y, building_id) end

--- Called when the player wins the scenario
function on_victory() end

--- Called when the player loses the scenario
function on_defeat() end

--- Called when an invasion starts
---@param type integer Invasion type
---@param size integer Number of invaders
function on_invasion_start(type, size) end

--- Called when a figure dies in combat
---@param type integer Figure type
---@param x integer Tile x
---@param y integer Tile y
function on_figure_died(type, x, y) end

--- Called when a trade resource is sold to a warehouse
---@param route_id integer Trade route id
---@param resource integer RESOURCE type constant
---@param amount integer Amount traded (1 load)
function on_trade_completed(route_id, resource, amount) end

--- Called when a Caesar's request is fulfilled
---@param id integer Request id
function on_request_fulfilled(id) end

--- Called when a Caesar's request is refused or ignored
---@param id integer Request id
function on_request_failed(id) end

--- Called when a building catches fire
---@param building_id integer
---@param type integer BUILDING type constant
function on_building_fire(building_id, type) end

--- Called when people immigrate or emigrate
---@param count integer Number of people
---@param direction integer 1 = immigration, -1 = emigration
function on_migration(count, direction) end

--- Called when a city rating changes
---@param type integer 0=favor, 1=peace, 2=prosperity
---@param old_value integer Previous rating value
---@param new_value integer New rating value
function on_rating_changed(type, old_value, new_value) end

--- Called when a god becomes angry (wrath bolts increase)
---@param god_id integer God id (0=Ceres, 1=Neptune, 2=Mercury, 3=Mars, 4=Venus)
function on_god_angry(god_id) end

--- Called when combat damage is dealt
---@param attacker_type integer Attacker figure type
---@param defender_type integer Defender figure type
function on_combat(attacker_type, defender_type) end

---@enum DIFFICULTY
DIFFICULTY = {
    VERY_EASY = 0,
    EASY      = 1,
    NORMAL    = 2,
    HARD      = 3,
    VERY_HARD = 4,
}
