#include "graphics/ui_one_row_border_primitive.h"

#include "graphics/ui_bordered_button_skin.h"
#include "graphics/ui_sprite_primitive.h"

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

    const UiBorderedButtonSkin skin = ui_bordered_button_skin_resolve(primitives_, has_focus_);
    const int block_width = skin.tile_width;
    const int block_height = skin.tile_height;
    if (block_width <= 0 || block_height <= 0) {
        return;
    }

    draw_horizontal_edges(skin, block_width, block_height);
    draw_vertical_edges(skin, block_width, block_height);
}

int UiOneRowBorderPrimitive::width_blocks() const
{
    const int block_width = ui_bordered_button_skin_resolve(primitives_, has_focus_).tile_width;
    return block_width > 0 ? (width_pixels_ + block_width - 1) / block_width : 0;
}

int UiOneRowBorderPrimitive::height_blocks() const
{
    const int block_height = ui_bordered_button_skin_resolve(primitives_, has_focus_).tile_height;
    return block_height > 0 ? (height_pixels_ + block_height - 1) / block_height : 0;
}

int UiOneRowBorderPrimitive::right_edge_x(int block_width) const
{
    const int width_block_count = width_blocks();
    const int last_block_offset_x = block_width * width_block_count - width_pixels_;
    return x_ + block_width * (width_block_count - 1) - last_block_offset_x;
}

int UiOneRowBorderPrimitive::bottom_edge_y(int block_height) const
{
    const int height_block_count = height_blocks();
    const int last_block_offset_y = block_height * height_block_count - height_pixels_;
    return y_ + block_height * (height_block_count - 1) - last_block_offset_y;
}

void UiOneRowBorderPrimitive::draw_horizontal_edges(const UiBorderedButtonSkin &skin, int block_width, int block_height) const
{
    const int width_block_count = width_blocks();
    const int draw_bottom_y = bottom_edge_y(block_height);

    for (int i = 0; i < width_block_count; ++i) {
        int draw_x = x_ + block_width * i;
        if (i == 0) {
        } else if (i == width_block_count - 1) {
            draw_x = right_edge_x(block_width);
        }

        if (skin.use_runtime) {
            const RuntimeDrawSlice &top_slice = skin.slices[static_cast<size_t>(i == 0 ? 0 : (i == width_block_count - 1 ? 2 : 1))];
            primitives_.draw_runtime_slice(top_slice, static_cast<float>(draw_x), static_cast<float>(y_), 0, 0, color_);
        } else {
            const int top_image_id = skin.image_base + (i == 0 ? 0 : (i == width_block_count - 1 ? 2 : 1));
            UiSpritePrimitive(primitives_, top_image_id, draw_x, y_, color_).draw();
        }
        if (height_blocks() > 1) {
            if (skin.use_runtime) {
                const RuntimeDrawSlice &bottom_slice = skin.slices[static_cast<size_t>(i == 0 ? 6 : (i == width_block_count - 1 ? 4 : 5))];
                primitives_.draw_runtime_slice(bottom_slice, static_cast<float>(draw_x), static_cast<float>(draw_bottom_y), 0, 0, color_);
            } else {
                const int bottom_image_id = skin.image_base + (i == 0 ? 6 : (i == width_block_count - 1 ? 4 : 5));
                UiSpritePrimitive(primitives_, bottom_image_id, draw_x, draw_bottom_y, color_).draw();
            }
        }
    }
}

void UiOneRowBorderPrimitive::draw_vertical_edges(const UiBorderedButtonSkin &skin, int block_width, int block_height) const
{
    const int height_block_count = height_blocks();
    if (height_block_count <= 2) {
        return;
    }

    const int right_x = right_edge_x(block_width);
    for (int i = 1; i < height_block_count - 1; ++i) {
        const int draw_y = y_ + block_height * i;
        if (skin.use_runtime) {
            primitives_.draw_runtime_slice(skin.slices[7], static_cast<float>(x_), static_cast<float>(draw_y), 0, 0, color_);
            primitives_.draw_runtime_slice(skin.slices[3], static_cast<float>(right_x), static_cast<float>(draw_y), 0, 0, color_);
        } else {
            UiSpritePrimitive(primitives_, skin.image_base + 7, x_, draw_y, color_).draw();
            UiSpritePrimitive(primitives_, skin.image_base + 3, right_x, draw_y, color_).draw();
        }
    }
}
