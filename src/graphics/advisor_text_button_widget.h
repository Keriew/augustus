#pragma once

#include "graphics/button_widget.h"
#include "graphics/color.h"
#include "graphics/ui_text_primitive.h"

class AdvisorTextButtonWidget : public ButtonWidget {
public:
    AdvisorTextButtonWidget(
        UiPrimitives &primitives,
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        UiTextSpec text_spec,
        color_t color = COLOR_MASK_NONE);

    void draw() const;

private:
    int width_pixels_;
    int height_pixels_;
    int has_focus_;
    UiTextSpec text_spec_;
    color_t color_;
};
