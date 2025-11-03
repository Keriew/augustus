#ifndef WIDGET_EXPANDED_SIDEBAR
#define WIDGET_EXPANDED_SIDEBAR

#include "graphics/tooltip.h"
#include "input/mouse.h"

int expanded_sidebar_width(void);

void draw_expanded_sidebar_background(int x_offset);
void draw_expanded_sidebar_foreground(void);

int handle_expanded_sidebar_mouse(const mouse *m);
int handle_expanded_sidebar_mouse_build_menu( const mouse *m);

int calculate_x_offset_sidebar_expanded(void);

unsigned int get_expanded_sidebar_tooltip_text(tooltip_context *c);

#endif //  WIDGET_EXPANDED_SIDEBAR
