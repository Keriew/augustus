// Logging
#include "core/log.h"
#include "SDL.h"
// Internal
#include "movement.h"
#include "movement_internal.h"
#include "direction.h"
#include "combat.h"
#include "route.h"
#include "service.h"
// External
#include "building/properties.h"
#include "building/building.h"
#include "building/destruction.h"
#include "building/roadblock.h"
#include "core/calc.h"
#include "core/config.h"
#include "game/time.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/random.h"
#include "map/road_access.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"

// --- MAPS ---
/* Defines the 4 Cardinal Directions */
static const int CARDINAL_DIRECTIONS[4] = {
    DIR_TOP, DIR_LEFT, DIR_RIGHT, DIR_BOTTOM
};
// Defines the clockwise rotation sequence for 1-9 indices
static const direction_type DIR_CW_ROTOR[10] = {
    0, // Null/Error
    DIR_TOP, // DIR_TOP_LEFT rotates to DIR_TOP
    DIR_TOP_RIGHT, // DIR_TOP rotates to DIR_TOP_RIGHT
    DIR_RIGHT, // DIR_TOP_RIGHT rotates to DIR_RIGHT
    DIR_TOP_LEFT, // DIR_LEFT rotates to DIR_TOP_LEFT
    DIR_CENTER, // Stays DIR_CENTER
    DIR_BOTTOM_RIGHT, // DIR_RIGHT rotates to DIR_BOTTOM_RIGHT
    DIR_LEFT, // DIR_BOTTOM_LEFT rotates to DIR_LEFT
    DIR_BOTTOM_LEFT, // DIR_BOTTOM rotates to DIR_BOTTOM_LEFT
    DIR_BOTTOM // DIR_BOTTOM_RIGHT rotates to DIR_BOTTOM
};
// Defines the counter-clockwise rotation sequence for 1-9 indices
static const direction_type DIR_CCW_ROTOR[10] = {
    0, // Null/Error
    DIR_LEFT, // DIR_TOP_LEFT rotates to DIR_LEFT
    DIR_TOP_LEFT, // DIR_TOP rotates to DIR_TOP_LEFT
    DIR_TOP, // DIR_TOP_RIGHT rotates to DIR_TOP
    DIR_BOTTOM_LEFT, // DIR_LEFT rotates to DIR_BOTTOM_LEFT
    DIR_CENTER, // Stays DIR_CENTER
    DIR_TOP_RIGHT, // DIR_RIGHT rotates to DIR_TOP_RIGHT
    DIR_BOTTOM, // DIR_BOTTOM_LEFT rotates to DIR_BOTTOM
    DIR_BOTTOM_RIGHT, // DIR_BOTTOM rotates to DIR_BOTTOM_RIGHT
    DIR_RIGHT // DIR_BOTTOM_RIGHT rotates to DIR_RIGHT
};

