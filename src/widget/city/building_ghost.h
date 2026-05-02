#ifndef WIDGET_CITY_BUILDING_GHOST_H
#define WIDGET_CITY_BUILDING_GHOST_H

#include "map/point.h"
#include "widget/city/overlay/overlay.h"

const city_overlay *city_building_ghost_get_overlay(void);

int city_building_ghost_mark_deleting(const map_tile *tile);

void city_building_ghost_draw(const map_tile *tile);

#endif // WIDGET_CITY_BUILDING_GHOST_H
