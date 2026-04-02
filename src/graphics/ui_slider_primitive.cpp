#include "graphics/ui_slider_primitive.h"

#include "graphics/ui_panel_primitive.h"
#include "graphics/ui_sprite_primitive.h"

#include "core/calc.h"
#include "core/image_group.h"
#include "graphics/ui_constants.h"

namespace {

const int kSliderPadding = 2;
const int kSliderThumbSize = 25;

}

UiSliderPrimitive::UiSliderPrimitive(
    UiPrimitives &primitives,
    int x,
    int y,
    int width_pixels,
    int min_value,
    int max_value,
    int current_value)
    : UiPrimitive(primitives)
    , x_(x)
    , y_(y)
    , width_pixels_(width_pixels)
    , min_value_(min_value)
    , max_value_(max_value)
    , current_value_(current_value)
{
}

int UiSliderPrimitive::track_width_blocks() const
{
    return width_pixels_ / BLOCK_SIZE;
}

int UiSliderPrimitive::trailing_width_pixels() const
{
    return width_pixels_ % BLOCK_SIZE;
}

int UiSliderPrimitive::thumb_offset() const
{
    int width = width_pixels_ - kSliderPadding * 2 - kSliderThumbSize;
    if (width <= 0) {
        return 0;
    }
    if (min_value_ == max_value_) {
        return width / 2;
    }
    return calc_adjust_with_percentage(
        width,
        calc_percentage(current_value_ - min_value_, max_value_ - min_value_));
}

void UiSliderPrimitive::draw() const
{
    int width_blocks = track_width_blocks();
    int extra_width = trailing_width_pixels();

    UiPanelPrimitive(primitives_, UiPanelPrimitiveStyle::Inner, x_, y_, width_blocks, 1).draw();
    if (extra_width > 0) {
        UiSpritePrimitive edge(primitives_, image_group(GROUP_SUNKEN_TEXTBOX_BACKGROUND) + 6, x_ + width_blocks * BLOCK_SIZE, y_);
        edge.set_logical_size(extra_width, BLOCK_SIZE);
        edge.draw();
    }

    UiSpritePrimitive thumb(
        primitives_,
        image_group(GROUP_PANEL_BUTTON) + 37,
        x_ + kSliderPadding + thumb_offset(),
        y_ - 2);
    thumb.draw();
}
