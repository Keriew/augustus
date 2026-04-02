#include "graphics/panel_widget.h"

extern "C" {
#include "core/image_group.h"
#include "graphics/panel.h"
}

PanelWidget::PanelWidget(
    UiPrimitives &primitives,
    PanelWidgetStyle style,
    int x,
    int y,
    int width_blocks,
    int height_blocks,
    color_t color)
    : UiWidget(primitives)
    , style_(style)
    , x_(x)
    , y_(y)
    , width_blocks_(width_blocks)
    , height_blocks_(height_blocks)
    , color_(color)
{
}

void PanelWidget::draw() const
{
    switch (style_) {
        case PanelWidgetStyle::Outer:
            draw_outer();
            break;
        case PanelWidgetStyle::Unbordered:
            draw_unbordered();
            break;
        case PanelWidgetStyle::Inner:
        default:
            draw_inner();
            break;
    }
}

void PanelWidget::draw_outer() const
{
    int image_base = image_group(GROUP_DIALOG_BACKGROUND);
    int image_y = 0;
    int y_add = 0;
    for (int yy = 0; yy < height_blocks_; ++yy) {
        int image_x = 0;
        for (int xx = 0; xx < width_blocks_; ++xx) {
            int image_id;
            if (yy == 0) {
                if (xx == 0) {
                    image_id = 0;
                } else if (xx < width_blocks_ - 1) {
                    image_id = 1 + image_x++;
                } else {
                    image_id = 11;
                }
                y_add = 0;
            } else if (yy < height_blocks_ - 1) {
                if (xx == 0) {
                    image_id = 12 + image_y;
                } else if (xx < width_blocks_ - 1) {
                    image_id = 13 + image_y + image_x++;
                } else {
                    image_id = 23 + image_y;
                }
                y_add = 12;
            } else {
                if (xx == 0) {
                    image_id = 132;
                } else if (xx < width_blocks_ - 1) {
                    image_id = 133 + image_x++;
                } else {
                    image_id = 143;
                }
                y_add = 0;
            }
            primitives_.draw_image_rect(
                image_base + image_id,
                x_ + BLOCK_SIZE * xx,
                y_ + BLOCK_SIZE * yy,
                BLOCK_SIZE,
                BLOCK_SIZE,
                COLOR_MASK_NONE);
            if (image_x >= 10) {
                image_x = 0;
            }
        }
        image_y += y_add;
        if (image_y >= 120) {
            image_y = 0;
        }
    }
}

void PanelWidget::draw_unbordered() const
{
    int image_base = image_group(GROUP_DIALOG_BACKGROUND);
    int image_y = 0;
    for (int yy = 0; yy < height_blocks_; ++yy) {
        int image_x = 0;
        for (int xx = 0; xx < width_blocks_; ++xx) {
            int image_id = 13 + image_y + image_x++;
            primitives_.draw_image_rect(
                image_base + image_id,
                x_ + BLOCK_SIZE * xx,
                y_ + BLOCK_SIZE * yy,
                BLOCK_SIZE,
                BLOCK_SIZE,
                color_);
            if (image_x >= 10) {
                image_x = 0;
            }
        }
        image_y += 12;
        if (image_y >= 120) {
            image_y = 0;
        }
    }
}

void PanelWidget::draw_inner() const
{
    int image_base = image_group(GROUP_SUNKEN_TEXTBOX_BACKGROUND);
    int image_y = 0;
    int y_add = 0;
    for (int yy = 0; yy < height_blocks_; ++yy) {
        int image_x = 0;
        for (int xx = 0; xx < width_blocks_; ++xx) {
            int image_id;
            if (yy == 0) {
                if (xx == 0) {
                    image_id = 0;
                } else if (xx < width_blocks_ - 1) {
                    image_id = 1 + image_x++;
                } else {
                    image_id = 6;
                }
                y_add = 0;
            } else if (yy < height_blocks_ - 1) {
                if (xx == 0) {
                    image_id = 7 + image_y;
                } else if (xx < width_blocks_ - 1) {
                    image_id = 8 + image_y + image_x++;
                } else {
                    image_id = 13 + image_y;
                }
                y_add = 7;
            } else {
                if (xx == 0) {
                    image_id = 42;
                } else if (xx < width_blocks_ - 1) {
                    image_id = 43 + image_x++;
                } else {
                    image_id = 48;
                }
                y_add = 0;
            }
            primitives_.draw_image_rect(
                image_base + image_id,
                x_ + BLOCK_SIZE * xx,
                y_ + BLOCK_SIZE * yy,
                BLOCK_SIZE,
                BLOCK_SIZE,
                COLOR_MASK_NONE);
            if (image_x >= 5) {
                image_x = 0;
            }
        }
        image_y += y_add;
        if (image_y >= 35) {
            image_y = 0;
        }
    }
}
