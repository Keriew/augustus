#include "graphics/bordered_button_widget.h"

#include "graphics/ui_border_primitive.h"
#include "graphics/ui_constants.h"
#include "graphics/ui_panel_primitive.h"

namespace {

class ScopedButtonClip {
public:
    ScopedButtonClip(const UiPrimitives &primitives, int x, int y, int width, int height)
        : primitives_(primitives)
        , active_(width > 0 && height > 0)
    {
        if (!active_) {
            return;
        }
        primitives_.push_renderer_state();
        primitives_.set_clip_rectangle(x, y, width, height);
    }

    ~ScopedButtonClip()
    {
        if (!active_) {
            return;
        }
        primitives_.pop_renderer_state();
    }

private:
    const UiPrimitives &primitives_;
    int active_;
};

} // namespace

BorderedButtonWidget::BorderedButtonWidget(
    UiPrimitives &primitives,
    int x,
    int y,
    int width_pixels,
    int height_pixels,
    int has_focus,
    color_t color,
    BorderedButtonFillStyle fill_style)
    : ButtonWidget(primitives, x, y)
    , width_pixels_(width_pixels)
    , height_pixels_(height_pixels)
    , has_focus_(has_focus)
    , color_(color)
    , fill_style_(fill_style)
{
}

void BorderedButtonWidget::draw() const
{
    draw_container();
}

void BorderedButtonWidget::draw_container() const
{
    if (width_pixels_ <= 0 || height_pixels_ <= 0) {
        return;
    }
    draw_fill();
    draw_border();
}

void BorderedButtonWidget::draw_fill() const
{
    if (fill_style_ == BorderedButtonFillStyle::Transparent) {
        return;
    }
    ScopedButtonClip clip(primitives_, x_, y_, width_pixels_, height_pixels_);
    UiPanelPrimitive(
        primitives_,
        UiPanelPrimitiveStyle::Unbordered,
        x_,
        y_,
        width_blocks(),
        height_blocks(),
        color_)
        .draw();
}

void BorderedButtonWidget::draw_border() const
{
    UiBorderPrimitive(primitives_, x_, y_, width_pixels_, height_pixels_, has_focus_, color_).draw();
}

int BorderedButtonWidget::width_blocks() const
{
    return (width_pixels_ + BLOCK_SIZE - 1) / BLOCK_SIZE;
}

int BorderedButtonWidget::height_blocks() const
{
    return (height_pixels_ + BLOCK_SIZE - 1) / BLOCK_SIZE;
}
