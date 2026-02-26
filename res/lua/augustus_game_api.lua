---@meta augustus
-- Augustus Lua Scripting API - Game API
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

--- Get the Lua API version supported by this build of Augustus.
--- Use this to check compatibility and guard against unsupported features.
---@return integer version
function game.api_version() end
