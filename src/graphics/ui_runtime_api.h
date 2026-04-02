#ifndef GRAPHICS_UI_RUNTIME_API_H
#define GRAPHICS_UI_RUNTIME_API_H

#include "graphics/color.h"
#include "graphics/image_button.h"
#include "graphics/scrollbar.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_runtime_draw_button_border(
    int x,
    int y,
    int width_pixels,
    int height_pixels,
    int has_focus,
    color_t color);
void ui_runtime_draw_one_row_button_border(
    int x,
    int y,
    int width_pixels,
    int height_pixels,
    int has_focus,
    color_t color);

void ui_runtime_draw_outer_panel(int x, int y, int width_blocks, int height_blocks);
void ui_runtime_draw_unbordered_panel(int x, int y, int width_blocks, int height_blocks, color_t color);
void ui_runtime_draw_inner_panel(int x, int y, int width_blocks, int height_blocks);
void ui_runtime_draw_label(int x, int y, int width_blocks, int type);
void ui_runtime_draw_large_label(int x, int y, int width_blocks, int type);
int ui_runtime_draw_top_menu_black_panel(int x, int y, int width);

void ui_runtime_draw_image_button(int x, int y, const image_button *button);
void ui_runtime_draw_scrollbar_dot(const scrollbar_type *scrollbar);
void ui_runtime_draw_slider(int x, int y, int width_pixels, int min_value, int max_value, int current_value);

int ui_runtime_save_to_image(int image_id, int x, int y, int width, int height);
void ui_runtime_draw_from_image(int image_id, int x, int y);

#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_UI_RUNTIME_API_H
