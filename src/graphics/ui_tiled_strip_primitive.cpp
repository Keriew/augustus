#include "graphics/ui_tiled_strip_primitive.h"

#include <utility>

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
    color_t color,
    int middle_pattern_length)
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
    , middle_pattern_length_(middle_pattern_length > 0 ? middle_pattern_length : 1)
{
}

UiTiledStripPrimitive::UiTiledStripPrimitive(
    UiPrimitives &primitives,
    RuntimeDrawSlice start_slice,
    std::vector<RuntimeDrawSlice> middle_slices,
    RuntimeDrawSlice end_slice,
    int x,
    int y,
    int segment_count,
    int logical_tile_width,
    int logical_tile_height,
    color_t color)
    : UiPrimitive(primitives)
    , start_image_id_(0)
    , middle_image_id_(0)
    , end_image_id_(0)
    , start_slice_(std::move(start_slice))
    , middle_slices_(std::move(middle_slices))
    , end_slice_(std::move(end_slice))
    , x_(x)
    , y_(y)
    , segment_count_(segment_count)
    , logical_tile_width_(logical_tile_width)
    , logical_tile_height_(logical_tile_height)
    , color_(color)
    , middle_pattern_length_(static_cast<int>(middle_slices_.size()) > 0 ? static_cast<int>(middle_slices_.size()) : 1)
{
}

int UiTiledStripPrimitive::use_runtime_slices() const
{
    return start_slice_.is_valid() && end_slice_.is_valid() && !middle_slices_.empty();
}

int UiTiledStripPrimitive::tile_width() const
{
    if (logical_tile_width_ > 0) {
        return logical_tile_width_;
    }
    if (use_runtime_slices()) {
        const RuntimeDrawSlice &reference = middle_slices_.front().is_valid() ? middle_slices_.front() : start_slice_;
        return reference.width > 0 ? reference.width : 0;
    }
    const image *img = primitives_.resolve_image(middle_image_id_ > 0 ? middle_image_id_ : start_image_id_);
    return img && img->width > 0 ? img->width : 0;
}

int UiTiledStripPrimitive::tile_height() const
{
    if (logical_tile_height_ > 0) {
        return logical_tile_height_;
    }
    if (use_runtime_slices()) {
        const RuntimeDrawSlice &reference = middle_slices_.front().is_valid() ? middle_slices_.front() : start_slice_;
        return reference.height > 0 ? reference.height : 0;
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
        const int draw_x = x_ + step_x * i;
        if (use_runtime_slices()) {
            const RuntimeDrawSlice *slice = &middle_slices_.front();
            if (i == 0) {
                slice = &start_slice_;
            } else if (i == segment_count_ - 1) {
                slice = &end_slice_;
            } else if (middle_pattern_length_ > 1) {
                slice = &middle_slices_[static_cast<size_t>((i - 1) % middle_pattern_length_)];
            }

            primitives_.draw_runtime_slice(
                *slice,
                static_cast<float>(draw_x),
                static_cast<float>(y_),
                logical_tile_width_,
                logical_tile_height_,
                color_);
            continue;
        }

        int image_id = middle_image_id_;
        if (i == 0) {
            image_id = start_image_id_;
        } else if (i == segment_count_ - 1) {
            image_id = end_image_id_;
        } else if (middle_pattern_length_ > 1) {
            image_id = middle_image_id_ + ((i - 1) % middle_pattern_length_);
        }
        primitives_.draw_image_rect(
            image_id,
            draw_x,
            y_,
            logical_tile_width_,
            logical_tile_height_,
            color_);
    }
}
