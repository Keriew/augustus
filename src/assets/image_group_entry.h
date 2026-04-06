#pragma once

#include "graphics/color.h"
#include "graphics/runtime_texture.h"

#include <string>
#include <vector>

struct RuntimeAnimationTrack {
    int num_frames = 0;
    int sprite_offset_x = 0;
    int sprite_offset_y = 0;
    int can_reverse = 0;
    int speed_id = 0;
    std::vector<RuntimeDrawSlice> frames;
};

class ImageGroupEntry {
public:
    ImageGroupEntry() = default;
    explicit ImageGroupEntry(std::string id);

    const std::string &id() const;
    const RuntimeDrawSlice *footprint() const;
    const RuntimeDrawSlice *top() const;
    int has_top() const;
    int has_animation() const;
    const RuntimeAnimationTrack &animation() const;
    int is_isometric() const;
    int tile_span() const;
    const std::vector<color_t> &split_pixels() const;
    int split_width() const;
    int split_height() const;
    int top_height() const;

    void set_base_slice(RuntimeDrawSlice footprint, int is_isometric, int tile_span);
    void set_top_slice(RuntimeDrawSlice top);
    void clear_top_slice();
    void set_animation(RuntimeAnimationTrack animation);
    void set_split_pixels(std::vector<color_t> split_pixels, int split_width, int split_height, int top_height);

private:
    std::string id_;
    RuntimeDrawSlice footprint_;
    RuntimeDrawSlice top_;
    RuntimeAnimationTrack animation_;
    int has_top_ = 0;
    int has_animation_ = 0;
    int is_isometric_ = 0;
    int tile_span_ = 0;
    std::vector<color_t> split_pixels_;
    int split_width_ = 0;
    int split_height_ = 0;
    int top_height_ = 0;
};
