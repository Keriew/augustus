#include "button.h"

#include "graphics/panel.h"
#include "graphics/ui_runtime_api.h"

void button_border_draw_colored(int x, int y, int width_pixels, int height_pixels, int has_focus, color_t color);

void button_none(int param1, int param2)
{

}

void button_border_draw(int x, int y, int width_pixels, int height_pixels, int has_focus)
{
    button_border_draw_colored(x, y, width_pixels, height_pixels, has_focus, COLOR_MASK_NONE);
}

void button_border_draw_colored(int x, int y, int width_pixels, int height_pixels, int has_focus, color_t color)
{
    ui_runtime_draw_button_border(x, y, width_pixels, height_pixels, has_focus, color);
}
