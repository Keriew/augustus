#include "core/image_payload.h"

extern "C" {
#include "core/png_read.h"
#include "graphics/renderer.h"
}

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>

namespace {

using PayloadMap = std::unordered_map<std::string, std::unique_ptr<ImagePayload>>;

PayloadMap g_payloads;

static char *duplicate_key(const std::string &key)
{
    char *copy = static_cast<char *>(malloc(key.size() + 1));
    if (!copy) {
        return nullptr;
    }
    memcpy(copy, key.c_str(), key.size() + 1);
    return copy;
}

static ImagePayload *payload_from_image(const image *img)
{
    return img ? static_cast<ImagePayload *>(img->resource_payload) : nullptr;
}

static void set_image_key(image *img, const std::string &key)
{
    if (!img) {
        return;
    }
    free(img->resource_key);
    img->resource_key = duplicate_key(key);
}

static PayloadMap::iterator find_payload(const char *path_key)
{
    return path_key ? g_payloads.find(path_key) : g_payloads.end();
}

static ImagePayload *find_payload_ptr(const char *path_key)
{
    auto it = find_payload(path_key);
    return it == g_payloads.end() ? nullptr : it->second.get();
}

static void release_renderer_handle(image_handle handle)
{
    if (handle <= 0) {
        return;
    }
    const graphics_renderer_interface *renderer = graphics_renderer();
    if (!renderer || !renderer->release_image_resource) {
        return;
    }
    image temp = {};
    temp.resource_handle = handle;
    renderer->release_image_resource(&temp);
}

static void release_payload_iterator(PayloadMap::iterator it)
{
    if (it == g_payloads.end()) {
        return;
    }
    if (it->second->release() <= 0) {
        release_renderer_handle(it->second->handle());
        g_payloads.erase(it);
    }
}

static void bind_image_to_payload(image *img, ImagePayload *payload)
{
    if (!img) {
        return;
    }
    if (!payload) {
        img->resource_payload = nullptr;
        img->resource_handle = 0;
        free(img->resource_key);
        img->resource_key = nullptr;
        return;
    }

    img->resource_payload = payload;
    img->resource_handle = payload->handle();
    set_image_key(img, payload->key());
}

static void release_image_binding(image *img, int release_renderer_resource)
{
    if (!img) {
        return;
    }

    image_handle handle = img->resource_handle;
    if (img->resource_key && *img->resource_key) {
        auto it = find_payload(img->resource_key);
        if (it != g_payloads.end()) {
            if (it->second->release() <= 0) {
                if (release_renderer_resource) {
                    release_renderer_handle(it->second->handle());
                }
                g_payloads.erase(it);
            }
            handle = 0;
        }
    }

    if (release_renderer_resource && handle > 0) {
        release_renderer_handle(handle);
    }

    free(img->resource_key);
    img->resource_key = nullptr;
    img->resource_handle = 0;
    img->resource_payload = nullptr;
}

} // namespace

ImagePayload::ImagePayload(std::string key, const image &legacy_image)
    : key_(std::move(key))
    , image_data_(legacy_image)
{
    image_data_.bind_payload(key_.c_str(), this, handle_);
}

const std::string &ImagePayload::key() const
{
    return key_;
}

image_handle ImagePayload::handle() const
{
    return handle_;
}

void ImagePayload::set_handle(image_handle handle)
{
    handle_ = handle;
    image_data_.bind_payload(key_.c_str(), this, handle_);
}

const ImageData &ImagePayload::image_data() const
{
    return image_data_;
}

const image &ImagePayload::legacy() const
{
    return image_data_.legacy();
}

void ImagePayload::sync_from_legacy(const image &legacy_image)
{
    image_data_.sync_from_legacy(legacy_image);
    image_data_.bind_payload(key_.c_str(), this, handle_);
}

void ImagePayload::retain()
{
    ++ref_count_;
}

int ImagePayload::release()
{
    if (ref_count_ > 0) {
        --ref_count_;
    }
    return ref_count_;
}

int ImagePayload::ref_count() const
{
    return ref_count_;
}

ImagePayload *image_payload_get(const char *path_key)
{
    return find_payload_ptr(path_key);
}

const ImagePayload *image_payload_get(const image *img)
{
    if (ImagePayload *payload = payload_from_image(img)) {
        return payload;
    }
    return img ? find_payload_ptr(img->resource_key) : nullptr;
}

extern "C" const char *image_payload_acquire(image *img, const char *path_key)
{
    if (!img || !path_key || !*path_key) {
        return nullptr;
    }
    ImagePayload *payload = find_payload_ptr(path_key);
    if (!payload) {
        return nullptr;
    }
    if (img->resource_key && strcmp(img->resource_key, path_key) == 0 &&
        img->resource_handle == payload->handle()) {
        return img->resource_key;
    }

    release_image_binding(img, 1);
    payload->retain();
    bind_image_to_payload(img, payload);
    return img->resource_key;
}

