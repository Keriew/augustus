---@meta augustus
-- Augustus Lua Scripting API - Building API
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

----------------------------------------------------------------
-- building.*  –  Building queries and manipulation
----------------------------------------------------------------

---@class building_api
building = {}

--- Count active (working) buildings of the given type
---@param type integer BUILDING type constant
---@return integer count
function building.count_active(type) end

--- Count buildings of the given type within a rectangular area
---@param type integer BUILDING type constant
---@param x1 integer Min x tile coordinate
---@param y1 integer Min y tile coordinate
---@param x2 integer Max x tile coordinate
---@param y2 integer Max y tile coordinate
---@return integer count
function building.count_in_area(type, x1, y1, x2, y2) end

--- Count upgraded buildings of the given type
---@param type integer BUILDING type constant
---@return integer count
function building.count_upgraded(type) end

--- Get the building type for a building id
---@param building_id integer
---@return integer|nil type BUILDING type constant, or nil if invalid
function building.get_type(building_id) end

--- Get the state of a building
---@param building_id integer
---@return integer|nil state building.STATE constant, or nil if invalid
function building.get_state(building_id) end

--- Set a building on fire
---@param building_id integer
---@return boolean success
function building.set_fire(building_id) end

--- Destroy a building (collapse)
---@param building_id integer
---@return boolean success
function building.destroy(building_id) end

--- Building state constants (accessible as building.STATE.IN_USE etc.)
---@class building_STATE
building.STATE = {}

---@enum BUILDING_STATE
BUILDING_STATE = {
    BUILDING_STATE_UNUSED = 0,
    BUILDING_STATE_IN_USE = 1,
    BUILDING_STATE_UNDO = 2,
    BUILDING_STATE_CREATED = 3,
    BUILDING_STATE_RUBBLE = 4,
    BUILDING_STATE_DELETED_BY_GAME = 5,
    BUILDING_STATE_DELETED_BY_PLAYER = 6,
    BUILDING_STATE_MOTHBALLED = 7
}

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
