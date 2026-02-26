---@meta augustus
-- Augustus Lua Scripting API - Finance APIs
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

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
