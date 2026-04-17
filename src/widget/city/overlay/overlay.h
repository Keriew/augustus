#ifndef WIDGET_CITY_OVERLAY_H
#define WIDGET_CITY_OVERLAY_H

#include "building/building.h"
#include "figure/figure.h"
#include "graphics/tooltip.h"

#define NO_COLUMN -1

typedef enum {
    COLUMN_COLOR_GREEN = 0,
    COLUMN_COLOR_RED = 1,
    COLUMN_COLOR_GREEN_TO_RED = 2,
    COLUMN_COLOR_RED_TO_GREEN = 3
} column_color_type;

typedef struct {
    int type;
    column_color_type column_type;
    int (*show_building)(const building *b);
    int (*show_figure)(const figure *f);
    int (*get_column_height)(const building *b);
    int (*get_tooltip)(tooltip_context *c, int grid_offset);
    void (*draw_layer)(int x, int y, float scale, int grid_offset);
} city_overlay;

const city_overlay *city_overlay_get(void);

int city_overlay_get_tooltip_text(tooltip_context *c, int grid_offset);

#endif // WIDGET_CITY_OVERLAY_H
