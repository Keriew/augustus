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

const image *ImageGroupEntry::legacy_image() const
{
    return image_key_.empty() ? nullptr : image_payload_legacy_for_key(image_key_.c_str());
}

void ImageGroupEntry::set_keys(std::string image_key, std::string top_image_key)
{
    image_key_ = std::move(image_key);
    top_image_key_ = std::move(top_image_key);
}
