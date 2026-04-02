#include "graphics/scrollbar_widget.h"

extern "C" {
#include "core/calc.h"
#include "core/image_group.h"
}

ScrollbarWidget::ScrollbarWidget(UiPrimitives &primitives, const scrollbar_type &scrollbar)
    : UiWidget(primitives)
    , scrollbar_(scrollbar)
{
}

void ScrollbarWidget::draw() const
{
    if (!(scrollbar_.max_scroll_position > 0 || scrollbar_.always_visible)) {
        return;
    }

    int pct;
    if (scrollbar_.scroll_position <= 0) {
        pct = 0;
    } else if (scrollbar_.scroll_position >= scrollbar_.max_scroll_position) {
        pct = 100;
    } else {
        pct = calc_percentage(scrollbar_.scroll_position, scrollbar_.max_scroll_position);
    }

    const int scroll_button_height = 26;
    const int scroll_button_width = 39;
    const int scroll_dot_size = 25;
    const int total_button_height = 2 * scroll_button_height + scroll_dot_size;

    int offset = calc_adjust_with_percentage(
        scrollbar_.height - total_button_height - 2 * scrollbar_.dot_padding,
        pct);
    if (scrollbar_.is_dragging_scrollbar_dot) {
        offset = scrollbar_.scrollbar_dot_drag_offset;
    }

    primitives_.draw_image_rect(
        image_group(GROUP_PANEL_BUTTON) + 39,
        scrollbar_.x + (scroll_button_width - scroll_dot_size) / 2,
        scrollbar_.y + offset + scroll_button_height + scrollbar_.dot_padding,
        scroll_dot_size,
        scroll_dot_size,
        COLOR_MASK_NONE);
}
