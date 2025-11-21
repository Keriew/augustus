// Logging
#include "core/log.h"
#include "SDL.h"
// Internal
#include "movement.h"
#include "figure.h"
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

// --- STATIC CONSTANTS ---
/* Direction Deltas */
const point_2d DIR_DELTAS[10] = {
{0, 0}, // Null
{-1, 1}, // Top left
{0, 1}, // Top
{1, 1}, // Top right
{-1, 0}, // Left
{0, 0}, // Center
{1, 0}, // Right
{-1, -1}, // Bottom left
{0, -1}, // Bottom
{1, -1} // Bottom right
};
// Defines the opposite direction for 1-9 indices
static const direction_type DIR_OPPOSITE_MAP[10] = {
    // Index 0: Unused
    [0] = 0,
    // Corners (Diagonals)
    [DIR_TOP_LEFT] = DIR_BOTTOM_RIGHT,
    [DIR_TOP_RIGHT] = DIR_BOTTOM_LEFT,
    [DIR_BOTTOM_LEFT] = DIR_TOP_RIGHT,
    [DIR_BOTTOM_RIGHT] = DIR_TOP_LEFT,
    // Cardinals
    [DIR_TOP] = DIR_BOTTOM,
    [DIR_BOTTOM] = DIR_TOP,
    [DIR_LEFT] = DIR_RIGHT,
    [DIR_RIGHT] = DIR_LEFT,
    // Center
    [DIR_CENTER] = DIR_CENTER
};
// Defines roadblock permissions
static const roadblock_permission FIGURE_PERMISSIONS[FIGURE_TYPE_COUNT] = {
    // Maintenance
    [FIGURE_ENGINEER] = PERMISSION_MAINTENANCE,
    [FIGURE_PREFECT] = PERMISSION_MAINTENANCE,
    // Entertainers
    [FIGURE_GLADIATOR] = PERMISSION_ENTERTAINER,
    [FIGURE_CHARIOTEER] = PERMISSION_ENTERTAINER,
    [FIGURE_ACTOR] = PERMISSION_ENTERTAINER,
    [FIGURE_LION_TAMER] = PERMISSION_ENTERTAINER,
    [FIGURE_BARKEEP] = PERMISSION_ENTERTAINER,
    // Medicine
    [FIGURE_SURGEON] = PERMISSION_MEDICINE,
    [FIGURE_DOCTOR] = PERMISSION_MEDICINE,
    [FIGURE_BARBER] = PERMISSION_MEDICINE,
    [FIGURE_BATHHOUSE_WORKER] = PERMISSION_MEDICINE,
    // Education
    [FIGURE_SCHOOL_CHILD] = PERMISSION_EDUCATION,
    [FIGURE_TEACHER] = PERMISSION_EDUCATION,
    [FIGURE_LIBRARIAN] = PERMISSION_EDUCATION,
    // Unique Walkers
    [FIGURE_PRIEST] = PERMISSION_PRIEST,
    [FIGURE_MARKET_TRADER] = PERMISSION_MARKET,
    [FIGURE_TAX_COLLECTOR] = PERMISSION_TAX_COLLECTOR,
    [FIGURE_LABOR_SEEKER] = PERMISSION_LABOR_SEEKER,
    [FIGURE_MISSIONARY] = PERMISSION_MISSIONARY,
    [FIGURE_WATCHMAN] = PERMISSION_WATCHMAN
};

// --- HELPER FUNCTIONS ---
/* Updates the figure's sub-tile coordinates based on its current direction. */
static void apply_sub_tile_movement(figure *f)
{
    // Only apply movement if the direction is a valid movement direction (1 to 8).
    // Directions > DIR_MAX_MOVEMENT (like ATTACK or REROUTE) should not change position.
    if (f->direction <= DIR_MAX_MOVEMENT) {
        // DIR_DELTAS is the map of sub-tile coordinate changes for each direction.
        f->cross_country_x += DIR_DELTAS[f->direction].x;
        f->cross_country_y += DIR_DELTAS[f->direction].y;
    }
}
// Defines how a figure will move on a given movement (tiles take 15 movements to move through)
static void advance_mov(figure *f)
{
    // 1. Apply the core physics movement for this tick
    apply_sub_tile_movement(f);

    // 2. Handle height adjustments and ghost state
    if (f->height_adjusted_mov) {
        f->height_adjusted_mov--;
        if (f->height_adjusted_mov > 0) {
            f->is_ghost = 1;
            if (f->current_height < f->target_height) {
                f->current_height++;
            }
            if (f->current_height > f->target_height) {
                f->current_height--;
            }
        } else {
            f->is_ghost = 0;
        }
    } else {
        if (f->current_height) {
            f->current_height--;
        }
    }
}
static void set_target_height_bridge(figure *f)
{
    f->height_adjusted_ticks = 18;
    f->target_height = map_bridge_height(f->grid_offset);
}

