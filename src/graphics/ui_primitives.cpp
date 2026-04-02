#include "graphics/ui_primitives.h"

extern "C" {
#include "assets/assets.h"
}

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
    request.x = x;
    request.y = y;
    request.logical_width = static_cast<float>(logical_width);
    request.logical_height = static_cast<float>(logical_height);
    request.color = color;
    request.domain = graphics_renderer()->get_render_domain();
    request.scaling_policy = scaling_policy;
    return request;
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
        return 0;
    }

    if (image_is_external(img)) {
        image_load_external_data(img);
    } else if ((img->atlas.id >> IMAGE_ATLAS_BIT_OFFSET) == ATLAS_UNPACKED_EXTRA_ASSET) {
        assets_load_unpacked_asset(image_id);
    }

    return img;
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
    return assets_get_image_id(assetlist_name.data(), image_name.data());
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
