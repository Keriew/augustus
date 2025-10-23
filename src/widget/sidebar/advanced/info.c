#include "info.h"

#include "graphics/graphics.h"
#include "graphics/panel.h"


void draw_infopanel_background(int x_offset, int y_offset, int width, int height)
{
    int panel_blocks = height / BLOCK_SIZE;
    graphics_draw_line(x_offset, x_offset, y_offset, y_offset + height, COLOR_WHITE);
    graphics_draw_line(x_offset + width - 1, x_offset + width - 1, y_offset,
        y_offset + height, COLOR_SIDEBAR);
    inner_panel_draw(x_offset + 1, y_offset, width / BLOCK_SIZE, panel_blocks);
}
