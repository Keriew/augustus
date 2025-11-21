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
}
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
// Defines how a figure will move on a given tick (tiles take 15 ticks to move through)
static void advance_tick(figure *f)
{
    // 1. Apply the core physics movement for this tick
    apply_sub_tile_movement(f);

    // 2. Handle height adjustments and ghost state
    if (f->height_adjusted_ticks) {
        f->height_adjusted_ticks--;
        if (f->height_adjusted_ticks > 0) {
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
// Checks if a roaming figure is blocked by a building (Roadblock/Granary)
static int is_roaming_blocked_by_building(figure *f, int grid_offset)
{
    if (!map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        return 0; // Not blocked by a building if there isn't one
    }

    building *b = building_get(map_building_at(grid_offset));

    // 1. Granary Dead-End Check
    if (b->type == BUILDING_GRANARY) {
        if (map_road_get_granary_inner_road_tiles_count(b) < 3) {
            return 1; // Blocked: Don't roam into dead-end granaries
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
// Determines the HP of a destroyable building
static int get_obstacle_hp(building_type type)
{
    switch (type) {
        case BUILDING_PALISADE:
        case BUILDING_PALISADE_GATE:
            return PALISADE_HP;
        case BUILDING_WALL:
            return WALL_HP;
        case BUILDING_GATEHOUSE:
            return GATEHOUSE_HP;
        default:
            return BUILDING_HP;
    }
}
// Handles enemy interaction with obstacles
static void attempt_obstacle_destruction(figure *f, int target_grid_offset)
{
    if (!map_routing_is_destroyable(target_grid_offset)) {
        return;
    }

    int max_damage = 0;
    int cause_damage = 1;
    int destroyable_type = map_routing_get_destroyable(target_grid_offset);

    // 1. Calculate Max Damage based on type
    if (destroyable_type == DESTROYABLE_BUILDING) {
        building *b = building_get(map_building_at(target_grid_offset));
        max_damage = get_obstacle_hp(b->type);
    } else if (destroyable_type == DESTROYABLE_AQUEDUCT_GARDEN) {
        if (map_terrain_is(target_grid_offset, TERRAIN_ACCESS_RAMP | TERRAIN_RUBBLE)) {
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
            building_destroy_increase_enemy_damage(target_grid_offset, max_damage);
        }
    }
}

// --- CORE MOVEMENT & STATE FUNCTIONS ---
/* Gets roadblock permissions for the figure */
roadblock_permission get_permission_for_figure_type(figure *f)
{
    // Safety check: Ensures the figure type is a valid index for the array.
    if (f->type >= 0 && f->type < FIGURE_TYPE_COUNT) {
        return FIGURE_PERMISSIONS[f->type];
    }
    // Default case
    return PERMISSION_NONE;
}
// Moves the figure to the next tile
void move_to_next_tile(figure *f)
{
    int old_x = f->x;
    int old_y = f->y;
    // Removes the figure
    if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
        map_figure_delete(f);
    }
    // Changes direction based on where it is moving to
    f->x += DIR_DELTAS[f->direction].x;
    f->y += DIR_DELTAS[f->direction].y;

    // Adds the figure to its new position
    f->grid_offset += map_grid_direction_delta(f->direction);
    if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
        map_figure_add(f);
    }
    // Adds to the figure if it is on a road, and if it is on a bridge it adjusts its height to match
    if (map_terrain_is(f->grid_offset, TERRAIN_ROAD)) {
        f->is_on_road = 1;
        if (map_terrain_is(f->grid_offset, TERRAIN_WATER)) { // bridge
            set_target_height_bridge(f);
        }
        // Makes it clear the figure is NOT on a road
    } else {
        f->is_on_road = 0;
    }
    // Attacks a figure if it detects (?) it
    if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
        figure_combat_attack_figure_at(f, f->grid_offset);
    }
    // Updates the previous tile (variable? or what?)
    f->previous_tile_x = old_x;
    f->previous_tile_y = old_y;
}
// Calculates the next movement direction, unless it is at the destination or lost
static void set_next_route_tile_direction(figure *f)
{
    // The figure currently has a route established (Path ID > 0)
    if (f->routing_path_id > 0) {
        // If the figure still has tiles left in the route
        if (f->routing_path_current_tile < f->routing_path_length) {
            // Get the next direction from the route data
            f->direction = figure_route_get_direction(f->routing_path_id, f->routing_path_current_tile);
        } else {
            // Figure has reached the end of the route
            figure_route_remove(f);
            f->direction = DIR_FIGURE_AT_DESTINATION;
        }

        // The figure is not currently following a route (Path ID <= 0)
    } else {
        // Check if the figure is currently on the destination tile
        if (calc_general_direction(f->x, f->y, f->destination_x, f->destination_y) == DIR_FIGURE_AT_DESTINATION) {
            f->direction = DIR_FIGURE_AT_DESTINATION;
        } else {
            // Route is finished/missing, and figure is NOT at destination -> consider it lost
            f->direction = DIR_FIGURE_LOST;
        }
    }
}
// Checks for collisions (also handles rerouting and destruction)
static void check_collision(figure *f, int roaming_enabled)
{
    // 1. Sanity Check (Use your new constant)
    if (f->direction > DIR_MAX_MOVEMENT) {
        return;
    }

    int target_offset = f->grid_offset + map_grid_direction_delta(f->direction);

    // --- CASE A: BOAT ---
    if (f->is_boat) {
        if (!map_terrain_is(target_offset, TERRAIN_WATER)) {
            f->direction = DIR_FIGURE_REROUTE;
        }
        return;
    }

    // --- CASE B: ENEMY ---
    if (f->terrain_usage == TERRAIN_USAGE_ENEMY) {
        if (!map_routing_noncitizen_is_passable(target_offset)) {
            f->direction = DIR_FIGURE_REROUTE;
        } else {
            // Extracted combat logic
            attempt_obstacle_destruction(f, target_offset);
        }
        return;
    }

    // --- CASE C: WALL WALKER ---
    if (f->terrain_usage == TERRAIN_USAGE_WALLS) {
        if (!map_routing_is_wall_passable(target_offset)) {
            f->direction = DIR_FIGURE_REROUTE;
        }
        return;
    }

    // --- CASE D: STANDARD WALKER (Roads & Terrain) ---

    // 1. Check Impassable Terrain
    if (map_terrain_is(target_offset, TERRAIN_IMPASSABLE)) {
        f->direction = DIR_FIGURE_REROUTE;
        return;
    }

    // 2. Check Roaming Restrictions (Roadblocks & Granaries)
    // This handles both buildings-on-roads and buildings-on-terrain
    if (roaming_enabled && is_roaming_blocked_by_building(f, target_offset)) {
        f->direction = DIR_FIGURE_REROUTE;
        return;
    }

    // 3. Check Generic Building Blocking
    // If it's a building, and it's NOT passable for citizens, block it.
    if (map_terrain_is(target_offset, TERRAIN_BUILDING)) {
        int is_passable = map_routing_citizen_is_passable_terrain(target_offset);
        int is_road = map_routing_citizen_is_road(target_offset);

        // Allow passage if it's defined as passable or if it's a road (unless roaming was disabled)
        if (is_passable || (is_road && !roaming_enabled)) {
            return;
        }

        // Special exception for Fort Grounds (troops can walk there)
        building *b = building_get(map_building_at(target_offset));
        if (b->type != BUILDING_FORT_GROUND) {
            f->direction = DIR_FIGURE_REROUTE;
        }
    }
}
// Handles all the complex logic when a figure reaches a tile boundary
static void handle_tile_boundary_logic(figure *f, int roaming_enabled)
{
    // Service Logic: Provide coverage (e.g., Prefect checks houses)
    // This is skipped for "preview" figures (Ghosting)
    if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
        figure_service_provide_coverage(f);
    }

    // Pathfinding Check: If figure lost its route, re-add it
    if (f->routing_path_id <= 0) {
        figure_route_add(f);
    }

    // State Transition 1: Determine the next step's direction
    set_next_route_tile_direction(f);

    // State Transition 2: Check collision for the chosen direction
    // This might change f->direction to DIR_FIGURE_REROUTE or DIR_FIGURE_ATTACK
    check_collision(f, roaming_enabled);

    // Halt Movement Check: Stop if the status is not a valid movement direction
    if (f->direction > DIR_MAX_MOVEMENT) {
        // This breaks the while loop if the figure is AT_DESTINATION, REROUTE, LOST, or ATTACKING
        break;
    }

    // 3. Commit to the new tile

    // Update route progress
    f->routing_path_current_tile++;

    // Store direction for snake followers or next tile decision
    f->previous_tile_direction = f->direction;

    // Reset progress for the new tile
    f->progress_on_tile = 0;

    // Change the figure's main grid coordinate (x, y, grid_offset)
    move_to_next_tile(f);

    // Take the first sub-coordinate step on the new tile
    if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
        advance_tick(f);
    }
}
// Defines the standard, path-following movement for non-roaming non-following figures
static void walk_ticks(figure *f, int num_ticks, int roaming_enabled)
{
    // 1. Terrain Speed Check (Highway doubles tick speed)
    int terrain = map_terrain_get(map_grid_offset(f->x, f->y));
    if (terrain & TERRAIN_HIGHWAY) {
        num_ticks *= 2;
    }

    // 2. Main Movement Loop: Process all available ticks
    while (num_ticks-- > 0) {
        f->progress_on_tile++;
        // A. Mid-Tile Movement (Sub-coordinate step)
        if (f->progress_on_tile < TICKS_PER_TILE) {
            advance_tick(f);
            // B. End-of-Tile Logic (Tile Boundary Reached)
        } else {
            handle_tile_boundary_logic(f, roaming_enabled);
            if (f->direction > DIR_MAX_MOVEMENT) {
                break;
            }
        }
    }
}

// --- PUBLIC FUNCTIONS ---
/* Handles variable speed accumulator, then calls walk_ticks. */
void figure_movement_move_ticks_with_percentage(figure *f, int num_ticks, int tick_percentage)
{
    // Sub-Tick Accumulator Logic:
    // Handles speed variation by accumulating leftover percentage.
    // If the accumulation hits 100%, we gain 1 full movement tick.
    int progress = f->progress_to_next_tick + tick_percentage;

    if (progress >= TICK_PERCENTAGE_BASE) {
        progress -= TICK_PERCENTAGE_BASE;
        num_ticks++;
    } else if (progress <= -TICK_PERCENTAGE_BASE) {
        progress += TICK_PERCENTAGE_BASE;
        num_ticks--;
    }
    f->progress_to_next_tick = (char) progress;

    // Delegate the actual movement to the main tick function.
    // The '0' indicates the figure is NOT following a leader.
    walk_ticks(f, num_ticks, 0);
}
// Animates Tower Sentry movement (sub-coordinates within a tile boundary only)
void figure_movement_move_ticks_tower_sentry(figure *f, int num_ticks)
{
    while (num_ticks-- > 0) {
        f->progress_on_tile++;
        // If we haven't reached the tile edge, continue animating sub-tile movement.
        if (f->progress_on_tile < TICKS_PER_TILE) {
            advance_tick(f);
        } else {
            // Reached the end of the tile walk (e.g., end of the tower wall).
            // Clamp the progress to prevent overflow and stop movement.
            f->progress_on_tile = TICKS_PER_TILE;
            // Note: No call to move_to_next_tile(f); the figure remains on its grid tile.
        }
    }
}
// Handles variable speed accumulator for snake followers
void figure_movement_follow_ticks_with_percentage(figure *f, int num_ticks, int tick_percentage)
{
    // 1. Handle Sub-Tick Accumulator (Variable Speed)
    // Adds the percentage variance. If it crosses 100, we gain/lose a whole tick.
    int progress = f->progress_to_next_tick + tick_percentage;

    if (progress >= 100) {
        progress -= 100;
        num_ticks++;
    } else if (progress <= -100) {
        progress += 100;
        num_ticks--;
    }
    // Cast back to the storage type (likely char or short)
    f->progress_to_next_tick = (char) progress;

    // 2. Get Leader with Safety Check
    const figure *leader = figure_get(f->leading_figure_id);
    if (!leader) {
        // If the leader is gone, the follower should probably stop or vanish.
        // For now, we just return to prevent a crash.
        return;
    }

    // 3. Handle "Spawn Ghosting"
    // If at the source coordinate, remain a ghost (prevents clipping at spawn structures)
    if (f->x == f->source_x && f->y == f->source_y) {
        f->is_ghost = 1;
    }

    // 4. Highway Rubber-Banding
    // If the leader is speeding on a highway, the follower matches speed to stay connected.
    if (map_terrain_is(map_grid_offset(leader->x, leader->y), TERRAIN_HIGHWAY)) {
        num_ticks *= 2;
    }

    // 5. Movement Loop
    while (num_ticks-- > 0) {
        f->progress_on_tile++;

        // Mid-tile movement
        if (f->progress_on_tile < TICKS_PER_TILE) {
            advance_tick(f);
        }
        // End of tile reached
        else {
            // SNAKE LOGIC: Target the tile the leader JUST left
            direction_type next_dir = calc_general_direction(f->x, f->y,
                leader->previous_tile_x, leader->previous_tile_y);

            // Stop if the direction is invalid (e.g., Status Flag or No Move)
            if (next_dir > DIR_MAX_MOVEMENT) {
                f->progress_on_tile = TICKS_PER_TILE; // Clamp progress
                break;
            }

            // Apply Movement
            f->direction = next_dir;
            f->previous_tile_direction = f->direction;
            f->progress_on_tile = 0;

            move_to_next_tile(f);

            // Advance the sub-coordinate tick immediately after entering the new tile
            advance_tick(f);
        }
    }
}
// Wrapper for followers without speed variation (which?)
void figure_movement_follow_ticks(figure *f, int num_ticks)
{
    figure_movement_follow_ticks_with_percentage(f, num_ticks, 0); // 0 represents 0% speed variation.
}
// Advances the figure during an attack animation
void figure_movement_advance_attack(figure *f)
{
    if (f->progress_on_tile <= 5) {
        f->progress_on_tile++;
        advance_tick(f);
    }
}