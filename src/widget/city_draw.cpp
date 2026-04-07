#include "widget/city_draw.h"

#include "building/building_runtime_graphics.h"
#include "core/crash_context.h"
#include "graphics/runtime_texture.h"
#include "map/tile_runtime_graphics.h"

extern "C" {
#include "building/building.h"
#include "map/property.h"
}

#include <cstdio>

namespace {

building *get_main_building(building *b)
{
    return b ? building_main(b) : nullptr;
}

int main_building_owns_runtime_graphics(building *b)
{
    return b ? building_runtime_owns_graphics(b) : 0;
}

int main_building_owns_runtime_animation(building *b)
{
    return b ? building_runtime_owns_graphic_animation(b) : 0;
}

void log_missing_building_stage_slice(const char *stage, building *b)
{
    char detail[256];
    if (b) {
        snprintf(
            detail,
            sizeof(detail),
            "building_id=%u type=%d grid_offset=%d stage=%s",
            b->id,
            static_cast<int>(b->type),
            b->grid_offset,
            stage ? stage : "");
    } else {
        snprintf(detail, sizeof(detail), "building=null stage=%s", stage ? stage : "");
    }

    error_context_report_error("Native building draw stage had no drawable slice. Falling back to legacy rendering.", detail);
}

}

int city_draw_runtime_owns_building_graphics(building *b)
{
    b = get_main_building(b);
    return main_building_owns_runtime_graphics(b);
}

int city_draw_runtime_owns_building_animation(building *b)
{
    b = get_main_building(b);
    return main_building_owns_runtime_animation(b);
}

int city_draw_runtime_building_footprint(building *b, int x, int y, int grid_offset, color_t color_mask, float scale)
{
    b = get_main_building(b);
    if (!main_building_owns_runtime_graphics(b)) {
        return 0;
    }
    if (!map_property_is_draw_tile(grid_offset)) {
        return 1;
    }

    const RuntimeDrawSlice *slice = building_runtime_get_graphic_footprint_slice(b);
    if (!slice) {
        log_missing_building_stage_slice("footprint", b);
        return 0;
    }

    runtime_texture_draw(*slice, x, y, color_mask, scale);
    return 1;
}

int city_draw_runtime_building_top(building *b, int x, int y, color_t color_mask, float scale)
{
    b = get_main_building(b);
    if (!main_building_owns_runtime_graphics(b)) {
        return 0;
    }

    const RuntimeDrawSlice *slice = building_runtime_get_graphic_top_slice(b);
    if (slice) {
        runtime_texture_draw(*slice, x, y, color_mask, scale);
    }
    return 1;
}

int city_draw_runtime_building_animation(building *b, int x, int y, int grid_offset, color_t color_mask, float scale)
{
    if (!map_property_is_draw_tile(grid_offset) || !b) {
        return 0;
    }

    b = get_main_building(b);
    if (!main_building_owns_runtime_graphics(b)) {
        return 0;
    }

    if (!main_building_owns_runtime_animation(b)) {
        return 1;
    }

    const RuntimeDrawSlice *frame = building_runtime_get_graphic_animation_slice(b);
    if (frame) {
        runtime_texture_draw(*frame, x, y, color_mask, scale);
    }
    return 1;
}

int city_draw_runtime_tile_footprint(int grid_offset, int x, int y, color_t color_mask, float scale)
{
    const RuntimeDrawSlice *slice = tile_runtime_get_graphic_footprint_slice(grid_offset);
    if (!slice) {
        return 0;
    }

    runtime_texture_draw(*slice, x, y, color_mask, scale);
    return 1;
}
