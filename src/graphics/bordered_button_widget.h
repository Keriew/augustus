#pragma once

#include "graphics/button_widget.h"

class BorderedButtonWidget : public ButtonWidget {
public:
    BorderedButtonWidget(
        UiPrimitives &primitives,
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        color_t color);

    void draw() const;

private:
    void draw_container() const;
    void draw_fill() const;
    void draw_border() const;
    int width_blocks() const;
    int height_blocks() const;

    int width_pixels_;
    int height_pixels_;
    int has_focus_;
    color_t color_;
};
