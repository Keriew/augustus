#ifndef GRAPHICS_PANEL_H
#define GRAPHICS_PANEL_H

#include "graphics/color.h"

#define BLOCK_SIZE 16
#define BLACK_PANEL_BLOCK_WIDTH 20
#define BLACK_PANEL_MIDDLE_BLOCKS 4
#define LABEL_TYPE_HOVER 2
#define LABEL_TYPE_NORMAL 1

void outer_panel_draw(int x, int y, int width_blocks, int height_blocks);

void inner_panel_draw(int x, int y, int width_blocks, int height_blocks);

void unbordered_panel_draw(int x, int y, int width_blocks, int height_blocks);

void unbordered_panel_draw_colored(int x, int y, int width_blocks, int height_blocks, color_t color);

void label_draw(int x, int y, int width_blocks, int type);

void large_label_draw(int x, int y, int width_blocks, int type);

int top_menu_black_panel_draw(int x, int y, int width);

#endif // GRAPHICS_PANEL_H
