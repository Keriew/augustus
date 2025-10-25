#ifndef GRAPHICS_BUTTON_H
#define GRAPHICS_BUTTON_H

#include "graphics/color.h"

void button_none(int param1, int param2);

void button_border_draw(int x, int y, int width_pixels, int height_pixels, int has_focus);

void button_border_draw_colored(int x, int y, int width_pixels, int height_pixels, int has_focus, color_t color);

#endif // GRAPHICS_BUTTON_H