// Checks if a roaming figure is blocked by a building
static int is_roaming_blocked_by_building(figure *f, int target_position, int after_target_position)
{
    if (!map_terrain_is(target_position, TERRAIN_BUILDING)) {
        return 0; // Not blocked by a building if there isn't one
    }

    building *b = building_get(map_building_at(target_position));

    // 1. Granary Dead-End Check
    if (b->type == BUILDING_GRANARY) {
        // Don't roam into a granary tile if the next tile is not a road
        // Makes it possible to build a granary without creating a pointless intersection
        // Allows the granary to connect two or three roads without forming a full cross
        if (!map_terrain_is(after_target_position, TERRAIN_ROAD)) {
            return 1;
        }
    }

    // 2. Roadblock Permission Check
    if (building_type_is_roadblock(b->type)) {
        int permission = get_permission_for_figure_type(f);
        if (!building_roadblock_get_permission(permission, b)) {
            return 1; // Blocked: No permission
        }
    }

    return 0; // Not blocked
}

// Handles enemy interaction with obstacles
static void attempt_obstacle_destruction(figure *f, int target_position)
{
    if (!map_routing_is_destroyable(target_position)) {
        return;
    }

    int max_damage = 0;
    int cause_damage = 1;
    int destroyable_type = map_routing_get_destroyable(target_position);

    // 1. Calculate Max Damage based on type
    if (destroyable_type == DESTROYABLE_BUILDING) {
        building *b = building_get(map_building_at(target_position));
        max_damage = get_obstacle_hp(b->type);
    } else if (destroyable_type == DESTROYABLE_AQUEDUCT_GARDEN) {
        if (map_terrain_is(target_position, TERRAIN_ACCESS_RAMP | TERRAIN_RUBBLE)) {
            cause_damage = 0;
        } else {
            max_damage = BUILDING_HP;
        }
    } else if (destroyable_type == DESTROYABLE_WALL) {
        max_damage = WALL_HP;
    } else if (destroyable_type == DESTROYABLE_GATEHOUSE) {
        max_damage = GATEHOUSE_HP;
    }

    // 2. Apply Attack
    if (cause_damage && max_damage > 0) {
        f->attack_direction = f->direction;
        f->direction = DIR_FIGURE_ATTACK;
        // Apply damage on specific ticks (bitmask 3 checks every 4th tick)
        if (!(game_time_tick() & 3)) {
            building_destroy_increase_enemy_damage(target_position, max_damage);
        }
    }
}
// Updates the figure coordinates given their new direction
static void update_figure_coordinates(figure *f)
{
    // Update the figure's x and y coordinate
    f->x += DIR_DELTAS[f->direction].x;
    f->y += DIR_DELTAS[f->direction].y;
    // Update the figure's grid offset (effectively a single number describing their x and y coordinate)
    f->grid_offset += map_grid_direction_delta(f->direction);
}
// Delete the figure when exiting a tile (unless it is a preview figure)
static void update_map_figure_state_on_tile_exit(figure *f)
{
    if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
        map_figure_delete(f);
    }
}
// Add the figure when entering a tile (unless it is a preview figure)
static void update_map_figure_state_on_tile_entry(figure *f)
{
    if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
        map_figure_add(f);
    }
}
static void handle_new_tile_environment(figure *f)
{
    // Check Road/Bridge Interaction
    if (map_terrain_is(f->grid_offset, TERRAIN_ROAD)) {
        f->is_on_road = 1;
        // If it's a road AND water, it must be a bridge, so set height.
        if (map_terrain_is(f->grid_offset, TERRAIN_WATER)) {
            set_target_height_bridge(f);
        }
    } else {
        // Clear road flag if not on a road
        f->is_on_road = 0;
    }

    // Check for Combat
    if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
        figure_combat_attack_figure_at(f, f->grid_offset);
    }
}

