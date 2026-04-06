#include "assets/image_group_entry.h"

#include <utility>

ImageGroupEntry::ImageGroupEntry(std::string id)
    : id_(std::move(id))
{
}

const std::string &ImageGroupEntry::id() const
{
    return id_;
}

const RuntimeDrawSlice *ImageGroupEntry::footprint() const
{
    return footprint_.is_valid() ? &footprint_ : nullptr;
}

const RuntimeDrawSlice *ImageGroupEntry::top() const
{
    return has_top_ && top_.is_valid() ? &top_ : nullptr;
}

int ImageGroupEntry::has_top() const
{
    return has_top_ && top_.is_valid();
}

int ImageGroupEntry::has_animation() const
{
    return has_animation_;
}

const RuntimeAnimationTrack &ImageGroupEntry::animation() const
{
    return animation_;
}

int ImageGroupEntry::is_isometric() const
{
    return is_isometric_;
}

int ImageGroupEntry::tile_span() const
{
    return tile_span_;
}

const std::vector<color_t> &ImageGroupEntry::split_pixels() const
{
    return split_pixels_;
}

int ImageGroupEntry::split_width() const
{
    return split_width_;
}

int ImageGroupEntry::split_height() const
{
    return split_height_;
}

int ImageGroupEntry::top_height() const
{
    return top_height_;
}

void ImageGroupEntry::set_base_slice(RuntimeDrawSlice footprint, int is_isometric, int tile_span)
{
    footprint_ = std::move(footprint);
    is_isometric_ = is_isometric;
    tile_span_ = tile_span;
}

void ImageGroupEntry::set_top_slice(RuntimeDrawSlice top)
{
    top_ = std::move(top);
    has_top_ = top_.is_valid();
}

void ImageGroupEntry::clear_top_slice()
{
    top_ = {};
    has_top_ = 0;
}

void ImageGroupEntry::set_animation(RuntimeAnimationTrack animation)
{
    animation_ = std::move(animation);
    animation_.num_frames = static_cast<int>(animation_.frames.size());
    has_animation_ = !animation_.frames.empty();
}

void ImageGroupEntry::set_split_pixels(std::vector<color_t> split_pixels, int split_width, int split_height, int top_height)
{
    split_pixels_ = std::move(split_pixels);
    split_width_ = split_width;
    split_height_ = split_height;
    top_height_ = top_height;
}
