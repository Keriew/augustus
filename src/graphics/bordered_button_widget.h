#pragma once

#include "graphics/button_widget.h"

enum class BorderedButtonFillStyle {
    PanelTexture,
    Transparent,
};

class BorderedButtonWidget : public ButtonWidget {
public:
    BorderedButtonWidget(
        UiPrimitives &primitives,
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        color_t color,
        BorderedButtonFillStyle fill_style = BorderedButtonFillStyle::PanelTexture);

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
    BorderedButtonFillStyle fill_style_;
};
