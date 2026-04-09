#ifndef WIDGET_CITY_DRAW_H
#define WIDGET_CITY_DRAW_H

#include "graphics/color.h"
#include "map/point.h"
#include "widget/city/city.h"

void city_draw(int selected_figure_id, pixel_coordinate *figure_coord,
                            const map_tile *tile, unsigned int roamer_preview_building_id);

color_t city_draw_get_color_mask(int grid_offset, int is_top);

void city_draw_building_footprint(int x, int y, int grid_offset, color_t color_mask);

void city_draw_building_top(int x, int y, int grid_offset, color_t color_mask);

#endif // WIDGET_CITY_DRAW_H
