#pragma once

#include "core/image.h"
#include "graphics/color.h"
#include "graphics/renderer.h"

#include <string_view>

class ImageGroupEntry;
class ImageGroupPayload;
struct RuntimeDrawSlice;

class UiPrimitives {
public:
    const image *resolve_image(int image_id) const;
    const ImageGroupPayload *resolve_image_group(std::string_view path_key, bool report_failure = true) const;
    const ImageGroupEntry *resolve_group_entry(
        const ImageGroupPayload *group,
        std::string_view group_key,
        int entry_index,
        bool report_failure = true) const;

    void draw_image(
        const image *img,
        float x,
        float y,
        int logical_width,
        int logical_height,
        color_t color,
        render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO) const;

    void draw_image_rect(
        int image_id,
        int x,
        int y,
        int logical_width,
        int logical_height,
        color_t color,
        render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO) const;
    void draw_runtime_slice(
        const RuntimeDrawSlice &slice,
        float x,
        float y,
        int logical_width,
        int logical_height,
        color_t color,
        render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO) const;

    int image_id_from_asset_names(std::string_view assetlist_name, std::string_view image_name) const;

    int save_region(int image_id, int x, int y, int width, int height) const;
    void draw_saved_region(int image_id, int x, int y) const;
    void push_renderer_state() const;
    void pop_renderer_state() const;
    void set_clip_rectangle(int x, int y, int width, int height) const;

private:
    int logical_size_or_native(int logical_size, int native_size) const;
};
