---@meta augustus
-- Augustus Lua Scripting API - Sound APIs
-- Place this file in your workspace for IDE autocomplete. Do NOT load it in scripts.

----------------------------------------------------------------
-- sound.*  –  Sound effects and music
----------------------------------------------------------------

---@class sound_api
sound = {}

--- Play a sound effect
---@param id integer sound.EFFECT constant
function sound.play_effect(id) end

--- Play a music track
---@param track integer sound.TRACK constant
function sound.play_music(track) end

--- Sound effect constants (accessible as sound.EFFECT.FANFARE etc.)
---@class sound_EFFECT
---@field EXPLOSION integer
---@field FANFARE integer
---@field FANFARE_URGENT integer
---@field ARROW integer
---@field SWORD integer
---@field CLUB integer
---@field MARCHING integer
---@field WOLF_HOWL integer
---@field FIRE_SPLASH integer
---@field FORMATION_SHIELD integer
---@field ELEPHANT integer
---@field LION_ATTACK integer
---@field BALLISTA_SHOOT integer
sound.EFFECT = {}

--- Music track constants (accessible as sound.TRACK.CITY_1 etc.)
---@class sound_TRACK
---@field CITY_1 integer
---@field CITY_2 integer
---@field CITY_3 integer
---@field CITY_4 integer
---@field CITY_5 integer
---@field COMBAT_SHORT integer
---@field COMBAT_LONG integer
sound.TRACK = {}
