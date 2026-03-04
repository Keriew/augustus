---@meta augustus
-- Augustus Lua Scripting API - Resource API
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

----------------------------------------------------------------
-- resource.*  –  Resource and food supply queries
----------------------------------------------------------------

---@class resource_api
resource = {}

--- Get total amount of a resource across all storages
---@param type integer RESOURCE type constant
---@return integer amount
function resource.total(type) end

--- Get total food stored across all granaries
---@return integer amount
function resource.food_stored() end

--- Get estimated months of food supply remaining
---@return integer months
function resource.food_supply_months() end

--- Get food production as percentage of consumption
---@return integer percentage
function resource.food_production_pct() end

--- Get available empty space in granaries for a food type
---@param food_type integer RESOURCE type constant (food)
---@return integer space
function resource.granary_space(food_type) end

--- Get available empty space in warehouses for a resource type
---@param type integer RESOURCE type constant
---@return integer space
function resource.warehouse_space(type) end

---@enum RESOURCE
RESOURCE = {
    NONE       = 0,
    WHEAT      = 1,
    VEGETABLES = 2,
    FRUIT      = 3,
    MEAT       = 4,
    FISH       = 5,
    CLAY       = 6,
    TIMBER     = 7,
    OLIVES     = 8,
    VINES      = 9,
    IRON       = 10,
    MARBLE     = 11,
    GOLD       = 12,
    SAND       = 13,
    STONE      = 14,
    POTTERY    = 15,
    FURNITURE  = 16,
    OIL        = 17,
    WINE       = 18,
    WEAPONS    = 19,
    CONCRETE   = 20,
    BRICKS     = 21,
    DENARII    = 22,
    TROOPS     = 23,
}
