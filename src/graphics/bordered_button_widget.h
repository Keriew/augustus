#pragma once

#include "graphics/button_widget.h"

enum class BorderedButtonFillStyle {
    PanelTexture,
    Transparent,
};

enum class BorderedButtonIconPlacement {
    None,
    ExplicitOffset,
};

struct BorderedButtonIconSpec {
    int image_id = 0;
    BorderedButtonIconPlacement placement = BorderedButtonIconPlacement::None;
    int x_offset = 0;
    int y_offset = 0;
    int logical_width = 0;
    int logical_height = 0;
    color_t color = COLOR_MASK_NONE;
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
        BorderedButtonFillStyle fill_style = BorderedButtonFillStyle::PanelTexture,
        BorderedButtonIconSpec icon = {});

    void draw() const;

private:
    void draw_container() const;
    void draw_fill() const;
    void draw_border() const;
    void draw_icon() const;
    int width_blocks() const;
    int height_blocks() const;

    int width_pixels_;
    int height_pixels_;
    int has_focus_;
    color_t color_;
    BorderedButtonFillStyle fill_style_;
    BorderedButtonIconSpec icon_;
};
