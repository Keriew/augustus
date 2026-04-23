#ifndef WIDGET_CITY_WATER_GHOST_H
#define WIDGET_CITY_WATER_GHOST_H

#ifdef __cplusplus
extern "C" {
#endif

#include "building/type.h"

void city_water_ghost_draw_water_structure_ranges(void);
void city_water_ghost_draw_reservoir_ranges(void);
void city_water_ghost_draw_preview(building_type type, int primary_grid_offset, int secondary_grid_offset);

#ifdef __cplusplus
}
#endif

#endif // WIDGET_CITY_WATER_GHOST_H
