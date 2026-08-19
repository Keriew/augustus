#ifndef GRAPHICS_PANEL_H
#define GRAPHICS_PANEL_H

#include "graphics/color.h"

#define BLOCK_SIZE 16
#define BLACK_PANEL_BLOCK_WIDTH 20
#define BLACK_PANEL_MIDDLE_BLOCKS 4
#define LABEL_TYPE_HOVER 2
#define LABEL_TYPE_NORMAL 1

void outer_panel_draw(int x, int y, int width_blocks, int height_blocks);

void outer_panel_draw_colored(int x, int y, int width, int height, color_t color);

void scrollbar_panel_draw(int x, int y, int height_px);

void inner_panel_draw(int x, int y, int width_blocks, int height_blocks);

void inner_panel_draw_colored(int x, int y, int width, int height, color_t color);

void unbordered_panel_draw(int x, int y, int width_blocks, int height_blocks);

void unbordered_panel_draw_px(int x, int y, int width_px, int height_px);

void unbordered_panel_draw_colored(int x, int y, int width_blocks, int height_blocks, color_t color);

void bordered_panel_draw_colored(int x, int y, int width_px, int height_px, int has_focus, color_t color, color_t color_border);

void label_draw(int x, int y, int width_blocks, int type);

void large_label_draw(int x, int y, int width_blocks, int type);

void large_label_draw_custom_size(int x, int y, int width, int height);

void large_label_draw_bg(int x, int y, int width, int height);

/// @brief draws a pattern of greyed out diagonal lines
/// @param x 
/// @param y 
/// @param width 
/// @param height 
/// @param opacity 0-100 
void label_draw_greyout_pattern(int x, int y, int width, int height, int opacity);

void large_label_draw_border(int x, int y, int width, int height);

int top_menu_black_panel_draw(int x, int y, int width);

#endif // GRAPHICS_PANEL_H
