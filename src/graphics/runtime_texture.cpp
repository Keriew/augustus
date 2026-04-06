#include "graphics/runtime_texture.h"

namespace {

// Input: one runtime-native slice plus draw-space placement and render settings.
// Output: a renderer-native managed-texture request ready for the SDL backend.
managed_image_request make_managed_request(
    const RuntimeDrawSlice &slice,
    float x,
    float y,
    float logical_width,
    float logical_height,
    color_t color,
    render_domain domain,
    render_scaling_policy scaling_policy)
{
    managed_image_request request = {};
    request.handle = slice.handle;
    request.width = slice.width;
    request.height = slice.height;
    request.x_offset = slice.draw_offset_x;
    request.y_offset = slice.draw_offset_y;
    request.is_isometric = slice.is_isometric;
    request.x = x;
    request.y = y;
    request.logical_width = logical_width > 0.0f ? logical_width : static_cast<float>(slice.width);
    request.logical_height = logical_height > 0.0f ? logical_height : static_cast<float>(slice.height);
    request.color = color;
    request.domain = domain;
    request.scaling_policy = scaling_policy;
    return request;
}

} // namespace

// Input: one runtime-native slice and integer screen-space placement.
// Output: no return value; the managed texture is drawn through the native renderer path only.
void runtime_texture_draw(const RuntimeDrawSlice &slice, int x, int y, color_t color, float scale)
{
    if (!slice.is_valid()) {
        return;
    }

    render_domain domain = graphics_renderer()->get_render_domain();
    int is_pixel_domain = domain == RENDER_DOMAIN_PIXEL
        || domain == RENDER_DOMAIN_TOOLTIP_PIXEL
        || domain == RENDER_DOMAIN_SNAPSHOT_PIXEL;
    managed_image_request request = make_managed_request(
        slice,
        scale ? x / scale : static_cast<float>(x),
        scale ? y / scale : static_cast<float>(y),
        scale ? slice.width / scale : static_cast<float>(slice.width),
        scale ? slice.height / scale : static_cast<float>(slice.height),
        color,
        domain,
        is_pixel_domain ? RENDER_SCALING_POLICY_PIXEL_ART : RENDER_SCALING_POLICY_AUTO);
    graphics_renderer()->draw_managed_image_request(&request);
}

// Input: one runtime-native slice plus fully-specified logical dimensions and render state.
// Output: no return value; the slice is submitted directly to the renderer without legacy image helpers.
void runtime_texture_draw_request(
    const RuntimeDrawSlice &slice,
    float x,
    float y,
    float logical_width,
    float logical_height,
    color_t color,
    render_domain domain,
    render_scaling_policy scaling_policy)
{
    if (!slice.is_valid()) {
        return;
    }

    managed_image_request request = make_managed_request(
        slice,
        x,
        y,
        logical_width,
        logical_height,
        color,
        domain,
        scaling_policy);
    graphics_renderer()->draw_managed_image_request(&request);
}
