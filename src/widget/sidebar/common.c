#include "common.h"

#include "city/view.h"
#include "graphics/image.h"
#include "graphics/screen.h"
#include "widget/minimap.h"

int sidebar_common_get_height(void)
{
    return screen_height() - TOP_MENU_HEIGHT;
}

void sidebar_common_draw_relief(int x_offset, int y_offset, int image_id, int is_collapsed, int scale_by)
{
    const int scale = 100 + scale_by;
    // relief images below panel
    const int image_base = image_group(image_id);
    const int image_offset = image_id == GROUP_SIDE_PANEL ? 2 : 1;
    const int y_max = screen_height();
    while (y_offset < y_max) {
        if (y_max - y_offset <= 120) {

            image_draw_scaled_from_corner(image_base + image_offset + is_collapsed, x_offset, y_offset, COLOR_MASK_NONE, scale);
            y_offset += 120;
        } else {
            image_draw_scaled_from_corner(image_base + image_offset + image_offset + is_collapsed, x_offset, y_offset,
                COLOR_MASK_NONE, scale);
            y_offset += 285;
        }
    }
}
