#pragma once

#include "core/image.h"

#ifdef __cplusplus
#include <string>
#include <vector>

class ImageGroupEntry {
public:
    ImageGroupEntry() = default;
    ImageGroupEntry(std::string id, std::string image_key, std::string top_image_key = std::string());

    const std::string &id() const;
    const std::string &image_key() const;
    const std::string &top_image_key() const;
    const image *legacy_image() const;
    const image *animation_frame(int animation_offset) const;
    int has_animation() const;
    const image_animation &animation() const;
    const std::vector<std::string> &animation_frame_keys() const;

    void set_keys(std::string image_key, std::string top_image_key = std::string());
    void set_animation(const image_animation &animation, std::vector<std::string> frame_image_keys);

private:
    void rebuild_legacy_image() const;

    std::string id_;
    std::string image_key_;
    std::string top_image_key_;
    image_animation animation_data_ = {};
    std::vector<std::string> animation_frame_keys_;
    int has_animation_ = 0;
    mutable int legacy_image_dirty_ = 1;
    mutable image legacy_image_ = {};
    mutable image legacy_top_image_ = {};
    mutable image_animation legacy_animation_ = {};
};
#endif
