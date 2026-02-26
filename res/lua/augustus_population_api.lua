---@meta augustus
-- Augustus Lua Scripting API - Population API
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

----------------------------------------------------------------
-- population.*  –  Population demographics
----------------------------------------------------------------

---@class population_api
population = {}

--- Get number of people of school age
---@return integer count
function population.school_age() end

--- Get number of people of academy age
---@return integer count
function population.academy_age() end

--- Get number of people of working age
---@return integer count
function population.working_age() end

--- Get number of retired people
---@return integer count
function population.retired() end

--- Get number of births this year
---@return integer count
function population.births() end

--- Get number of deaths this year
---@return integer count
function population.deaths() end

--- Get average age of the population
---@return integer age
function population.average_age() end

---@enum POP_CLASS
POP_CLASS = {
    ALL       = 1,
    PATRICIAN = 2,
    PLEBEIAN  = 3,
    SLUMS     = 4,
}
