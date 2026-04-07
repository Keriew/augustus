#pragma once

#include "graphics/button_widget.h"
#include "graphics/color.h"
#include "graphics/ui_text_primitive.h"

class AdvisorCardButtonWidget : public ButtonWidget {
public:
    AdvisorCardButtonWidget(
        UiPrimitives &primitives,
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        const UiTextSpec *text_specs,
        int text_spec_count,
        color_t color = COLOR_MASK_NONE);

    void draw() const;

private:
    int width_pixels_;
    int height_pixels_;
    int has_focus_;
    const UiTextSpec *text_specs_;
    int text_spec_count_;
    color_t color_;
};
