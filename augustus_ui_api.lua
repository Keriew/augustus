---@meta augustus
-- Augustus Lua Scripting API - UI APIs
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

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

---@class ui_custom_message_params
---@field title? string Message title
---@field subtitle? string Message subtitle
---@field text? string Main message body text
---@field background_image? string Background image filename
---@field background_music? string Background music filename
---@field sound? string Sound effect filename
---@field speech? string Speech/narration filename

--- Show a custom message dialog with optional media.
--- All fields are optional. Returns true on success, false on failure.
---@param params ui_custom_message_params
---@return boolean success
function ui.show_custom_message(params) end

---@class ui_input_dialog_button
---@field label string Button label text
---@field on_click? fun() Optional callback when button is clicked

---@class ui_input_dialog_params
---@field title? string Dialog title
---@field subtitle? string Dialog subtitle
---@field text? string Main dialog body text
---@field image? string Image filepath (displayed centered above text)
---@field buttons ui_input_dialog_button[] Array of buttons (max 4, vertically stacked)

--- Show a modal input dialog with buttons. The dialog cannot be closed
--- with ESC or a close button - the player must click one of the buttons.
--- If a button has no on_click callback, clicking it simply closes the dialog.
--- Returns true on success, false on failure.
---@param params ui_input_dialog_params
---@return boolean success
function ui.input_dialog(params) end
