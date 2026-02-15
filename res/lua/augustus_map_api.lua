---@meta augustus
-- Augustus Lua Scripting API - Map API
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

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
