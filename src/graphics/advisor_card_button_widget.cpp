#include "graphics/advisor_card_button_widget.h"

#include "core/crash_context.h"
#include "graphics/bordered_button_widget.h"

AdvisorCardButtonWidget::AdvisorCardButtonWidget(
    UiPrimitives &primitives,
    int x,
    int y,
    int width_pixels,
    int height_pixels,
    int has_focus,
    const UiTextSpec *text_specs,
    int text_spec_count,
    color_t color)
    : ButtonWidget(primitives, x, y)
    , width_pixels_(width_pixels)
    , height_pixels_(height_pixels)
    , has_focus_(has_focus)
    , text_specs_(text_specs)
    , text_spec_count_(text_spec_count)
    , color_(color)
{
}

void AdvisorCardButtonWidget::draw() const
{
    ErrorContextScope error_scope("ui.advisor_card_button");
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
    for (int i = 0; i < text_spec_count_; ++i) {
        UiTextPrimitive(primitives_, text_specs_[i]).draw();
    }
}
