#include "graphics/ui_border_primitive.h"

#include "core/image_group.h"
#include "graphics/ui_constants.h"

UiBorderPrimitive::UiBorderPrimitive(
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

void UiBorderPrimitive::draw() const
{
    if (width_pixels_ <= 0 || height_pixels_ <= 0) {
        return;
    }

    const int current_image_base = image_base();
    const int block_width = tile_width();
    const int block_height = tile_height();

    if (block_width <= 0 || block_height <= 0) {
        return;
    }

    draw_top_edge(current_image_base, block_width);
    draw_bottom_edge(current_image_base, block_width, block_height);
    draw_left_edge(current_image_base, block_height);
    draw_right_edge(current_image_base, block_width, block_height);
}

int UiBorderPrimitive::image_base() const
{
    return image_group(GROUP_BORDERED_BUTTON) + (has_focus_ ? 8 : 0);
}

int UiBorderPrimitive::tile_width() const
{
    const image *top_middle = primitives_.resolve_image(image_base() + 1);
    return top_middle && top_middle->width > 0 ? top_middle->width : BLOCK_SIZE;
}

int UiBorderPrimitive::tile_height() const
{
    const image *left_side = primitives_.resolve_image(image_base() + 7);
    return left_side && left_side->height > 0 ? left_side->height : BLOCK_SIZE;
}

int UiBorderPrimitive::width_blocks() const
{
    const int block_width = tile_width();
    return block_width > 0 ? (width_pixels_ + block_width - 1) / block_width : 0;
}

int UiBorderPrimitive::height_blocks() const
{
    const int block_height = tile_height();
    return block_height > 0 ? (height_pixels_ + block_height - 1) / block_height : 0;
}

int UiBorderPrimitive::right_edge_x(int block_width) const
{
    const int width_block_count = width_blocks();
    const int last_block_offset_x = block_width * width_block_count - width_pixels_;
    return x_ + block_width * (width_block_count - 1) - last_block_offset_x;
}

int UiBorderPrimitive::bottom_edge_y(int block_height) const
{
    const int height_block_count = height_blocks();
    const int last_block_offset_y = block_height * height_block_count - height_pixels_;
    return y_ + block_height * (height_block_count - 1) - last_block_offset_y;
}

void UiBorderPrimitive::draw_top_edge(int image_base, int block_width) const
{
    const int width_block_count = width_blocks();
    for (int i = 0; i < width_block_count; ++i) {
        int image_id = image_base + 1;
        int draw_x = x_ + block_width * i;
        if (i == 0) {
            image_id = image_base + 0;
        } else if (i == width_block_count - 1) {
            image_id = image_base + 2;
            draw_x = right_edge_x(block_width);
        }
        primitives_.draw_image_rect(image_id, draw_x, y_, 0, 0, color_);
    }
}

void UiBorderPrimitive::draw_bottom_edge(int image_base, int block_width, int block_height) const
{
    const int width_block_count = width_blocks();
    const int draw_y = bottom_edge_y(block_height);
    for (int i = 0; i < width_block_count; ++i) {
        int image_id = image_base + 5;
        int draw_x = x_ + block_width * i;
        if (i == 0) {
            image_id = image_base + 6;
        } else if (i == width_block_count - 1) {
            image_id = image_base + 4;
            draw_x = right_edge_x(block_width);
        }
        primitives_.draw_image_rect(image_id, draw_x, draw_y, 0, 0, color_);
    }
}

void UiBorderPrimitive::draw_left_edge(int image_base, int block_height) const
{
    const int height_block_count = height_blocks();
    for (int i = 1; i < height_block_count - 1; ++i) {
        primitives_.draw_image_rect(image_base + 7, x_, y_ + block_height * i, 0, 0, color_);
    }
}

void UiBorderPrimitive::draw_right_edge(int image_base, int block_width, int block_height) const
{
    const int height_block_count = height_blocks();
    const int draw_x = right_edge_x(block_width);
    for (int i = 1; i < height_block_count - 1; ++i) {
        primitives_.draw_image_rect(image_base + 3, draw_x, y_ + block_height * i, 0, 0, color_);
    }
}
