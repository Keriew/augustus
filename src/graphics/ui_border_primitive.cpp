#include "graphics/ui_border_primitive.h"

#include "graphics/ui_bordered_button_skin.h"

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

    const UiBorderedButtonSkin skin = ui_bordered_button_skin_resolve(primitives_, has_focus_);
    const int block_width = skin.tile_width;
    const int block_height = skin.tile_height;

    if (block_width <= 0 || block_height <= 0) {
        return;
    }

    draw_top_edge(skin, block_width);
    draw_bottom_edge(skin, block_width, block_height);
    draw_left_edge(skin, block_height);
    draw_right_edge(skin, block_width, block_height);
}

int UiBorderPrimitive::width_blocks() const
{
    const int block_width = ui_bordered_button_skin_resolve(primitives_, has_focus_).tile_width;
    return block_width > 0 ? (width_pixels_ + block_width - 1) / block_width : 0;
}

int UiBorderPrimitive::height_blocks() const
{
    const int block_height = ui_bordered_button_skin_resolve(primitives_, has_focus_).tile_height;
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

void UiBorderPrimitive::draw_top_edge(const UiBorderedButtonSkin &skin, int block_width) const
{
    const int width_block_count = width_blocks();
    for (int i = 0; i < width_block_count; ++i) {
        int draw_x = x_ + block_width * i;
        if (i == 0) {
        } else if (i == width_block_count - 1) {
            draw_x = right_edge_x(block_width);
        }
        if (skin.use_runtime) {
            const RuntimeDrawSlice &slice = skin.slices[static_cast<size_t>(i == 0 ? 0 : (i == width_block_count - 1 ? 2 : 1))];
            primitives_.draw_runtime_slice(slice, static_cast<float>(draw_x), static_cast<float>(y_), 0, 0, color_);
        } else {
            const int image_id = skin.image_base + (i == 0 ? 0 : (i == width_block_count - 1 ? 2 : 1));
            primitives_.draw_image_rect(image_id, draw_x, y_, 0, 0, color_);
        }
    }
}

void UiBorderPrimitive::draw_bottom_edge(const UiBorderedButtonSkin &skin, int block_width, int block_height) const
{
    const int width_block_count = width_blocks();
    const int draw_y = bottom_edge_y(block_height);
    for (int i = 0; i < width_block_count; ++i) {
        int draw_x = x_ + block_width * i;
        if (i == 0) {
        } else if (i == width_block_count - 1) {
            draw_x = right_edge_x(block_width);
        }
        if (skin.use_runtime) {
            const RuntimeDrawSlice &slice = skin.slices[static_cast<size_t>(i == 0 ? 6 : (i == width_block_count - 1 ? 4 : 5))];
            primitives_.draw_runtime_slice(slice, static_cast<float>(draw_x), static_cast<float>(draw_y), 0, 0, color_);
        } else {
            const int image_id = skin.image_base + (i == 0 ? 6 : (i == width_block_count - 1 ? 4 : 5));
            primitives_.draw_image_rect(image_id, draw_x, draw_y, 0, 0, color_);
        }
    }
}

void UiBorderPrimitive::draw_left_edge(const UiBorderedButtonSkin &skin, int block_height) const
{
    const int height_block_count = height_blocks();
    for (int i = 1; i < height_block_count - 1; ++i) {
        const int draw_y = y_ + block_height * i;
        if (skin.use_runtime) {
            primitives_.draw_runtime_slice(skin.slices[7], static_cast<float>(x_), static_cast<float>(draw_y), 0, 0, color_);
        } else {
            primitives_.draw_image_rect(skin.image_base + 7, x_, draw_y, 0, 0, color_);
        }
    }
}

void UiBorderPrimitive::draw_right_edge(const UiBorderedButtonSkin &skin, int block_width, int block_height) const
{
    const int height_block_count = height_blocks();
    const int draw_x = right_edge_x(block_width);
    for (int i = 1; i < height_block_count - 1; ++i) {
        const int draw_y = y_ + block_height * i;
        if (skin.use_runtime) {
            primitives_.draw_runtime_slice(skin.slices[3], static_cast<float>(draw_x), static_cast<float>(draw_y), 0, 0, color_);
        } else {
            primitives_.draw_image_rect(skin.image_base + 3, draw_x, draw_y, 0, 0, color_);
        }
    }
}
