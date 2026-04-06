#include "graphics/label_widget.h"

#include "graphics/ui_tiled_strip_primitive.h"

#include "core/image_group.h"
#include "graphics/ui_constants.h"

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
    int start_image_id;
    int middle_image_id;
    int end_image_id;

    if (style_ == LabelWidgetStyle::Large) {
        start_image_id = image_base + 3 * type_;
        middle_image_id = image_base + 3 * type_ + 1;
        end_image_id = image_base + 3 * type_ + 2;
    } else {
        start_image_id = image_base + 3 * type_ + 40;
        middle_image_id = image_base + 3 * type_ + 41;
        end_image_id = image_base + 3 * type_ + 42;
    }

    UiTiledStripPrimitive(
        primitives_,
        start_image_id,
        middle_image_id,
        end_image_id,
        x_,
        y_,
        width_blocks_,
        0,
        0)
        .draw();
}
