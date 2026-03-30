#ifndef GRAPHICS_SCREEN_H
#define GRAPHICS_SCREEN_H

#include "graphics/color.h"

void screen_set_dialog_offset(int width, int height);

void screen_set_resolution(int pixel_width, int pixel_height, int scale_percentage);

color_t *screen_pixel(int x, int y);

int screen_width(void);

int screen_height(void);

int screen_pixel_width(void);

int screen_pixel_height(void);

int screen_scale_percentage(void);

int screen_pixel_to_ui(int pixels);

int screen_ui_to_pixel(int ui);

void screen_set_ui_render_scale(void);

void screen_set_pixel_render_scale(void);

int screen_dialog_offset_x(void);

int screen_dialog_offset_y(void);

#endif // GRAPHICS_SCREEN_H
