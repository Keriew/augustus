#include "widget/city_draw_overlay.h"

#include "widget/city_draw.h"

int city_draw_overlay_runtime_building_footprint(building *b, int x, int y, color_t color_mask, float scale)
{
    return city_draw_runtime_building_footprint(b, x, y, color_mask, scale);
}

int city_draw_overlay_runtime_building_top(building *b, int x, int y, color_t color_mask, float scale)
{
    return city_draw_runtime_building_top(b, x, y, color_mask, scale);
}

int city_draw_overlay_runtime_building_animation(building *b, int x, int y, int grid_offset, color_t color_mask, float scale)
{
    return city_draw_runtime_building_animation(b, x, y, grid_offset, color_mask, scale);
}

int city_draw_overlay_runtime_tile_footprint(int grid_offset, int x, int y, color_t color_mask, float scale)
{
    return city_draw_runtime_tile_footprint(grid_offset, x, y, color_mask, scale);
}
