---@meta augustus
-- Augustus Lua Scripting API
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

----------------------------------------------------------------
-- game.*  –  Game time and flow control
----------------------------------------------------------------

---@class game
game = {}

--- Get the current in-game year
---@return integer year
function game.year() end

--- Get the current in-game month (0-11)
---@return integer month
function game.month() end

--- Get the current in-game day (0-15)
---@return integer day
function game.day() end

--- Get the current tick within the day
---@return integer tick
function game.tick() end

--- Get the current game speed setting
---@return integer speed
function game.speed() end

--- Set the game speed
---@param speed integer
function game.set_speed(speed) end

--- Force a scenario victory
function game.win() end

--- Force a scenario defeat
function game.lose() end

----------------------------------------------------------------
-- city.*  –  City stats and ratings
----------------------------------------------------------------

---@class city
city = {}

--- Get current city population
---@return integer population
function city.population() end

--- Get current city health value
---@return integer health
function city.health() end

--- Change city health by amount (positive or negative)
---@param amount integer
function city.change_health(amount) end

--- Get current city sentiment (happiness)
---@return integer sentiment
function city.sentiment() end

--- Change city sentiment by amount (positive or negative)
---@param amount integer
function city.change_sentiment(amount) end

--- Get current culture rating
---@return integer rating
function city.rating_culture() end

--- Get current prosperity rating
---@return integer rating
function city.rating_prosperity() end

--- Get current peace rating
---@return integer rating
function city.rating_peace() end

--- Get current favor rating
---@return integer rating
function city.rating_favor() end

--- Change Caesar's favor by amount (positive or negative)
---@param amount integer
function city.change_favor(amount) end

----------------------------------------------------------------
-- finance.*  –  Treasury and taxes
----------------------------------------------------------------

---@class finance
finance = {}

--- Get current treasury balance (denarii)
---@return integer amount
function finance.treasury() end

--- Add (or subtract) denarii from the treasury
---@param amount integer Positive to add, negative to subtract
function finance.add_treasury(amount) end

--- Get current tax percentage
---@return integer percentage
function finance.tax_percentage() end

--- Set the tax percentage
---@param percentage integer
function finance.set_tax_percentage(percentage) end

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

----------------------------------------------------------------
-- map.*  –  Map queries
----------------------------------------------------------------

---@class map
map = {}

--- Get the map width in tiles
---@return integer width
function map.width() end

--- Get the map height in tiles
---@return integer height
function map.height() end

--- Get terrain flags at a grid offset
---@param grid_offset integer
---@return integer terrain_flags
function map.terrain_at(grid_offset) end

--- Get the building id at a grid offset (0 if none)
---@param grid_offset integer
---@return integer building_id
function map.building_at(grid_offset) end

--- Convert tile x,y to a grid offset
---@param x integer
---@param y integer
---@return integer grid_offset
function map.grid_offset(x, y) end

----------------------------------------------------------------
-- ui.*  –  Logging and in-game messages
----------------------------------------------------------------

---@class ui
ui = {}

--- Log a message (visible in Augustus log output)
---@param message string
function ui.log(message) end

--- Show a warning banner at the top of the screen (lasts ~15 seconds)
---@param text string
function ui.show_warning(text) end

--- Post a city message to the player's message list
---@param message_type integer Message type id
---@param param1? integer Optional parameter 1 (default 0)
---@param param2? integer Optional parameter 2 (default 0)
function ui.post_message(message_type, param1, param2) end

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

--- Adjust or set Rome's wages
---@param amount integer Wage value
---@param hard_set? boolean If true, sets wages to amount; if false/nil, adjusts by amount
---@return boolean success
function action.adjust_rome_wages(amount, hard_set) end

--- Trigger a gladiator revolt in the city
---@return boolean success
function action.gladiator_revolt() end

--- Allow or disallow a building type
---@param building_id integer Building type (see BUILDING constants)
---@param allowed boolean True to allow, false to disallow
---@return boolean success
function action.change_allowed_building(building_id, allowed) end

