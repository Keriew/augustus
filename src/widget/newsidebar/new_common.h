#ifndef WIDGET_NEW_SIDEBAR_COMMON_H
#define WIDGET_NEW_SIDEBAR_COMMON_H

#include "graphics/menu.h"

#define SIDEBAR_COLLAPSED_WIDTH 42
#define SIDEBAR_ADVANCED_WIDTH 204 // 162 + 42 (old values combined)
//#define SIDEBAR_MAIN_SECTION_HEIGHT 450
#define MINIMAP_WIDTH 146
#define MINIMAP_HEIGHT 111
#define SIDEBAR_FILLER_Y_OFFSET (MINIMAP_HEIGHT + TOP_MENU_HEIGHT)

int new_sidebar_common_get_x_offset_advanced(void);

int new_sidebar_common_get_height(void);

void new_sidebar_common_draw_relief(int x_offset, int y_offset, int image_id);

#endif // WIDGET_NEW_SIDEBAR_COMMON_H
