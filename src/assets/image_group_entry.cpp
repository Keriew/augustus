#include "assets/image_group_entry.h"

#include "core/image_payload.h"

#include <utility>

ImageGroupEntry::ImageGroupEntry(std::string id, std::string image_key, std::string top_image_key)
    : id_(std::move(id))
    , image_key_(std::move(image_key))
    , top_image_key_(std::move(top_image_key))
{
}

const std::string &ImageGroupEntry::id() const
{
    return id_;
}

const std::string &ImageGroupEntry::image_key() const
{
    return image_key_;
}

const std::string &ImageGroupEntry::top_image_key() const
{
    return top_image_key_;
}

void ImageGroupEntry::rebuild_legacy_image() const
{
    legacy_image_dirty_ = 0;
    legacy_image_ = {};
    legacy_top_image_ = {};
    legacy_animation_ = {};

    const image *base = image_key_.empty() ? nullptr : image_payload_legacy_for_key(image_key_.c_str());
    if (!base) {
        return;
    }

    legacy_image_ = *base;
    legacy_image_.top = nullptr;
    legacy_image_.animation = nullptr;

    if (!top_image_key_.empty()) {
        if (const image *top = image_payload_legacy_for_key(top_image_key_.c_str())) {
            legacy_top_image_ = *top;
            legacy_image_.top = &legacy_top_image_;
        }
    }

    if (has_animation_) {
        legacy_animation_ = animation_data_;
        legacy_animation_.num_sprites = static_cast<int>(animation_frame_keys_.size());
        legacy_image_.animation = &legacy_animation_;
    }
}

const image *ImageGroupEntry::legacy_image() const
{
    if (legacy_image_dirty_) {
        rebuild_legacy_image();
    }
    return legacy_image_.resource_handle > 0 ? &legacy_image_ : nullptr;
}

const image *ImageGroupEntry::animation_frame(int animation_offset) const
{
    if (animation_offset <= 0 || animation_offset > static_cast<int>(animation_frame_keys_.size())) {
        return nullptr;
    }
    const std::string &frame_image_key = animation_frame_keys_[animation_offset - 1];
    return frame_image_key.empty() ? nullptr : image_payload_legacy_for_key(frame_image_key.c_str());
}

int ImageGroupEntry::has_animation() const
{
    return has_animation_;
}

const image_animation &ImageGroupEntry::animation() const
{
    return animation_data_;
}

const std::vector<std::string> &ImageGroupEntry::animation_frame_keys() const
{
    return animation_frame_keys_;
}

void ImageGroupEntry::set_keys(std::string image_key, std::string top_image_key)
{
    image_key_ = std::move(image_key);
    top_image_key_ = std::move(top_image_key);
    legacy_image_dirty_ = 1;
}

void ImageGroupEntry::set_animation(const image_animation &animation, std::vector<std::string> frame_image_keys)
{
    animation_data_ = animation;
    animation_frame_keys_ = std::move(frame_image_keys);
    has_animation_ = !animation_frame_keys_.empty();
    legacy_image_dirty_ = 1;
}
