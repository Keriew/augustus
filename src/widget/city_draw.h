#pragma once

#include "graphics/color.h"

struct building;

// Input: a building pointer that may reference any part of a multi-tile building plus the tile draw origin.
// Output: draws the native runtime footprint when this stage is owned by the native path and returns 1 in that case.
// Returning 0 means the caller should continue with its existing fallback stage logic.
int city_draw_runtime_building_footprint(building *b, int x, int y, color_t color_mask, float scale);

// Input: a building pointer that may reference any part of a multi-tile building plus the tile draw origin.
// Output: draws the native runtime top when this stage is owned by the native path and returns 1 in that case.
// Returning 0 means the caller should continue with its existing fallback stage logic.
int city_draw_runtime_building_top(building *b, int x, int y, color_t color_mask, float scale);

// Input: a building pointer, the tile draw origin, and the grid offset for the owning draw tile.
// Output: draws the native runtime animation stage when native graphics own that stage and returns 1 in that case.
// Returning 0 means the caller should continue with its existing fallback stage logic.
int city_draw_runtime_building_animation(building *b, int x, int y, int grid_offset, color_t color_mask, float scale);

// Input: a terrain grid offset plus the tile draw origin.
// Output: draws the native runtime terrain footprint for that tile and returns 1 when a native tile slice exists.
// Returning 0 means the caller should continue with its existing fallback stage logic.
int city_draw_runtime_tile_footprint(int grid_offset, int x, int y, color_t color_mask, float scale);

// Input: a building pointer that may reference any part of a multi-tile building.
// Output: returns 1 when the native runtime path claims authoritative building graphics for that building.
int city_draw_runtime_owns_building_graphics(building *b);

// Input: a building pointer that may reference any part of a multi-tile building.
// Output: returns 1 when the native runtime path claims authoritative building animation for that building.
int city_draw_runtime_owns_building_animation(building *b);
