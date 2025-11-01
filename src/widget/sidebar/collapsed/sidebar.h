#ifndef WIDGET_COLLAPSED_SIDEBAR
#define WIDGET_COLLAPSED_SIDEBAR

#include "graphics/tooltip.h"
#include "input/mouse.h"

int collapsed_sidebar_width(void);

void draw_collapsed_sidebar_background(int x_offset);
void draw_collapsed_sidebar_foreground(void);

int handle_collapsed_sidebar_mouse(const mouse *m);
int handle_collapsed_sidebar_mouse_build_menu( const mouse *m);

int calculate_x_offset_sidebar_collapsed(void);

unsigned int get_collapsed_sidebar_tooltip_text(tooltip_context *c);

#endif //  WIDGET_COLLAPSED_SIDEBAR
