#pragma once

#include "core/image.h"

#ifdef __cplusplus
class ImageData {
public:
    ImageData();
    explicit ImageData(const image &legacy_image);

    void sync_from_legacy(const image &legacy_image);
    void bind_payload(const char *key, void *payload, image_handle handle);

    int width() const;
    int height() const;
    int original_width() const;
    int original_height() const;

    const image &legacy() const;

private:
    int width_ = 0;
    int height_ = 0;
    int original_width_ = 0;
    int original_height_ = 0;
    image legacy_image_ = {};
};
#endif
