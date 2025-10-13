#ifndef WIDGET_NEW_SIDEBAR_FILLER_H
#define WIDGET_NEW_SIDEBAR_FILLER_H

#include "graphics/tooltip.h"
#include "input/mouse.h"

typedef enum {
    NEW_SIDEBAR_EXTRA_DISPLAY_NONE = 0,
    NEW_SIDEBAR_EXTRA_DISPLAY_GAME_SPEED = 1,
    NEW_SIDEBAR_EXTRA_DISPLAY_UNEMPLOYMENT = 2,
    NEW_SIDEBAR_EXTRA_DISPLAY_INVASIONS = 4,
    NEW_SIDEBAR_EXTRA_DISPLAY_GODS = 8,
    NEW_SIDEBAR_EXTRA_DISPLAY_RATINGS = 16,
    NEW_SIDEBAR_EXTRA_DISPLAY_REQUESTS = 32,
    NEW_SIDEBAR_EXTRA_DISPLAY_ALL = 63
} new_sidebar_extra_display;

/**
 * @return The actual height of the extra info
 */
int new_sidebar_extra_draw_background(int x_offset, int y_offset, int width, int height,
    int is_collapsed, new_sidebar_extra_display info_to_display);
void new_sidebar_extra_draw_foreground(void);

int new_sidebar_extra_handle_mouse(const mouse *m);

int new_sidebar_extra_get_tooltip(tooltip_context *c);

int new_sidebar_extra_is_information_displayed(new_sidebar_extra_display display);

#endif // WIDGET_NEW_SIDEBAR_FILLER_H
