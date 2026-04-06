#pragma once

#include "graphics/color.h"

struct building;

// Input: the same footprint stage inputs used by the overlay city renderer.
// Output: forwards native runtime footprint ownership/drawing for overlay mode and returns 1 when native handled the stage.
int city_draw_overlay_runtime_building_footprint(building *b, int x, int y, int grid_offset, color_t color_mask, float scale);

// Input: the same top stage inputs used by the overlay city renderer.
// Output: forwards native runtime top ownership/drawing for overlay mode and returns 1 when native handled the stage.
int city_draw_overlay_runtime_building_top(building *b, int x, int y, color_t color_mask, float scale);

// Input: the same animation stage inputs used by the overlay city renderer.
// Output: forwards native runtime animation ownership/drawing for overlay mode and returns 1 when native handled the stage.
int city_draw_overlay_runtime_building_animation(building *b, int x, int y, int grid_offset, color_t color_mask, float scale);

// Input: the same terrain footprint stage inputs used by the overlay city renderer.
// Output: forwards native runtime tile drawing for overlay mode and returns 1 when native handled the stage.
int city_draw_overlay_runtime_tile_footprint(int grid_offset, int x, int y, color_t color_mask, float scale);
