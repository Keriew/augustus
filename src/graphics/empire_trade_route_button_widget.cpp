#include "graphics/empire_trade_route_button_widget.h"

#include "core/crash_context.h"
#include "graphics/bordered_button_widget.h"
#include "graphics/ui_sprite_primitive.h"

EmpireTradeRouteButtonWidget::EmpireTradeRouteButtonWidget(
    UiPrimitives &primitives,
    int x,
    int y,
    int width_pixels,
    int height_pixels,
    int has_focus,
    EmpireTradeRouteButtonSpec spec,
    color_t color)
    : ButtonWidget(primitives, x, y)
    , width_pixels_(width_pixels)
    , height_pixels_(height_pixels)
    , has_focus_(has_focus)
    , spec_(spec)
    , color_(color)
{
}

void EmpireTradeRouteButtonWidget::draw() const
{
    ErrorContextScope error_scope("ui.empire_trade_route_button");
    BorderedButtonWidget(
        primitives_,
        x_,
        y_,
        width_pixels_,
        height_pixels_,
        has_focus_,
        color_,
        BorderedButtonFillStyle::Transparent)
        .draw();
    UiTextPrimitive(primitives_, spec_.cost_text).draw();
    if (spec_.draw_label) {
        UiTextPrimitive(primitives_, spec_.label_text).draw();
    }
    if (spec_.icon_image_id > 0) {
        UiSpritePrimitive(primitives_, spec_.icon_image_id, spec_.icon_x, spec_.icon_y, COLOR_MASK_NONE).draw();
    }
}
