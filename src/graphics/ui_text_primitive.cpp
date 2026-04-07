#include "graphics/ui_text_primitive.h"

#include "core/crash_context.h"
#include "graphics/lang_text.h"
#include "graphics/text.h"

#include <stdio.h>

namespace {

void report_invalid_text_spec(const UiTextSpec &spec)
{
    char detail[256];
    snprintf(detail, sizeof(detail),
        "content_type=%d alignment=%d group=%d text_id=%d x=%d y=%d box_width=%d font=%d",
        static_cast<int>(spec.content_type),
        static_cast<int>(spec.alignment),
        spec.text_group,
        spec.text_id,
        spec.x,
        spec.y,
        spec.box_width,
        spec.font);
    error_context_report_error("Widget text primitive resolved to no renderable payload", detail);
}

} // namespace

UiTextPrimitive::UiTextPrimitive(UiPrimitives &primitives, UiTextSpec spec)
    : UiPrimitive(primitives)
    , spec_(spec)
{
}

bool UiTextPrimitive::has_renderable_payload() const
{
    switch (spec_.content_type) {
        case UiTextContentType::Language:
            return true;
        case UiTextContentType::Raw:
            return spec_.raw_text != nullptr && spec_.raw_text[0] != '\0';
        case UiTextContentType::Number:
        case UiTextContentType::Amount:
            return true;
        default:
            return false;
    }
}

int UiTextPrimitive::measure_width() const
{
    switch (spec_.content_type) {
        case UiTextContentType::Language:
            return lang_text_get_width(spec_.text_group, spec_.text_id, spec_.font);
        case UiTextContentType::Raw:
            return spec_.raw_text ? text_get_width(spec_.raw_text, spec_.font) : 0;
        case UiTextContentType::Number:
            return text_get_number_width(spec_.value, spec_.prefix, spec_.postfix ? spec_.postfix : "", spec_.font);
        case UiTextContentType::Amount:
            return lang_text_get_amount_width(spec_.text_group, spec_.text_id, spec_.value, spec_.font);
        default:
            return 0;
    }
}

void UiTextPrimitive::draw() const
{
    if (!has_renderable_payload()) {
        report_invalid_text_spec(spec_);
        return;
    }

    const int box_width = spec_.box_width > 0 ? spec_.box_width : measure_width();
    const int draw_x = spec_.alignment == UiTextAlignment::Right ? spec_.x + box_width - measure_width() : spec_.x;

    switch (spec_.content_type) {
        case UiTextContentType::Language:
            if (spec_.alignment == UiTextAlignment::Center) {
                lang_text_draw_centered_colored(
                    spec_.text_group,
                    spec_.text_id,
                    spec_.x,
                    spec_.y,
                    box_width,
                    spec_.font,
                    spec_.color);
            } else if (spec_.alignment == UiTextAlignment::Right) {
                lang_text_draw_colored(spec_.text_group, spec_.text_id, draw_x, spec_.y, spec_.font, spec_.color);
            } else {
                lang_text_draw_colored(spec_.text_group, spec_.text_id, spec_.x, spec_.y, spec_.font, spec_.color);
            }
            return;

        case UiTextContentType::Raw:
            if (spec_.alignment == UiTextAlignment::Center) {
                text_draw_centered(spec_.raw_text, spec_.x, spec_.y, box_width, spec_.font, spec_.color);
            } else if (spec_.alignment == UiTextAlignment::Right) {
                text_draw(spec_.raw_text, draw_x, spec_.y, spec_.font, spec_.color);
            } else {
                text_draw(spec_.raw_text, spec_.x, spec_.y, spec_.font, spec_.color);
            }
            return;

        case UiTextContentType::Number:
            if (spec_.alignment == UiTextAlignment::Center) {
                text_draw_number_centered_colored(spec_.value, spec_.x, spec_.y, box_width, spec_.font, spec_.color);
            } else {
                text_draw_number(
                    spec_.value,
                    spec_.prefix,
                    spec_.postfix ? spec_.postfix : "",
                    spec_.alignment == UiTextAlignment::Right ? draw_x : spec_.x,
                    spec_.y,
                    spec_.font,
                    spec_.color);
            }
            return;

        case UiTextContentType::Amount:
            if (spec_.alignment == UiTextAlignment::Center) {
                lang_text_draw_amount_centered(
                    spec_.text_group,
                    spec_.text_id,
                    spec_.value,
                    spec_.x,
                    spec_.y,
                    box_width,
                    spec_.font);
            } else {
                lang_text_draw_amount_colored(
                    spec_.text_group,
                    spec_.text_id,
                    spec_.value,
                    spec_.alignment == UiTextAlignment::Right ? draw_x : spec_.x,
                    spec_.y,
                    spec_.font,
                    spec_.color);
            }
            return;

        default:
            report_invalid_text_spec(spec_);
            return;
    }
}
