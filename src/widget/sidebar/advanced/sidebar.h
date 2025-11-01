#ifndef WIDGET_ADVANCED_SIDEBAR
#define WIDGET_ADVANCED_SIDEBAR

#include "graphics/tooltip.h"
#include "input/mouse.h"

int advanced_sidebar_width(void);

void draw_advanced_sidebar_background(int x_offset);
void draw_advanced_sidebar_foreground(void);

int handle_advanced_sidebar_mouse(const mouse *m);
int handle_advanced_sidebar_mouse_build_menu( const mouse *m);

int calculate_x_offset_sidebar_advanced(void);

unsigned int get_advanced_sidebar_tooltip_text(tooltip_context *c);

#endif
