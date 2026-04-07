#pragma once

#include "graphics/ui_primitive.h"

struct UiBorderedButtonSkin;

class UiBorderPrimitive : public UiPrimitive {
public:
    UiBorderPrimitive(
        UiPrimitives &primitives,
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        color_t color = COLOR_MASK_NONE);

    void draw() const;

private:
    int width_blocks() const;
    int height_blocks() const;
    int right_edge_x(int block_width) const;
    int bottom_edge_y(int block_height) const;
    void draw_top_edge(const UiBorderedButtonSkin &skin, int block_width) const;
    void draw_bottom_edge(const UiBorderedButtonSkin &skin, int block_width, int block_height) const;
    void draw_left_edge(const UiBorderedButtonSkin &skin, int block_height) const;
    void draw_right_edge(const UiBorderedButtonSkin &skin, int block_width, int block_height) const;

    int x_;
    int y_;
    int width_pixels_;
    int height_pixels_;
    int has_focus_;
    color_t color_;
};
