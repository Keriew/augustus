#include "graphics/ui_tiled_strip_primitive.h"

UiTiledStripPrimitive::UiTiledStripPrimitive(
    UiPrimitives &primitives,
    int start_image_id,
    int middle_image_id,
    int end_image_id,
    int x,
    int y,
    int segment_count,
    int logical_tile_width,
    int logical_tile_height,
    color_t color)
    : UiPrimitive(primitives)
    , start_image_id_(start_image_id)
    , middle_image_id_(middle_image_id)
    , end_image_id_(end_image_id)
    , x_(x)
    , y_(y)
    , segment_count_(segment_count)
    , logical_tile_width_(logical_tile_width)
    , logical_tile_height_(logical_tile_height)
    , color_(color)
{
}

int UiTiledStripPrimitive::tile_width() const
{
    if (logical_tile_width_ > 0) {
        return logical_tile_width_;
    }
    const image *img = primitives_.resolve_image(middle_image_id_ > 0 ? middle_image_id_ : start_image_id_);
    return img && img->width > 0 ? img->width : 0;
}

int UiTiledStripPrimitive::tile_height() const
{
    if (logical_tile_height_ > 0) {
        return logical_tile_height_;
    }
    const image *img = primitives_.resolve_image(middle_image_id_ > 0 ? middle_image_id_ : start_image_id_);
    return img && img->height > 0 ? img->height : 0;
}

void UiTiledStripPrimitive::draw() const
{
    if (segment_count_ <= 0) {
        return;
    }

    int step_x = tile_width();
    int draw_height = tile_height();
    if (step_x <= 0 || draw_height <= 0) {
        return;
    }

    for (int i = 0; i < segment_count_; ++i) {
        int image_id = middle_image_id_;
        if (i == 0) {
            image_id = start_image_id_;
        } else if (i == segment_count_ - 1) {
            image_id = end_image_id_;
        }

        primitives_.draw_image_rect(
            image_id,
            x_ + step_x * i,
            y_,
            logical_tile_width_,
            logical_tile_height_,
            color_);
    }
}
