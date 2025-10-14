#include "new_common.h"

#include "city/view.h"
#include "graphics/image.h"
#include "graphics/screen.h"
#include "widget/minimap.h"

int new_sidebar_common_get_x_offset_advanced(void)
{
    return screen_width() - SIDEBAR_ADVANCED_WIDTH;
}

int new_sidebar_common_get_height(void)
{
    return screen_height() - TOP_MENU_HEIGHT;
}

void new_sidebar_common_draw_relief(int x_offset, int y_offset, int image_id)
{
    // relief images below panel
    int image_base = image_group(image_id);
    int image_offset = image_id == GROUP_SIDE_PANEL ? 2 : 1;
    int y_max = screen_height();
    while (y_offset < y_max) {
        if (y_max - y_offset <= 120) {
            image_draw(image_base + image_offset, x_offset, y_offset, COLOR_MASK_NONE, SCALE_NONE);
            y_offset += 120;
        } else {
            image_draw(image_base + image_offset + image_offset, x_offset, y_offset,
                COLOR_MASK_NONE, SCALE_NONE);
            y_offset += 285;
        }
    }
}
