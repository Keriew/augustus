#include "graphics/ui_primitives.h"

#include "assets/image_group_entry.h"
#include "assets/image_group_payload.h"
#include "assets/assets.h"
#include "core/crash_context.h"
#include "graphics/runtime_texture.h"

#include <stdio.h>
#include <string>

namespace {

render_2d_request make_request(
    const image *img,
    float x,
    float y,
    int logical_width,
    int logical_height,
    color_t color,
    render_scaling_policy scaling_policy)
{
    render_2d_request request = {};
    request.img = img;
    request.handle = img ? img->resource_handle : 0;
    request.x = x;
    request.y = y;
    request.logical_width = static_cast<float>(logical_width);
    request.logical_height = static_cast<float>(logical_height);
    request.color = color;
    request.domain = graphics_renderer()->get_render_domain();
    request.scaling_policy = scaling_policy;
    return request;
}

std::string make_path_key(std::string_view path_key)
{
    return std::string(path_key.begin(), path_key.end());
}

void report_widget_group_error(const char *message, std::string_view group_key, int entry_index)
{
    char detail[256];
    if (entry_index >= 0) {
        snprintf(detail, sizeof(detail),
            "group=%.*s entry_index=%d domain=%d",
            static_cast<int>(group_key.size()), group_key.data(),
            entry_index,
            graphics_renderer()->get_render_domain());
    } else {
        snprintf(detail, sizeof(detail),
            "group=%.*s domain=%d",
            static_cast<int>(group_key.size()), group_key.data(),
            graphics_renderer()->get_render_domain());
    }
    error_context_report_error(message, detail);
}

} // namespace

int UiPrimitives::logical_size_or_native(int logical_size, int native_size) const
{
    return logical_size > 0 ? logical_size : native_size;
}

const image *UiPrimitives::resolve_image(int image_id) const
{
    if (image_id <= 0) {
        return 0;
    }

    const image *img = image_get(image_id);
    if (!img) {
        char detail[128];
        snprintf(detail, sizeof(detail),
            "image_id=%d domain=%d",
            image_id,
            graphics_renderer()->get_render_domain());
        error_context_report_error("Widget image id could not be resolved", detail);
        return 0;
    }

    if (image_is_external(img)) {
        image_load_external_data(img);
    } else if ((img->atlas.id >> IMAGE_ATLAS_BIT_OFFSET) == ATLAS_UNPACKED_EXTRA_ASSET) {
        assets_load_unpacked_asset(image_id);
    }

    return img;
}

const ImageGroupPayload *UiPrimitives::resolve_image_group(std::string_view path_key, bool report_failure) const
{
    if (path_key.empty()) {
        return nullptr;
    }

    const std::string normalized_key = make_path_key(path_key);
    if (!image_group_payload_load(normalized_key.c_str())) {
        if (report_failure) {
            report_widget_group_error("Widget image group could not be loaded", path_key, -1);
        }
        return nullptr;
    }

    const ImageGroupPayload *group = image_group_payload_get(normalized_key.c_str());
    if (!group && report_failure) {
        report_widget_group_error("Widget image group could not be resolved", path_key, -1);
    }
    return group;
}

const ImageGroupEntry *UiPrimitives::resolve_group_entry(
    const ImageGroupPayload *group,
    std::string_view group_key,
    int entry_index,
    bool report_failure) const
{
    if (!group) {
        if (report_failure) {
            report_widget_group_error("Widget image group entry could not be resolved", group_key, entry_index);
        }
        return nullptr;
    }

    const ImageGroupEntry *entry = group->entry_at_index(entry_index);
    if (!entry && report_failure) {
        report_widget_group_error("Widget image group entry could not be resolved", group_key, entry_index);
    }
    return entry;
}

void UiPrimitives::draw_image(
    const image *img,
    float x,
    float y,
    int logical_width,
    int logical_height,
    color_t color,
    render_scaling_policy scaling_policy) const
{
    if (!img) {
        return;
    }

    int effective_logical_width = logical_size_or_native(logical_width, img->width);
    int effective_logical_height = logical_size_or_native(logical_height, img->height);

    render_2d_request request = make_request(
        img,
        x,
        y,
        effective_logical_width,
        effective_logical_height,
        color,
        scaling_policy);
    graphics_renderer()->draw_image_request(&request);
}

void UiPrimitives::draw_runtime_slice(
    const RuntimeDrawSlice &slice,
    float x,
    float y,
    int logical_width,
    int logical_height,
    color_t color,
    render_scaling_policy scaling_policy) const
{
    if (!slice.is_valid()) {
        return;
    }

    const int effective_logical_width = logical_size_or_native(logical_width, slice.width);
    const int effective_logical_height = logical_size_or_native(logical_height, slice.height);
    runtime_texture_draw_request(
        slice,
        x,
        y,
        static_cast<float>(effective_logical_width),
        static_cast<float>(effective_logical_height),
        color,
        graphics_renderer()->get_render_domain(),
        scaling_policy);
}

void UiPrimitives::draw_image_rect(
    int image_id,
    int x,
    int y,
    int logical_width,
    int logical_height,
    color_t color,
    render_scaling_policy scaling_policy) const
{
    draw_image(resolve_image(image_id), static_cast<float>(x), static_cast<float>(y),
        logical_width, logical_height, color, scaling_policy);
}

int UiPrimitives::image_id_from_asset_names(std::string_view assetlist_name, std::string_view image_name) const
{
    if (assetlist_name.empty() || image_name.empty()) {
        return 0;
    }

    const int image_id = assets_get_image_id(assetlist_name.data(), image_name.data());
    if (!image_id) {
        char detail[256];
        snprintf(detail, sizeof(detail),
            "asset=%.*s image=%.*s domain=%d",
            static_cast<int>(assetlist_name.size()), assetlist_name.data(),
            static_cast<int>(image_name.size()), image_name.data(),
            graphics_renderer()->get_render_domain());
        error_context_report_error("Widget asset image could not be resolved", detail);
    }
    return image_id;
}

int UiPrimitives::save_region(int image_id, int x, int y, int width, int height) const
{
    return graphics_renderer()->save_image_from_screen_for_domain(
        graphics_renderer()->get_render_domain(), image_id, x, y, width, height);
}

void UiPrimitives::draw_saved_region(int image_id, int x, int y) const
{
    graphics_renderer()->draw_image_to_screen_for_domain(graphics_renderer()->get_render_domain(), image_id, x, y);
}

void UiPrimitives::push_renderer_state() const
{
    graphics_renderer()->push_state();
}

void UiPrimitives::pop_renderer_state() const
{
    graphics_renderer()->pop_state();
}

void UiPrimitives::set_clip_rectangle(int x, int y, int width, int height) const
{
    graphics_renderer()->set_clip_rectangle(x, y, width, height);
}
