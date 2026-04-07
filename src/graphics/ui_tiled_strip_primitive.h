#ifndef GRAPHICS_UI_TILED_STRIP_PRIMITIVE_H
#define GRAPHICS_UI_TILED_STRIP_PRIMITIVE_H

#include "graphics/runtime_texture.h"
#include "graphics/ui_primitive.h"

#include <vector>

class UiTiledStripPrimitive : public UiPrimitive {
public:
    UiTiledStripPrimitive(
        UiPrimitives &primitives,
        int start_image_id,
        int middle_image_id,
        int end_image_id,
        int x,
        int y,
        int segment_count,
        int logical_tile_width,
        int logical_tile_height,
        color_t color = COLOR_MASK_NONE,
        int middle_pattern_length = 1);
    UiTiledStripPrimitive(
        UiPrimitives &primitives,
        RuntimeDrawSlice start_slice,
        std::vector<RuntimeDrawSlice> middle_slices,
        RuntimeDrawSlice end_slice,
        int x,
        int y,
        int segment_count,
        int logical_tile_width,
        int logical_tile_height,
        color_t color = COLOR_MASK_NONE);

    void draw() const;

private:
    int tile_width() const;
    int tile_height() const;
    int use_runtime_slices() const;

    int start_image_id_;
    int middle_image_id_;
    int end_image_id_;
    RuntimeDrawSlice start_slice_;
    std::vector<RuntimeDrawSlice> middle_slices_;
    RuntimeDrawSlice end_slice_;
    int x_;
    int y_;
    int segment_count_;
    int logical_tile_width_;
    int logical_tile_height_;
    color_t color_;
    int middle_pattern_length_;
};

#endif // GRAPHICS_UI_TILED_STRIP_PRIMITIVE_H
