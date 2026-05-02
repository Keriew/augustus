#ifndef BUILDING_CONSTRUCTION_CLEAR_H
#define BUILDING_CONSTRUCTION_CLEAR_H

#define BUILDING_CONSTRUCTION_CLEAR_LAND_INTERRUPTED ((unsigned int) -1)

#include "graphics/color.h"

/**
 * Selects how many tiles would be cleared by clear land
 * @param x_start Start X
 * @param y_start Start Y
 * @param x_end End X
 * @param y_end End Y
 * @return Number of tiles cleared
 */
unsigned int building_construction_clear_select(int x_start, int y_start, int x_end, int y_end);

/**
 * Clears land
 * @param x_start Start X
 * @param y_start Start Y
 * @param x_end End X
 * @param y_end End Y
 * @return Number of tiles cleared
 */
unsigned int building_construction_clear_land(int x_start, int y_start, int x_end, int y_end);

/**
 * Gets the color used for highlighting clearing or repairing land
 * @return Color
 */
color_t building_construction_clear_color(void);

/**
 *@brief Repairs land and buildings on that land.
 *
 * @param measure_only if `1`, the function only calculates the cost and number of buildings that would be repaired without actually performing the repairs.
 * @param x_start Start X
 * @param y_start Start Y
 * @param x_end End X
 * @param y_end End Y
 * @param buildings_count Pointer to store the number of buildings that would be repaired
 * @return The total cost of repairs
 */
int building_construction_repair_land(int measure_only, int x_start, int y_start, int x_end, int y_end, int *buildings_count);

#endif // BUILDING_CONSTRUCTION_CLEAR_H
