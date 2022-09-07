#ifndef WIDGET_CITY_BUILDING_GHOST_H
#define WIDGET_CITY_BUILDING_GHOST_H

#include "building/type.h"
#include "map/point.h"

void city_building_ghost_set_type(building_type type);
int city_building_ghost_mark_deleting(const map_tile *tile);
void city_building_ghost_draw(const map_tile *tile);

#endif // WIDGET_CITY_BUILDING_GHOST_H
