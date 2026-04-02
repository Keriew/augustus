#include "graphics/bordered_button_widget.h"

extern "C" {
#include "core/image_group.h"
#include "graphics/panel.h"
}

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
    const int image_base = image_group(GROUP_BORDERED_BUTTON) + (has_focus_ ? 8 : 0);
    const image *top_middle = primitives_.resolve_image(image_base + 1);
    const image *left_side = primitives_.resolve_image(image_base + 7);

    const int tile_width = top_middle && top_middle->width > 0 ? top_middle->width : BLOCK_SIZE;
    const int tile_height = left_side && left_side->height > 0 ? left_side->height : BLOCK_SIZE;
    const int width_blocks = (width_pixels_ + tile_width - 1) / tile_width;
    const int height_blocks = (height_pixels_ + tile_height - 1) / tile_height;
    int last_block_offset_x = tile_width * width_blocks - width_pixels_;
    int last_block_offset_y = tile_height * height_blocks - height_pixels_;

    if (width_blocks <= 0 || height_blocks <= 0) {
        return;
    }

    for (int yy = 0; yy < height_blocks; ++yy) {
        int draw_offset_y = y_ + BLOCK_SIZE * yy;
        for (int xx = 0; xx < width_blocks; ++xx) {
            int draw_offset_x = x_ + tile_width * xx;
            int image_id;
            int offset_x = 0;
            int offset_y = 0;
            if (yy == 0) {
                if (xx == 0) {
                    image_id = image_base;
                } else if (xx < width_blocks - 1) {
                    image_id = image_base + 1;
                } else {
                    image_id = image_base + 2;
                    offset_x = -last_block_offset_x;
                }
            } else if (yy < height_blocks - 1) {
                if (xx == 0) {
                    image_id = image_base + 7;
                    image_id = image_base + 3;
                    offset_x = -last_block_offset_x;
                } else {
                    continue;
                }
            } else {
                if (xx == 0) {
                    image_id = image_base + 6;
                    offset_y = -last_block_offset_y;
                } else if (xx < width_blocks - 1) {
                    image_id = image_base + 5;
                    offset_y = -last_block_offset_y;
                } else {
                    image_id = image_base + 4;
                    offset_x = -last_block_offset_x;
                    offset_y = -last_block_offset_y;
                }
            }
            primitives_.draw_image_rect(image_id, draw_offset_x + offset_x, draw_offset_y + offset_y, 0, 0, color_);
        }
    }
}