// --- CORE MOVEMENT & STATE FUNCTIONS ---
/* Gets roadblock permissions for the figure */
static roadblock_permission get_permission_for_figure_type(figure *f)
{
    // Safety check: Ensures the figure type is a valid index for the array.
    if (f->type >= 0 && f->type < FIGURE_TYPE_COUNT) {
        return FIGURE_PERMISSIONS[f->type];
    }
    // Default case
    return PERMISSION_NONE;
}
// Moves the figure to the next tile
static void move_to_next_tile(figure *f)
{
    // A. Store old state for subsequent logic
    const int old_x = f->x;
    const int old_y = f->y;

    // 1. Update Map State (Remove figure from old tile)
    update_map_figure_state_on_tile_exit(f);

    // 2. Update Coordinates (Calculate new position)
    update_figure_coordinates(f);

    // 3. Update Map State (Add figure to new tile)
    update_map_figure_state_on_tile_entry(f);

    // 4. Update Terrain State & Events (Check roads/bridges/combat)
    handle_new_tile_environment(f);

    // 5. Final State Update (Set previous tile coordinates)
    f->previous_tile_x = old_x;
    f->previous_tile_y = old_y;
}

// Checks for collisions (also handles rerouting and destruction)
static void check_collision(figure *f, int roaming_enabled, int target_position, int after_target_position)
{
    // CASE A: TILE NOT ADJACENT
    if (f->direction > DIR_MAX_MOVEMENT) {
        return; // No collision possible, no further calculations
    }

    // CASE B: BOAT
    if (f->is_boat) {
        if (!map_terrain_is(target_position, TERRAIN_WATER)) {
            f->direction DIR_FIGURE_REROUTE; // Target is not water, reroute
        }
        return; // No further calculations for boats
    }

    // CASE C: ENEMY
    if (f->terrain_usage == TERRAIN_USAGE_ENEMY) {
        if (!map_routing_noncitizen_is_passable(target_position)) {
            f->direction = DIR_FIGURE_REROUTE; // Terrain is impassable, as opposed to passable or destructible
        } else {
            attempt_obstacle_destruction(f, target_position); // Terrain is either passable or destructible
        }
        return; // No further calculations for enemies
    }

    // CASE D: TOWER SENTRY WALKING INTO TOWER
    if (f->terrain_usage == TERRAIN_USAGE_WALLS) {
        if (!map_routing_is_wall_passable(target_position)) {
            f->direction = DIR_FIGURE_REROUTE; // Target is not a passable wall tile, reroute
        }
        return; // No further calculations for tower sentries
    }

    // CASE E: STANDARD WALKER
    // 1. Terrain must be passable
    if (map_terrain_is(target_position, TERRAIN_IMPASSABLE)) {
        f->direction = DIR_FIGURE_REROUTE;
        return;
    }

    // 2. If roaming, a building must not block the figure
    if (roaming_enabled && is_roaming_blocked_by_building(f, target_position, after_target_position)) {
        f->direction = DIR_FIGURE_REROUTE;
        return;
    }

    // 3. If it's a building, it must be passable by the figure
    if (map_terrain_is(target_position, TERRAIN_BUILDING)) {
        // Check 1: If the building is passable terrain, let them through
        if (map_routing_citizen_is_passable_terrain(target_position)) {
            return;
        }
        // Check 2: If the building is a road AND the figure isn't roaming, let them through
        const int is_road = map_routing_citizen_is_road(target_position);
        if (is_road && !roaming_enabled) {
            return;
        }

        // Check 3: If the building is a fort ground, let them through
        building *b = building_get(map_building_at(target_position));
        if (b->type == BUILDING_FORT_GROUND) {
            return;
        }

        // DENY ACCESS: If none of the above conditions returned, reroute.
        f->direction = DIR_FIGURE_REROUTE;
        return;
    }
}

