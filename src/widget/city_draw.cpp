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

void log_missing_building_stage_slice(const char *stage, building *b)
{
    b = get_main_building(b);

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
    return b ? building_runtime_owns_graphics(b) : 0;
}

int city_draw_runtime_owns_building_animation(building *b)
{
    b = get_main_building(b);
    return b ? building_runtime_owns_graphic_animation(b) : 0;
}

int city_draw_runtime_building_footprint(building *b, int x, int y, color_t color_mask, float scale)
{
    if (!city_draw_runtime_owns_building_graphics(b)) {
        return 0;
    }

    b = get_main_building(b);
    const RuntimeDrawSlice *slice = b ? building_runtime_get_graphic_footprint_slice(b) : nullptr;
    if (!slice) {
        log_missing_building_stage_slice("footprint", b);
        return 0;
    }

    runtime_texture_draw(*slice, x, y, color_mask, scale);
    return 1;
}

int city_draw_runtime_building_top(building *b, int x, int y, color_t color_mask, float scale)
{
    if (!city_draw_runtime_owns_building_graphics(b)) {
        return 0;
    }

    b = get_main_building(b);
    const RuntimeDrawSlice *slice = b ? building_runtime_get_graphic_top_slice(b) : nullptr;
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
    if (!city_draw_runtime_owns_building_graphics(b)) {
        return 0;
    }

    if (!city_draw_runtime_owns_building_animation(b)) {
        return 0;
    }

    const int layer_count = building_runtime_get_graphic_animation_layer_count(b);
    int drew_frame = 0;
    for (int layer_index = 0; layer_index < layer_count; layer_index++) {
        const RuntimeDrawSlice *frame = building_runtime_get_graphic_animation_layer_frame(b, layer_index);
        if (!frame) {
            continue;
        }
        runtime_texture_draw(*frame, x, y, color_mask, scale);
        drew_frame = 1;
    }
    return drew_frame;
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
