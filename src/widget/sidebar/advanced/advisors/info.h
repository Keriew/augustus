#ifndef WIDGET_SIDEBAR_INFO
#define WIDGET_SIDEBAR_INFO
#include "input/mouse.h"

void draw_info_panel_background(int x_offset, int y_offset);

void draw_housing_table(void);

void draw_gods_table(void);

void draw_health_table(void);

void draw_education_table(void);

void draw_entertainment_table(void);

int info_panel_mouse_handle(const mouse *m);

#endif