--- Change a city rating (prosperity or peace)
---@param rating_type integer 0=prosperity, 1=peace
---@param value integer Rating value
---@param hard_set? boolean If true, sets rating; if false/nil, adjusts by value
---@return boolean success
function action.change_city_rating(rating_type, value, hard_set) end

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
-- Constants: Population class (for condition.city_population)
----------------------------------------------------------------

---@enum POP_CLASS
POP_CLASS = {
    ALL       = 1,
    PATRICIAN = 2,
    PLEBEIAN  = 3,
    SLUMS     = 4,
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
-- Constants: Building types
----------------------------------------------------------------

---@enum BUILDING
BUILDING = {
    NONE                    = 0,
    ROAD                    = 5,
    WALL                    = 6,
    DRAGGABLE_RESERVOIR     = 7,
    AQUEDUCT                = 8,
    CLEAR_LAND              = 9,
    HOUSE_VACANT_LOT        = 10,
    HOUSE_SMALL_TENT        = 10,
    HOUSE_LARGE_TENT        = 11,
    HOUSE_SMALL_SHACK       = 12,
    HOUSE_LARGE_SHACK       = 13,
    HOUSE_SMALL_HOVEL       = 14,
    HOUSE_LARGE_HOVEL       = 15,
    HOUSE_SMALL_CASA        = 16,
    HOUSE_LARGE_CASA        = 17,
    HOUSE_SMALL_INSULA      = 18,
    HOUSE_MEDIUM_INSULA     = 19,
    HOUSE_LARGE_INSULA      = 20,
    HOUSE_GRAND_INSULA      = 21,
    HOUSE_SMALL_VILLA       = 22,
    HOUSE_MEDIUM_VILLA      = 23,
    HOUSE_LARGE_VILLA       = 24,
    HOUSE_GRAND_VILLA       = 25,
    HOUSE_SMALL_PALACE      = 26,
    HOUSE_MEDIUM_PALACE     = 27,
    HOUSE_LARGE_PALACE      = 28,
    HOUSE_LUXURY_PALACE     = 29,
    AMPHITHEATER            = 30,
    THEATER                 = 31,
    HIPPODROME              = 32,
    COLOSSEUM               = 33,
    GLADIATOR_SCHOOL        = 34,
    LION_HOUSE              = 35,
    ACTOR_COLONY            = 36,
    CHARIOT_MAKER           = 37,
    PLAZA                   = 38,
    GARDENS                 = 39,
    FORT_LEGIONARIES        = 40,
    SMALL_STATUE            = 41,
    MEDIUM_STATUE           = 42,
    LARGE_STATUE            = 43,
    FORT_JAVELIN            = 44,
    FORT_MOUNTED            = 45,
    DOCTOR                  = 46,
    HOSPITAL                = 47,
    BATHHOUSE               = 48,
    BARBER                  = 49,
    SCHOOL                  = 51,
    ACADEMY                 = 52,
    LIBRARY                 = 53,
    PREFECTURE              = 55,
    TRIUMPHAL_ARCH          = 56,
    GATEHOUSE               = 58,
    TOWER                   = 59,
    SMALL_TEMPLE_CERES      = 60,
    SMALL_TEMPLE_NEPTUNE    = 61,
    SMALL_TEMPLE_MERCURY    = 62,
    SMALL_TEMPLE_MARS       = 63,
    SMALL_TEMPLE_VENUS      = 64,
    LARGE_TEMPLE_CERES      = 65,
    LARGE_TEMPLE_NEPTUNE    = 66,
    LARGE_TEMPLE_MERCURY    = 67,
    LARGE_TEMPLE_MARS       = 68,
    LARGE_TEMPLE_VENUS      = 69,
    MARKET                  = 70,
    GRANARY                 = 71,
    WAREHOUSE               = 72,
    WAREHOUSE_SPACE         = 73,
    SHIPYARD                = 74,
    DOCK                    = 75,
    WHARF                   = 76,
    GOVERNORS_HOUSE         = 77,
    GOVERNORS_VILLA         = 78,
    GOVERNORS_PALACE        = 79,
    MISSION_POST            = 80,
    ENGINEERS_POST          = 81,
    LOW_BRIDGE              = 82,
    SHIP_BRIDGE             = 83,
    SENATE                  = 85,
    FORUM                   = 86,
    NATIVE_HUT              = 88,
    NATIVE_MEETING          = 89,
    RESERVOIR               = 90,
    FOUNTAIN                = 91,
    WELL                    = 92,
    NATIVE_CROPS            = 93,
    MILITARY_ACADEMY        = 94,
    BARRACKS                = 95,
    ORACLE                  = 98,
    BURNING_RUIN            = 99,
    WHEAT_FARM              = 100,
    VEGETABLE_FARM          = 101,
    FRUIT_FARM              = 102,
    OLIVE_FARM              = 103,
    VINES_FARM              = 104,
    PIG_FARM                = 105,
    MARBLE_QUARRY           = 106,
    IRON_MINE               = 107,
    TIMBER_YARD             = 108,
    CLAY_PIT                = 109,
    WINE_WORKSHOP           = 110,
    OIL_WORKSHOP            = 111,
    WEAPONS_WORKSHOP        = 112,
    FURNITURE_WORKSHOP      = 113,
    POTTERY_WORKSHOP        = 114,
    ROADBLOCK               = 115,
    WORKCAMP                = 116,
    GRAND_TEMPLE_CERES      = 117,
    GRAND_TEMPLE_NEPTUNE    = 118,
    GRAND_TEMPLE_MERCURY    = 119,
    GRAND_TEMPLE_MARS       = 120,
    GRAND_TEMPLE_VENUS      = 121,
    SMALL_POND              = 126,
    LARGE_POND              = 127,
    PINE_TREE               = 128,
    FIR_TREE                = 129,
    OAK_TREE                = 130,
    ELM_TREE                = 131,
    FIG_TREE                = 132,
    PLUM_TREE               = 133,
    PALM_TREE               = 134,
    DATE_TREE               = 135,
    PAVILION_BLUE           = 144,
    PAVILION_RED            = 145,
    PAVILION_ORANGE         = 146,
    PAVILION_YELLOW         = 147,
    PAVILION_GREEN          = 148,
    GODDESS_STATUE          = 149,
    SENATOR_STATUE          = 150,
    OBELISK                 = 151,
    PANTHEON                = 152,
    ARCHITECT_GUILD         = 153,
    MESS_HALL               = 154,
    LIGHTHOUSE              = 155,
    TAVERN                  = 158,
    GRAND_GARDEN            = 159,
    ARENA                   = 160,
    HORSE_STATUE            = 161,
    DOLPHIN_FOUNTAIN        = 162,
    HEDGE_DARK              = 163,
    HEDGE_LIGHT             = 164,
    LOOPED_GARDEN_WALL      = 165,
    LEGION_STATUE           = 166,
    DECORATIVE_COLUMN       = 167,
    COLONNADE               = 168,
    LARARIUM                = 169,
    NYMPHAEUM               = 170,
    SMALL_MAUSOLEUM         = 171,
    LARGE_MAUSOLEUM         = 172,
    WATCHTOWER              = 173,
    PALISADE                = 174,
    GARDEN_PATH             = 175,
    CARAVANSERAI            = 176,
    ROOFED_GARDEN_WALL      = 177,
    ROOFED_GARDEN_WALL_GATE = 178,
    HEDGE_GATE_DARK         = 179,
    HEDGE_GATE_LIGHT        = 180,
    PALISADE_GATE           = 181,
    GLADIATOR_STATUE        = 182,
    HIGHWAY                 = 183,
    GOLD_MINE               = 184,
    CITY_MINT               = 185,
    DEPOT                   = 186,
    SAND_PIT                = 187,
    STONE_QUARRY            = 188,
    CONCRETE_MAKER          = 189,
    BRICKWORKS              = 190,
    PANELLED_GARDEN_WALL    = 191,
    PANELLED_GARDEN_GATE    = 192,
    LOOPED_GARDEN_GATE      = 193,
    SHRINE_CERES            = 194,
    SHRINE_NEPTUNE          = 195,
    SHRINE_MERCURY          = 196,
    SHRINE_MARS             = 197,
    SHRINE_VENUS            = 198,
    OVERGROWN_GARDENS       = 201,
    FORT_AUXILIA_INFANTRY   = 202,
    ARMOURY                 = 203,
    FORT_ARCHERS            = 204,
    LATRINES                = 205,
    NATIVE_HUT_ALT          = 206,
    NATIVE_WATCHTOWER       = 207,
    NATIVE_MONUMENT         = 208,
    NATIVE_DECORATION       = 209,
    TYPE_MAX                = 211,
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

----------------------------------------------------------------
-- Constants: Scenario event action types
----------------------------------------------------------------

---@enum ACTION
ACTION = {
    ADJUST_FAVOR                          = 1,
    ADJUST_MONEY                          = 2,
    ADJUST_SAVINGS                        = 3,
    TRADE_ADJUST_PRICE                    = 4,
    TRADE_PROBLEM_LAND                    = 5,
    TRADE_PROBLEM_SEA                     = 6,
    TRADE_ADJUST_ROUTE_AMOUNT             = 7,
    ADJUST_ROME_WAGES                     = 8,
    GLADIATOR_REVOLT                      = 9,
    CHANGE_RESOURCE_PRODUCED              = 10,
    CHANGE_ALLOWED_BUILDINGS              = 11,
    SEND_STANDARD_MESSAGE                 = 12,
    ADJUST_CITY_HEALTH                    = 13,
    TRADE_SET_PRICE                       = 14,
    EMPIRE_MAP_CONVERT_FUTURE_TRADE_CITY  = 15,
    REQUEST_IMMEDIATELY_START             = 16,
    SHOW_CUSTOM_MESSAGE                   = 17,
    TAX_RATE_SET                          = 18,
    CHANGE_CUSTOM_VARIABLE                = 19,
    TRADE_ADJUST_ROUTE_OPEN_PRICE         = 20,
    CHANGE_CITY_RATING                    = 21,
    CHANGE_RESOURCE_STOCKPILES            = 22,
    TRADE_ROUTE_SET_OPEN                  = 23,
    TRADE_ROUTE_ADD_NEW_RESOURCE          = 24,
    TRADE_SET_BUY_PRICE_ONLY              = 25,
    TRADE_SET_SELL_PRICE_ONLY             = 26,
    BUILDING_FORCE_COLLAPSE               = 27,
    INVASION_IMMEDIATE                    = 28,
    CAUSE_BLESSING                        = 29,
    CAUSE_MINOR_CURSE                     = 30,
    CAUSE_MAJOR_CURSE                     = 31,
    CHANGE_CLIMATE                        = 32,
    CHANGE_TERRAIN                        = 33,
    CHANGE_CUSTOM_VARIABLE_VISIBILITY     = 34,
    CUSTOM_VARIABLE_FORMULA               = 35,
    CUSTOM_VARIABLE_CITY_PROPERTY         = 36,
    GOD_SENTIMENT_CHANGE                  = 37,
    POP_SENTIMENT_CHANGE                  = 38,
    WIN                                   = 39,
    LOSE                                  = 40,
    CHANGE_RANK                           = 41,
    CHANGE_MODEL_DATA                     = 42,
    CHANGE_PRODUCTION_RATE                = 43,
    LOCK_TRADE_ROUTE                      = 44,
    LUA_CUSTOM                            = 45,
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
