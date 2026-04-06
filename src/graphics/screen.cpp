#include "screen.h"

#include "city/view.h"
#include "city/warning.h"
#include "core/calc.h"
#include "graphics/renderer.h"
#include "graphics/window.h"

static struct {
    int width;
    int height;
    int pixel_width;
    int pixel_height;
    int scale_percentage;
    struct {
        int x;
        int y;
    } dialog_offset;
} data;

static int normalize_scale_percentage(int scale_percentage)
{
    return scale_percentage > 0 ? scale_percentage : 100;
}

int screen_pixel_to_ui(int pixels)
{
    int inverse_scale = calc_percentage(100, normalize_scale_percentage(data.scale_percentage));
    return calc_adjust_with_percentage(pixels, inverse_scale);
}

int screen_ui_to_pixel(int ui)
{
    return calc_adjust_with_percentage(ui, normalize_scale_percentage(data.scale_percentage));
}

void screen_set_dialog_offset(int width, int height)
{
    data.dialog_offset.x = (data.width - width) / 2;
    data.dialog_offset.y = (data.height - height) / 2;
}

void screen_set_resolution(int pixel_width, int pixel_height, int scale_percentage)
{
    data.scale_percentage = normalize_scale_percentage(scale_percentage);
    data.pixel_width = pixel_width;
    data.pixel_height = pixel_height;
    data.width = screen_pixel_to_ui(pixel_width);
    data.height = screen_pixel_to_ui(pixel_height);
    screen_set_dialog_offset(640, 480);

    graphics_renderer()->clear_screen();
    graphics_renderer()->set_clip_rectangle(0, 0, data.width, data.height);

    city_view_set_viewport(pixel_width, pixel_height);
    city_warning_clear_all();
    window_invalidate();
}

int screen_width(void)
{
    return data.width;
}

int screen_height(void)
{
    return data.height;
}

int screen_pixel_width(void)
{
    return data.pixel_width;
}

int screen_pixel_height(void)
{
    return data.pixel_height;
}

int screen_scale_percentage(void)
{
    return normalize_scale_percentage(data.scale_percentage);
}

void screen_set_ui_render_scale(void)
{
    graphics_renderer()->set_render_domain(RENDER_DOMAIN_UI);
}

void screen_set_pixel_render_scale(void)
{
    graphics_renderer()->set_render_domain(RENDER_DOMAIN_PIXEL);
}

int screen_dialog_offset_x(void)
{
    return data.dialog_offset.x;
}

int screen_dialog_offset_y(void)
{
    return data.dialog_offset.y;
}
