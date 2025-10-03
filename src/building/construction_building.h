#ifndef BUILDING_CONSTRUCTION_BUILDING_H
#define BUILDING_CONSTRUCTION_BUILDING_H

#include "building/type.h"
#include "map/grid.h"

typedef enum {
    CLEAR_MODE_FORCE = 0, //removes everything, even if not removable by player
    CLEAR_MODE_RUBBLE = 1, //removes only rubble
    CLEAR_MODE_TREES = 2, //removes only trees
    CLEAR_MODE_PLAYER = 3, //removes only things that player can clear, i.e. buildings, trees, rubble, roads etc.
} clear_mode;

typedef enum {
    COST_FREE = -1, // perform operation for free
    COST_MEASURE = 0, // measure cost only, do not perform operation
    COST_PROCESS = 1 // perform operation for a cost
}cost_calculation;
/**
 * @brief Places a building of the specified type at the given coordinates
 * CAREFUL: x and y are offset depending on city orientation, can cause problems with exact coordinates given
 * Doesn't process the finance, but checks the correct terrain and figure collisions
 * @param type Building type to place
 * @param x tile X coordinate for placement
 * @param y tile Y coordinate for placement
 * @param exact_coordinates If 1, x and y are used as exact coordinates without any offset adjustments
*/
int building_construction_place_building(building_type type, int x, int y, int exact_coordinates);
int building_construction_is_granary_cross_tile(int tile_no);
int building_construction_is_warehouse_corner(int tile_no);

/**
 *@brief Uses building_construction_place to fill all vacant lots in the specified area
* @param area area to fill
* @return number of lots filled
*/
int building_construction_fill_vacant_lots(grid_slice *area);

/**
 * @brief Prepares terrain for building construction by clearing the specified area
 *
 * @param grid_slice array of grid offsets representing the area to prepare
 * @param clear_mode Determines what types of terrain/objects to remove
 * @param cost Calculation mode for cost (free, measure only, or process)
 * @return Success/failure status of the terrain preparation
 */
int building_construction_prepare_terrain(grid_slice *grid_slice, clear_mode clear_mode, cost_calculation cost);

#endif // BUILDING_CONSTRUCTION_BUILDING_H


