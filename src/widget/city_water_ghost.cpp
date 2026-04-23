#include "city_water_ghost.h"

#include "building/water_access_runtime.h"

extern "C" {
#include "city/view.h"
#include "widget/city_building_ghost.h"
}

namespace {

void draw_water_structure_access(int x, int y, int grid_offset)
{
    if (!water_access_runtime_should_draw_overlay_at(grid_offset)) {
        return;
    }
    if (water_access_runtime_tile_has_access(grid_offset, WATER_ACCESS_RUNTIME_TYPE_FOUNTAIN)) {
        city_building_ghost_draw_fountain_range(x, y, grid_offset);
    } else if (water_access_runtime_tile_has_access(grid_offset, WATER_ACCESS_RUNTIME_TYPE_WELL)) {
        city_building_ghost_draw_well_range(x, y, grid_offset);
    }
}

void draw_reservoir_access(int x, int y, int grid_offset)
{
    if (!water_access_runtime_should_draw_overlay_at(grid_offset)) {
        return;
    }
    if (water_access_runtime_tile_has_access(grid_offset, WATER_ACCESS_RUNTIME_TYPE_RESERVOIR)) {
        city_building_ghost_draw_reservoir_range(x, y, grid_offset);
    }
}

void draw_preview_access(int x, int y, int grid_offset)
{
    if (!water_access_runtime_should_draw_overlay_at(grid_offset)) {
        return;
    }
    if (water_access_runtime_tile_has_preview_highlight(grid_offset)) {
        city_building_ghost_draw_fountain_range(x, y, grid_offset);
    }
}

} // namespace

extern "C" void city_water_ghost_draw_water_structure_ranges(void)
{
    city_view_foreach_valid_map_tile(draw_water_structure_access);
}

extern "C" void city_water_ghost_draw_reservoir_ranges(void)
{
    city_view_foreach_valid_map_tile(draw_reservoir_access);
}

extern "C" void city_water_ghost_draw_preview(building_type type, int primary_grid_offset, int secondary_grid_offset)
{
    water_access_runtime_begin_preview(type, primary_grid_offset, secondary_grid_offset);
    city_view_foreach_valid_map_tile(draw_preview_access);
    water_access_runtime_end_preview();
}
