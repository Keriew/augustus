#include "graphics/bordered_button_widget.h"

#include "graphics/ui_border_primitive.h"

BorderedButtonWidget::BorderedButtonWidget(
    UiPrimitives &primitives,
    int x,
    int y,
    int width_pixels,
    int height_pixels,
    int has_focus,
    color_t color)
    : ButtonWidget(primitives, x, y)
    , width_pixels_(width_pixels)
    , height_pixels_(height_pixels)
    , has_focus_(has_focus)
    , color_(color)
{
}

void BorderedButtonWidget::draw() const
{
    UiBorderPrimitive(primitives_, x_, y_, width_pixels_, height_pixels_, has_focus_, color_).draw();
}
