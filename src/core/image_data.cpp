#include "core/image_data.h"

ImageData::ImageData() = default;

ImageData::ImageData(const image &legacy_image)
{
    sync_from_legacy(legacy_image);
}

void ImageData::sync_from_legacy(const image &legacy_image)
{
    width_ = legacy_image.width;
    height_ = legacy_image.height;
    original_width_ = legacy_image.original.width;
    original_height_ = legacy_image.original.height;
    legacy_image_ = legacy_image;
    legacy_image_.resource_key = nullptr;
    legacy_image_.resource_payload = nullptr;
}

void ImageData::bind_payload(const char *key, void *payload, image_handle handle)
{
    legacy_image_.resource_key = const_cast<char *>(key);
    legacy_image_.resource_payload = payload;
    legacy_image_.resource_handle = handle;
}

int ImageData::width() const
{
    return width_;
}

int ImageData::height() const
{
    return height_;
}

int ImageData::original_width() const
{
    return original_width_;
}

int ImageData::original_height() const
{
    return original_height_;
}

const image &ImageData::legacy() const
{
    return legacy_image_;
}
