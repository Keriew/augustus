#ifndef GRAPHICS_UI_TILED_STRIP_PRIMITIVE_H
#define GRAPHICS_UI_TILED_STRIP_PRIMITIVE_H

#include "graphics/ui_primitive.h"

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

    void draw() const;

private:
    int tile_width() const;
    int tile_height() const;

    int start_image_id_;
    int middle_image_id_;
    int end_image_id_;
    int x_;
    int y_;
    int segment_count_;
    int logical_tile_width_;
    int logical_tile_height_;
    color_t color_;
    int middle_pattern_length_;
};

#endif // GRAPHICS_UI_TILED_STRIP_PRIMITIVE_H
