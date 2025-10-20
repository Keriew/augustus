#ifndef WIDGET_SIDEBAR_COMMON_H
#define WIDGET_SIDEBAR_COMMON_H

#include "graphics/menu.h"

#define SIDEBAR_MAIN_SECTION_HEIGHT 450
#define SIDEBAR_FILLER_Y_OFFSET (SIDEBAR_MAIN_SECTION_HEIGHT + TOP_MENU_HEIGHT)
#define MINIMAP_WIDTH 146
#define MINIMAP_HEIGHT 111

int sidebar_common_get_height(void);
void sidebar_common_draw_relief(int x_offset, int y_offset, int image_id, int is_collapsed);

#endif // WIDGET_SIDEBAR_COMMON_H
