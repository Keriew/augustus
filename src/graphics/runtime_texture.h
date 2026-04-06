#pragma once

#include "graphics/color.h"
#include "graphics/renderer.h"

#ifdef __cplusplus

struct RuntimeDrawSlice {
    image_handle handle = 0;
    int width = 0;
    int height = 0;
    int draw_offset_x = 0;
    int draw_offset_y = 0;
    int is_isometric = 0;

    int is_valid() const
    {
        return handle > 0 && width > 0 && height > 0;
    }
};

void runtime_texture_draw(const RuntimeDrawSlice &slice, int x, int y, color_t color, float scale);
void runtime_texture_draw_request(
    const RuntimeDrawSlice &slice,
    float x,
    float y,
    float logical_width,
    float logical_height,
    color_t color,
    render_domain domain,
    render_scaling_policy scaling_policy);

#endif
