#ifndef BUILDING_CONSTRUCTION_BUILDING_H
#define BUILDING_CONSTRUCTION_BUILDING_H

#include "building/type.h"
#include "map/grid.h"

typedef enum {
    CLEAR_MODE_FORCE = 0, //removes everything, even if not removable by player
    CLEAR_MODE_RUBBLE = 1, //removes only rubble
    CLEAR_MODE_TREES = 2, //removes trees and shrubs
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

/**
 * Per-tile auto-clear of vegetation. If the auto-clear-trees option is enabled and
 * the tile contains TREE or SHRUB:
 *  - measure_only=0: removes those bits from live terrain and undo backup, sets the
 *    tile image backup to grass, charges 3 dn, and bumps the dry-run counter.
 *  - measure_only=1: leaves the terrain untouched but bumps the dry-run counter.
 * Returns the cost (3 or 0). Image-backup / routing refresh are deferred and
 * applied by auto_clear_finalize() so we pay the cost once per placement action.
 */
int building_construction_auto_clear_vegetation_at(int grid_offset, int measure_only);

/**
 * Finalize step paired with building_construction_auto_clear_vegetation_at.
 * If any tile was cleared since the previous finalize, refreshes the image backup
 * (so undo restores the cleared state visually) and the citizen routing grid.
 */
void building_construction_auto_clear_finalize(void);

/**
 * Computes all auto-clear footprints for placing `type` at (x_tl, y_tl), the
 * top-left corner of the main footprint after orientation adjustment. The
 * footprint size is derived from the building type (with warehouse overridden
 * to 3x3 to match the real placement path). Handles fort (3x3 body + 4x4
 * parade ground) and hippodrome (three 5x5 parts) special cases. Returns
 * total cost.
 *  - measure_only=0: clears trees + charges.
 *  - measure_only=1: counts cost without clearing; bumps the dry-run counter.
 */
int building_construction_auto_clear_for_building(building_type type, int x_tl, int y_tl,
    int measure_only);

/**
 * Resets the dry-run vegetation-cost counter. Call once at the start of a
 * preview pass before invoking any measure-only place_*** function.
 */
void building_construction_dry_run_vegetation_reset(void);

/**
 * Total cost accumulated by auto_clear_vegetation_at / auto_clear_for_building
 * calls since the most recent dry_run_vegetation_reset, in denarii. Used by the
 * construction cost-preview tooltip.
 */
int building_construction_dry_run_vegetation_cost(void);

#endif // BUILDING_CONSTRUCTION_BUILDING_H


