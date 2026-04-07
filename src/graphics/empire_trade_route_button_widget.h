#pragma once

#include "graphics/button_widget.h"
#include "graphics/color.h"
#include "graphics/ui_text_primitive.h"

struct EmpireTradeRouteButtonSpec {
    UiTextSpec cost_text;
    UiTextSpec label_text;
    int draw_label = 0;
    int icon_image_id = 0;
    int icon_x = 0;
    int icon_y = 0;
};

class EmpireTradeRouteButtonWidget : public ButtonWidget {
public:
    EmpireTradeRouteButtonWidget(
        UiPrimitives &primitives,
        int x,
        int y,
        int width_pixels,
        int height_pixels,
        int has_focus,
        EmpireTradeRouteButtonSpec spec,
        color_t color = COLOR_MASK_NONE);

    void draw() const;

private:
    int width_pixels_;
    int height_pixels_;
    int has_focus_;
    EmpireTradeRouteButtonSpec spec_;
    color_t color_;
};
