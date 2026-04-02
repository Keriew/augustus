#include "graphics/label_widget.h"

extern "C" {
#include "core/image_group.h"
#include "graphics/panel.h"
}

LabelWidget::LabelWidget(UiPrimitives &primitives, LabelWidgetStyle style, int x, int y, int width_blocks, int type)
    : UiWidget(primitives)
    , style_(style)
    , x_(x)
    , y_(y)
    , width_blocks_(width_blocks)
    , type_(type)
{
}

void LabelWidget::draw() const
{
    int image_base = image_group(GROUP_PANEL_BUTTON);
    for (int i = 0; i < width_blocks_; ++i) {
        int image_id;
        if (style_ == LabelWidgetStyle::Large) {
            if (i == 0) {
                image_id = 3 * type_;
            } else if (i < width_blocks_ - 1) {
                image_id = 3 * type_ + 1;
            } else {
                image_id = 3 * type_ + 2;
            }
        } else {
            if (i == 0) {
                image_id = 3 * type_ + 40;
            } else if (i < width_blocks_ - 1) {
                image_id = 3 * type_ + 41;
            } else {
                image_id = 3 * type_ + 42;
            }
        }

        primitives_.draw_image_rect(image_base + image_id, x_ + BLOCK_SIZE * i, y_, 0, 0, COLOR_MASK_NONE);
    }
}