// --- HELPER FUNCTIONS ---
/* Filters road options for complex shapes (Double-wide roads, Plazas) */
// Also returns the new count of valid roads.
// You can word these comments better right?
static int filter_complex_road_topology(int grid_offset, int adjacent_count, int *road_tiles, direction_type came_from)
{
    // Check for Double-Wide Roads (3 exits, specific diagonals)
    if (adjacent_count == 3 && map_get_diagonal_road_tiles_for_roaming(grid_offset, road_tiles) >= 5) {
        // Force straight movement on double-wide roads
        if (came_from == DIR_TOP || came_from == DIR_BOTTOM) {
            road_tiles[DIR_LEFT] = road_tiles[DIR_RIGHT] = 0;
        } else {
            road_tiles[DIR_TOP] = road_tiles[DIR_BOTTOM] = 0;
        }
        return 2;
    }

    // Check for Plazas/Open Roads (4 exits, many diagonals)
    if (adjacent_count == 4 && map_get_diagonal_road_tiles_for_roaming(grid_offset, road_tiles) >= 8) {
        // Force straight movement
        if (came_from == DIR_TOP || came_from == DIR_BOTTOM) {
            road_tiles[DIR_LEFT] = road_tiles[DIR_RIGHT] = 0;
        } else {
            road_tiles[DIR_TOP] = road_tiles[DIR_BOTTOM] = 0;
        }
        return 2;
    }

    return adjacent_count;
}
// Determines if a road can be roamed through or not
static int is_valid_road_for_roaming(int grid_offset, int permission)
{
    // If terrain is not a road it is never valid
    if (!map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        return 0;
    }
    // If the road is clear (not under a building) then it is valid
    if (!map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        return 1;
    }
    // If the road is NOT under a roadblock then it is not valid
    building *b = building_get(map_building_at(grid_offset));
    if (!building_type_is_roadblock(b->type)) {
        return 0;
    }
    // Check for figure permissions to enter the roadblock
    int has_permission = building_roadblock_get_permission(permission, b);
    if (has_permission) {
        // If the building would be a dead-end, do not wander into it
        if (b->type == BUILDING_GRANARY) {
            // granary stuff goes here
            // prevents them from wandering into dead ends within the granary
        }
        // Permission is valid
        return 1;
    }
    // Permission is not valid
    return 0;
}
// Determines the next direction based on available roads
static direction_type pick_next_roam_direction(figure *f, int *road_tiles, int adjacent_count, direction_type came_from, int permission)
{
    // Case 0: Dead End
    if (adjacent_count <= 0) {
        return DIR_NULL;
    }

    // Case 1: Only one way out (Cul-de-sac or corner)
    if (adjacent_count == 1) {
        // Find the only valid cardinal direction
        if (road_tiles[DIR_TOP]) return DIR_TOP;
        if (road_tiles[DIR_RIGHT]) return DIR_RIGHT;
        if (road_tiles[DIR_BOTTOM]) return DIR_BOTTOM;
        if (road_tiles[DIR_LEFT]) return DIR_LEFT;
    }

    // Case 2: Two ways out (Straight or simple corner)
    if (adjacent_count == 2) {
        // Initialize decision logic if needed
        if (f->roam_ticks_until_next_turn == -1) {
            roam_set_direction(f, permission);
            // Reset came_from so we don't filter based on it immediately
            came_from = DIR_NULL;
        }

        // Try to continue current direction first
        if (road_tiles[f->direction] && f->direction != came_from) {
            return f->direction;
        }

        // Apply Turn Bias (Clockwise or Counter-Clockwise)
        direction_type next_dir = f->direction;
        for (int i = 0; i < 4; i++) {
            if (f->roam_turn_direction == ROAM_TURN_CW) {
                next_dir = DIR_CW_ROTOR[next_dir];
            } else {
                next_dir = DIR_CCW_ROTOR[next_dir];
            }

            if (road_tiles[next_dir] && next_dir != came_from) {
                return next_dir;
            }
        }
    }

    // Case 3: Intersection (> 2 ways)
    // Use random walker logic
    direction_type random_dir = (f->roam_random_counter + map_random_get(f->grid_offset)) & 3;
    // Note: You'll need a way to map 0-3 back to Cardinal directions (2, 6, 8, 4)
    // For now, we assume existing logic handles the randomness, or we iterate.

    // ... (Simplified logic for brevity: Use existing logic or roam_set_direction)

    // Fallback: Re-run direction logic
    f->roam_ticks_until_next_turn--;
    if (f->roam_ticks_until_next_turn <= 0) {
        roam_set_direction(f, permission);
        return f->direction; // roam_set_direction sets f->direction
    }

    return f->direction;
}
// Initializes the first roaming destination
void figure_movement_init_roaming(figure *f)
{
    building *b = building_get(f->building_id);
    f->progress_on_tile = TICKS_PER_TILE;
    f->roam_choose_destination = 0;
    f->roam_ticks_until_next_turn = -1;
    f->roam_turn_direction = ROAM_TURN_LEFT;
    // Disables corner skipping for roamers depending on configuration
    if (config_get(CONFIG_GP_CH_ROAMERS_DONT_SKIP_CORNERS)) {
        f->disallow_diagonal = 1;
    }
    // Get the current cycle index (0, 1, 2, or 3) from the building's direction field.
    b->figure_roam_direction += 2;
    if (b->figure_roam_direction > 6) {
        b->figure_roam_direction = 0;
    }

    // Map the cycle index (0-3) to the correct new Keypad direction index (2, 6, 8, 4)
    int new_dir_keypad = ROAM_CARDINAL_DIRECTIONS[roam_dir_index % 4];

    // Get the position from the figure object
    int x = f->x;
    int y = f->y;

    switch (new_dir_keypad) {
        case DIR_TOP:
            y += ROAM_INITIAL_OFFSET;
            break;
        case DIR_BOTTOM:
            y -= ROAM_INITIAL_OFFSET;
            break;
        case DIR_RIGHT:
            x += ROAM_INITIAL_OFFSET;
            break;
        case DIR_LEFT:
            x -= ROAM_INITIAL_OFFSET;
            break;
    }
    // Provides an offset so they don't spawn inside the building (?)
    switch (roam_dir) {
        case DIR_TOP: y += ROAM_INITIAL_OFFSET; break;
        case DIR_BOTTOM: y -= ROAM_INITIAL_OFFSET; break;
        case DIR_RIGHT: x += ROAM_INITIAL_OFFSET; break;
        case DIR_LEFT: x -= ROAM_INITIAL_OFFSET; break;
    }
    // Sets destination for the closest road, if it is within 6 tiles
    map_grid_bound(&x, &y);
    int x_road, y_road;
    if (map_closest_road_within_radius(x, y, 1, 6, &x_road, &y_road)) {
        f->destination_x = x_road;
        f->destination_y = y_road;
    } else {
        f->roam_choose_destination = 1;
    }
}
// Sets roam direction
static void roam_set_direction(figure *f, int permission)
{
    // Calculate position (grid_offset) and direction
    int grid_offset = map_grid_offset(f->x, f->y);
    direction_type target_dir = calc_general_direction(f->x, f->y, f->destination_x, f->destination_y);

    // Handle Invalid/Non-Movement Fallback (If direction > 9, it's a status flag)
    if (target_dir > DIR_MAX_MOVEMENT) {
        target_dir = DIR_NULL;
    }

    // Define Angular offset (clockwise and counter-clockwise)
    int cw_angular_offset = 0; // Angular distance (clockwise)
    int ccw_angular_offset = 0; // Angular distance (counter-clockwise)
    direction_type cw_direction = DIR_NULL; // Best direction found (clockwise)
    direction_type ccw_direction = DIR_NULL; // Best direction found (counter-clockwise)

    // Loop 1: Search clockwise (cw)
    for (int i = 0, current_dir = target_dir; i < DIR_INDEX_WRAP; i++) {
        // Check only cardinal directions (2, 4, 6, 8)
        if (CARDINAL_CHECK(current_dir) && is_valid_road_for_roaming(grid_offset + map_grid_direction_delta((direction_type) current_dir), permission)) {
            cw_direction = (direction_type) current_dir;
            break;
        }
        // Correct Rotation: Use the lookup table for the next direction
        current_dir = DIR_CW_ROTOR[current_dir];
        cw_angular_offset++;
    }

    // Loop 2: Search counter-clockwise (ccw)
    for (int i = 0, current_dir = target_dir; i < DIR_INDEX_WRAP; i++) {
        // Check only cardinal directions (2, 4, 6, 8)
        if (CARDINAL_CHECK(current_dir) && is_valid_road_for_roaming(grid_offset + map_grid_direction_delta((direction_type) current_dir), permission)) {
            ccw_direction = (direction_type) current_dir;
            break;
        }
        // Correct Rotation: Use the lookup table for the next direction
        current_dir = DIR_CCW_ROTOR[current_dir];
        ccw_angular_offset++;
    }

    // 3. Final Decision: Choose the smaller offset
    if (cw_angular_offset <= ccw_angular_offset) {
        f->direction = cw_direction;
        f->roam_turn_direction = ROAM_TURN_CW;
    } else {
        f->direction = ccw_direction;
        f->roam_turn_direction = ROAM_TURN_CCW;
    }

    // Assuming ROAM_DECISION_INTERVAL is defined in a header
    f->roam_ticks_until_next_turn = ROAM_DECISION_INTERVAL;
}