extern "C" const char *image_payload_load_png(image *img, const char *path_key, const char *file_path)
{
    if (!img || !path_key || !*path_key || !file_path || !*file_path) {
        return nullptr;
    }

    if (find_payload_ptr(path_key)) {
        return image_payload_acquire(img, path_key);
    }

    if (!png_load_from_file(file_path, 0)) {
        return nullptr;
    }

    int width = 0;
    int height = 0;
    if (!png_get_image_size(&width, &height) || width <= 0 || height <= 0) {
        png_unload();
        return nullptr;
    }

    color_t *pixels = static_cast<color_t *>(malloc(sizeof(color_t) * width * height));
    if (!pixels) {
        png_unload();
        return nullptr;
    }

    if (!png_read(pixels, 0, 0, width, height, 0, 0, width, 0)) {
        free(pixels);
        png_unload();
        return nullptr;
    }
    png_unload();

    img->x_offset = 0;
    img->y_offset = 0;
    img->width = width;
    img->height = height;
    img->original.width = width;
    img->original.height = height;

    const graphics_renderer_interface *renderer = graphics_renderer();
    if (!renderer || !renderer->upload_image_resource) {
        free(pixels);
        return nullptr;
    }

    renderer->upload_image_resource(img, pixels, width, height);
    free(pixels);
    return image_payload_register(img, path_key);
}

ImagePayload *image_payload_load_png_payload(const char *path_key, const char *file_path)
{
    if (!path_key || !*path_key || !file_path || !*file_path) {
        return nullptr;
    }

    if (ImagePayload *existing = find_payload_ptr(path_key)) {
        existing->retain();
        return existing;
    }

    if (!png_load_from_file(file_path, 0)) {
        return nullptr;
    }

    int width = 0;
    int height = 0;
    if (!png_get_image_size(&width, &height) || width <= 0 || height <= 0) {
        png_unload();
        return nullptr;
    }

    std::unique_ptr<color_t[]> pixels(new (std::nothrow) color_t[static_cast<size_t>(width) * height]);
    if (!pixels) {
        png_unload();
        return nullptr;
    }

    if (!png_read(pixels.get(), 0, 0, width, height, 0, 0, width, 0)) {
        png_unload();
        return nullptr;
    }
    png_unload();

    const graphics_renderer_interface *renderer = graphics_renderer();
    if (!renderer || !renderer->upload_image_resource) {
        return nullptr;
    }

    image legacy_image = {};
    legacy_image.width = width;
    legacy_image.height = height;
    legacy_image.original.width = width;
    legacy_image.original.height = height;
    renderer->upload_image_resource(&legacy_image, pixels.get(), width, height);
    if (legacy_image.resource_handle <= 0) {
        return nullptr;
    }

    auto payload = std::make_unique<ImagePayload>(path_key, legacy_image);
    payload->set_handle(legacy_image.resource_handle);
    payload->retain();
    ImagePayload *payload_ptr = payload.get();
    g_payloads.emplace(payload_ptr->key(), std::move(payload));
    return payload_ptr;
}

extern "C" const char *image_payload_register(image *img, const char *path_key)
{
    if (!img || !path_key || !*path_key || img->resource_handle <= 0) {
        return nullptr;
    }

    ImagePayload *existing_payload = find_payload_ptr(path_key);
    image legacy_image = *img;
    if (existing_payload) {
        if (!(img->resource_key && strcmp(img->resource_key, path_key) == 0 &&
            img->resource_handle == existing_payload->handle())) {
            release_image_binding(img, img->resource_handle != existing_payload->handle());
            existing_payload->retain();
        }
        legacy_image.resource_handle = existing_payload->handle();
        existing_payload->sync_from_legacy(legacy_image);
        bind_image_to_payload(img, existing_payload);
        return img->resource_key;
    }

    release_image_binding(img, 0);
    auto payload = std::make_unique<ImagePayload>(path_key, legacy_image);
    payload->set_handle(legacy_image.resource_handle);
    payload->retain();
    ImagePayload *payload_ptr = payload.get();
    g_payloads.emplace(payload_ptr->key(), std::move(payload));
    bind_image_to_payload(img, payload_ptr);
    return img->resource_key;
}

extern "C" void image_payload_release(image *img)
{
    release_image_binding(img, 1);
}

extern "C" const char *image_payload_key(const image *img)
{
    if (const ImagePayload *payload = image_payload_get(img)) {
        return payload->key().c_str();
    }
    return img ? img->resource_key : nullptr;
}

extern "C" const image *image_payload_legacy_view(const image *img)
{
    if (const ImagePayload *payload = image_payload_get(img)) {
        return &payload->legacy();
    }
    return img;
}

extern "C" const image *image_payload_legacy_for_key(const char *path_key)
{
    if (const ImagePayload *payload = image_payload_get(path_key)) {
        return &payload->legacy();
    }
    return nullptr;
}

extern "C" image_handle image_payload_handle_for_key(const char *path_key)
{
    if (ImagePayload *payload = find_payload_ptr(path_key)) {
        return payload->handle();
    }
    return 0;
}

void image_payload_retain_key(const char *path_key)
{
    if (ImagePayload *payload = find_payload_ptr(path_key)) {
        payload->retain();
    }
}

void image_payload_release_key(const char *path_key)
{
    release_payload_iterator(find_payload(path_key));
}

extern "C" void image_payload_clear_all(void)
{
    g_payloads.clear();
}
