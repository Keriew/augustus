#include "graphics/advisor_text_button_widget.h"

#include "core/crash_context.h"
#include "graphics/bordered_button_widget.h"

AdvisorTextButtonWidget::AdvisorTextButtonWidget(
    UiPrimitives &primitives,
    int x,
    int y,
    int width_pixels,
    int height_pixels,
    int has_focus,
    UiTextSpec text_spec,
    color_t color)
    : ButtonWidget(primitives, x, y)
    , width_pixels_(width_pixels)
    , height_pixels_(height_pixels)
    , has_focus_(has_focus)
    , text_spec_(text_spec)
    , color_(color)
{
}

void AdvisorTextButtonWidget::draw() const
{
    ErrorContextScope error_scope("ui.advisor_text_button");
    BorderedButtonWidget(primitives_, x_, y_, width_pixels_, height_pixels_, has_focus_, color_).draw();
    UiTextPrimitive(primitives_, text_spec_).draw();
}
