#ifndef BUILDING_CONSTRUCTION_BUILDING_H
#define BUILDING_CONSTRUCTION_BUILDING_H

#include "building/type.h"

typedef enum {
    CLEAR_MODE_FORCE = 0, //removes everything, even if not removable by player
    CLEAR_MODE_RUBBLE = 1, //removes only rubble
    CLEAR_MODE_TREES = 2, //removes only trees
    CLEAR_MODE_PLAYER = 3, //removes only things that player can clear, i.e. buildings, trees, rubble, roads etc.
} clear_mode;

int building_construction_place_building(building_type type, int x, int y);
int building_construction_is_granary_cross_tile(int tile_no);
int building_construction_is_warehouse_corner(int tile_no);

/**
 * Prepares terrain for building construction by clearing the specified area
 *
 * @param grid_offset Starting grid position for terrain clearing
 * @param size_x Horizontal size of the area to clear (in tiles)
 * @param size_y Vertical size of the area to clear (in tiles)
 * @param clear_mode Determines what types of terrain/objects to remove:
 *                   - CLEAR_MODE_FORCE: Removes everything, including non-player removable objects
 *                   - CLEAR_MODE_RUBBLE: Removes only rubble
 *                   - CLEAR_MODE_TREES: Removes only trees
 *                   - CLEAR_MODE_PLAYER: Removes only player-clearable objects (buildings, trees, rubble, roads)
 * @return Success/failure status of the terrain preparation
 */
int building_construction_prepare_terrain(int grid_offset, int size_x, int size_y, clear_mode clear_mode);

#endif // BUILDING_CONSTRUCTION_BUILDING_H
