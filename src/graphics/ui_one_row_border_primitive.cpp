#include "graphics/ui_one_row_border_primitive.h"

#include "graphics/ui_sprite_primitive.h"

extern "C" {
#include "core/image_group.h"
#include "graphics/panel.h"
}

UiOneRowBorderPrimitive::UiOneRowBorderPrimitive(
    UiPrimitives &primitives,
    int x,
    int y,
    int width_pixels,
    int height_pixels,
    int has_focus,
    color_t color)
    : UiPrimitive(primitives)
    , x_(x)
    , y_(y)
    , width_pixels_(width_pixels)
    , height_pixels_(height_pixels)
    , has_focus_(has_focus)
    , color_(color)
{
}

void UiOneRowBorderPrimitive::draw() const
{
    if (width_pixels_ <= 0 || height_pixels_ <= 0) {
        return;
    }

    const int image_base = image_group(GROUP_BORDERED_BUTTON) + (has_focus_ ? 8 : 0);
    const image *top_middle = primitives_.resolve_image(image_base + 1);
    const image *left_side = primitives_.resolve_image(image_base + 7);
    const int tile_width = top_middle && top_middle->width > 0 ? top_middle->width : BLOCK_SIZE;
    const int tile_height = left_side && left_side->height > 0 ? left_side->height : BLOCK_SIZE;
    const int width_blocks = (width_pixels_ + tile_width - 1) / tile_width;
    const int height_blocks = (height_pixels_ + tile_height - 1) / tile_height;
    const int last_block_offset_x = tile_width * width_blocks - width_pixels_;
    const int last_block_offset_y = tile_height * height_blocks - height_pixels_;

    const int top_y = y_;
    const int bottom_y = y_ + tile_height * (height_blocks - 1) - last_block_offset_y;

    for (int i = 0; i < width_blocks; ++i) {
        int top_image_id = image_base + 1;
        int bottom_image_id = image_base + 5;
        int draw_x = x_ + tile_width * i;
        if (i == 0) {
            top_image_id = image_base + 0;
            bottom_image_id = image_base + 6;
        } else if (i == width_blocks - 1) {
            top_image_id = image_base + 2;
            bottom_image_id = image_base + 4;
            draw_x -= last_block_offset_x;
        }

        UiSpritePrimitive top_piece(primitives_, top_image_id, draw_x, top_y, color_);
        top_piece.draw();

        if (height_blocks > 1) {
            UiSpritePrimitive bottom_piece(primitives_, bottom_image_id, draw_x, bottom_y, color_);
            bottom_piece.draw();
        }
    }
}
