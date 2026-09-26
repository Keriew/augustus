#ifndef WIDGET_EMPIRE_FUNDS_H
#define WIDGET_EMPIRE_FUNDS_H

#include "graphics/tooltip.h"
#include "input/mouse.h"

int widget_empire_funds_width_for_available(int available_width);

void widget_empire_funds_initialise(int x, int y, int width);

void widget_empire_funds_draw(int offset_x, int offset_y);

int widget_empire_funds_handle_mouse(const mouse *m, int offset_x, int offset_y);

int widget_empire_funds_handle_tooltip(tooltip_context *c);

int widget_empire_funds_width(void);

int widget_empire_funds_height(void);

#endif // WIDGET_EMPIRE_FUNDS_H