// Calculates the next movement direction, unless it is at the destination or lost
static void set_next_route_tile_direction(figure *f)
{
    // The figure currently has a route established (Path ID > 0)
    if (f->routing_path_id > 0) {
        // If the figure still has tiles left in the route
        if (f->routing_path_current_tile < f->routing_path_length) {
            // Get the next direction from the route data
            return = figure_route_get_direction(f->routing_path_id, f->routing_path_current_tile);
        } else {
            // Figure has reached the end of the route
            figure_route_remove(f);
            return = DIR_FIGURE_AT_DESTINATION;
        }

    } else {
        // The figure is not currently following a route (Path ID <= 0)
        if (calc_general_direction(f->x, f->y, f->destination_x, f->destination_y) == DIR_FIGURE_AT_DESTINATION) {
            // The figure is currently on the destination tile
            return = DIR_FIGURE_AT_DESTINATION;
        } else {
            // Route is finished/missing, and figure is NOT at destination -> consider it lost
            return = DIR_FIGURE_LOST;
        }
    }
}
// Handles all the complex logic when a figure reaches a tile boundary
static void handle_tile_boundary_logic(figure *f, int roaming_enabled)
{
    // Provide coverage (e.g prefects)
    if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
        figure_service_provide_coverage(f);
    }

    // Pathfinding Check: If figure lost its route, re-add it
    if (f->routing_path_id <= 0) {
        figure_route_add(f);
    }

    // Determine the next step's direction
    f->direction = set_next_route_tile_direction(f);
    // Check for Collision in the chosen direction
    target_position = f->grid_offset + map_grid_direction_delta(f->direction);
    after_target_position = target_position + map_grid_direction_delta(f->direction);
    check_collision(f, roaming_enabled, target_position, after_target_position);

    // Halt Movement Check: Stop if the status is not a valid movement direction
    if (f->direction > DIR_MAX_MOVEMENT) {
        // This breaks the while loop if the figure is AT_DESTINATION, REROUTE, LOST, or ATTACKING
        break;
    }

    // Commit to the new tile
    // Update route progress
    f->routing_path_current_tile++;
    // Store direction for snake followers or next tile decision
    f->previous_tile_direction = f->direction;
    // Reset progress for the new tile
    f->progress_on_tile = 0;
    // Change the figure's tile position
    move_to_next_tile(f);
    // Take the first sub-coordinate step on the new tile
    if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
        advance_mov(f);
    }
}

/**
 * @brief Moves a path-bound figure by a number of movements
 *
 *  Does not apply to follower figures
 *
 * @param f The figure that will move.
 * @param num_mov Number of movements this tick.
 * @param roaming_enabled Allows the figure to roam, which is the case for all non-followers
 */
static void move_figure_path(figure *f, int num_mov, int roaming_enabled)
{
    // Speed Modifiers (just highways for now)
    int terrain = map_terrain_get(map_grid_offset(f->x, f->y));
    if (terrain & TERRAIN_HIGHWAY) {
        num_mov *= 2;
    }

    // Main Movement Loop: Process all available movements
    while (num_mov-- > 0) {
        f->progress_on_tile++;
        // A. Mid-Tile Movement (Sub-coordinate step)
        if (f->progress_on_tile < MOV_PER_TILE) {
            advance_mov(f);

        } else {
            // B. End-of-Tile Logic (Tile Boundary Reached)
            handle_tile_boundary_logic(f, roaming_enabled);
            if (f->direction > DIR_MAX_MOVEMENT) {
                break;
            }
        }
    }
}

// --- CORE FUNCTION ---
/**
 * @brief Handles figure movement on a path.
 *
 * This function accumulates percent ticks based on speed.
 *
 * When it reaches 100%, the figure moves 1 tick.
 *
 * @param f The figure that will move.
 * @param speed The speed of the figure in movements per tick.
 */
void figure_movement_path(figure *f, int speed)
{
    // Defines how many times will the figure move this tick
    int num_mov = 0;

    // Adds progress based on speed
    int progress = f->progress_to_next_movement + speed;

    // Converts accumulated progress into movements (actual movement happens later)
    while (progress >= MOVEMENT_THRESHOLD) {
        progress -= MOVEMENT_THRESHOLD;
        num_mov++;
    }

    // Stores the updated movements (must be a signed char if it can be negative)
    f->progress_to_next_movement = (char) progress;

    // Moves the figure the total number of ticks
    move_figure_path(f, num_mov, 0);
}
