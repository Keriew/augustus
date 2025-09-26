#ifndef BUILDING_CONSTRUCTION_CLEAR_H
#define BUILDING_CONSTRUCTION_CLEAR_H

#include "graphics/color.h"
/**
 * Clears land
 * @param measure_only Whether to measure only
 * @param x_start Start X
 * @param y_start Start Y
 * @param x_end End X
 * @param y_end End Y
 * @return Number of tiles cleared
 */
int building_construction_clear_land(int measure_only, int x_start, int y_start, int x_end, int y_end);

color_t building_construction_clear_color(void);

int building_construction_repair_land(int measure_only, int x_start, int y_start, int x_end, int y_end, int *buildings_count);

#endif // BUILDING_CONSTRUCTION_CLEAR_H
