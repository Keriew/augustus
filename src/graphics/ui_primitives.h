#ifndef GRAPHICS_UI_PRIMITIVES_H
#define GRAPHICS_UI_PRIMITIVES_H

#include "core/image.h"
#include "graphics/color.h"
#include "graphics/renderer.h"

#include <string_view>

class UiPrimitives {
public:
    const image *resolve_image(int image_id) const;

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

    int image_id_from_asset_names(std::string_view assetlist_name, std::string_view image_name) const;

    int save_region(int image_id, int x, int y, int width, int height) const;
    void draw_saved_region(int image_id, int x, int y) const;

private:
    int logical_size_or_native(int logical_size, int native_size) const;
};

#endif // GRAPHICS_UI_PRIMITIVES_H
