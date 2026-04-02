#include "panel.h"

#include "graphics/ui_runtime_api.h"

void outer_panel_draw(int x, int y, int width_blocks, int height_blocks)
{
    ui_runtime_draw_outer_panel(x, y, width_blocks, height_blocks);
}

void unbordered_panel_draw_colored(int x, int y, int width_blocks, int height_blocks, color_t color)
{
    ui_runtime_draw_unbordered_panel(x, y, width_blocks, height_blocks, color);
}

void unbordered_panel_draw(int x, int y, int width_blocks, int height_blocks)
{
    unbordered_panel_draw_colored(x, y, width_blocks, height_blocks, COLOR_MASK_NONE);
}

void inner_panel_draw(int x, int y, int width_blocks, int height_blocks)
{
    ui_runtime_draw_inner_panel(x, y, width_blocks, height_blocks);
}

void label_draw(int x, int y, int width_blocks, int type)
{
    ui_runtime_draw_label(x, y, width_blocks, type);
}

void large_label_draw(int x, int y, int width_blocks, int type)
{
    ui_runtime_draw_large_label(x, y, width_blocks, type);
}

int top_menu_black_panel_draw(int x, int y, int width)
{
    return ui_runtime_draw_top_menu_black_panel(x, y, width);
}
