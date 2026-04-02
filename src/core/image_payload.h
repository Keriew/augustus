#pragma once

#include "core/image_data.h"
#include "core/image.h"

#ifdef __cplusplus
#include <string>

class ImagePayload {
public:
    ImagePayload(std::string key, const image &legacy_image);

    const std::string &key() const;
    image_handle handle() const;
    void set_handle(image_handle handle);
    const ImageData &image_data() const;
    const image &legacy() const;
    void sync_from_legacy(const image &legacy_image);
    void retain();
    int release();
    int ref_count() const;

private:
    std::string key_;
    image_handle handle_ = 0;
    int ref_count_ = 0;
    ImageData image_data_;
};

ImagePayload *image_payload_get(const char *path_key);
const ImagePayload *image_payload_get(const image *img);
ImagePayload *image_payload_load_png_payload(const char *path_key, const char *file_path);
void image_payload_retain_key(const char *path_key);
void image_payload_release_key(const char *path_key);
#endif

#ifdef __cplusplus
extern "C" {
#endif

const char *image_payload_acquire(image *img, const char *path_key);
const char *image_payload_load_png(image *img, const char *path_key, const char *file_path);
const char *image_payload_register(image *img, const char *path_key);
void image_payload_release(image *img);
const char *image_payload_key(const image *img);
const image *image_payload_legacy_view(const image *img);
const image *image_payload_legacy_for_key(const char *path_key);
image_handle image_payload_handle_for_key(const char *path_key);
void image_payload_clear_all(void);

#ifdef __cplusplus
}
#endif