// Outdated???
void figure_movement_path(figure *f, int num_ticks)
{
    walk_ticks(f, num_ticks, 1);
}

// -- CORE FUNCTION --
/* Handles roaming movement */
void figure_movement_roam_ticks(figure *f, int num_ticks)
{
    // 1. Check if we need to initialize a new roam destination
    if (f->roam_choose_destination == 0) {
        walk_ticks(f, num_ticks, 1); // Walk a bit to see if we hit something

        // If status changed to Arrived, Reroute, or Lost
        if (f->direction == DIR_FIGURE_AT_DESTINATION ||
            f->direction == DIR_FIGURE_REROUTE ||
            f->direction == DIR_FIGURE_LOST) {
            f->roam_choose_destination = 1;
        }

        if (f->roam_choose_destination) {
            f->roam_ticks_until_next_turn = 100;
            f->roam_length = 0; // Reset length
            // Assuming previous_tile_direction is valid 1-9
            f->direction = f->previous_tile_direction;
        } else {
            return; // Continue walking current path
        }
    }

    // 2. Main Movement Loop
    while (num_ticks-- > 0) {
        f->progress_on_tile++;

        // A. Mid-tile movement
        if (f->progress_on_tile < TICKS_PER_TILE) {
            advance_tick(f);
            continue;
        }

        // B. End-of-tile Logic (The Decision Point)
        f->progress_on_tile = TICKS_PER_TILE;
        f->roam_random_counter++;

        // Provide service coverage (if applicable)
        if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
            if (figure_service_provide_coverage(f)) {
                return; // Walker finished job (e.g., Market Lady found food)
            }
        }

        // Analyze surroundings
        // IMPORTANT: Ensure road_tiles is size 10 to handle indices 1-9
        int road_tiles[10] = { 0 };
        int permission = get_permission_for_figure_type(f);

        // NOTE: map_get_adjacent... likely needs updating to fill indices 2,4,6,8 
        // instead of 0,2,4,6. Wrapper needed if map.c isn't updated.
        int adjacent_count = map_get_adjacent_road_tiles_for_roaming(f->grid_offset, road_tiles, permission);

        direction_type came_from = get_opposite_direction(f->previous_tile_direction);

        // Filter topology (Plazas/Double-wide roads)
        adjacent_count = filter_complex_road_topology(f->grid_offset, adjacent_count, road_tiles, came_from);

        // Dead End Check
        if (adjacent_count <= 0) {
            f->roam_length = f->max_roam_length; // Trigger end of roam
            return;
        }

        // Pick new direction
        direction_type next_dir = pick_next_roam_direction(f, road_tiles, adjacent_count, came_from, permission);

        if (next_dir == DIR_NULL) {
            // Fallback if logic failed to find path
            f->roam_length = f->max_roam_length;
            return;
        }

        // Apply Move
        f->direction = next_dir;
        f->routing_path_current_tile++;
        f->previous_tile_direction = f->direction;
        f->progress_on_tile = 0;

        move_to_next_tile(f);

        if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
            advance_tick(f);
        }
    }
}