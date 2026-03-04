---@meta augustus
-- Augustus Lua Scripting API - City API
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

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

--- Change prosperity rating by amount (positive or negative)
---@param amount integer
function city.change_prosperity(amount) end

--- Get current peace rating
---@return integer rating
function city.rating_peace() end

--- Change peace rating by amount (positive or negative)
---@param amount integer
function city.change_peace(amount) end

--- Get current favor rating
---@return integer rating
function city.rating_favor() end

--- Change Caesar's favor by amount (positive or negative)
---@param amount integer
function city.change_favor(amount) end

--- Get Rome's wages
---@return integer wages
function city.rome_wages() end

--- Change Rome's wages by amount (positive or negative)
---@param amount integer
function city.change_rome_wages(amount) end

--- Set Rome's wages to a specific value
---@param amount integer
function city.set_rome_wages(amount) end
