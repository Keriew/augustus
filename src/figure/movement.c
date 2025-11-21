#include "movement.h"
#include "direction.h"
#include "building/properties.h"

#include "core/log.h"
#include "SDL.h"

#include "building/building.h"
#include "building/destruction.h"
#include "building/roadblock.h"
#include "core/calc.h"
#include "core/config.h"
#include "figure/combat.h"
#include "figure/route.h"
#include "figure/service.h"
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

// ---------- MAPS ----------
// Defines the 4 Cardinal Directions
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

// Direction Deltas
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
    if (f->direction <= DIR_MAX_MOVEMENT) {
        f->cross_country_x += DIR_DELTAS[f->direction].x;
        f->cross_country_y += DIR_DELTAS[f->direction].y;
    }


// Roadblock Permissions
static const roadblock_permission FIGURE_PERMISSIONS[FIGURE_TYPE_COUNT] = {
    
    // Maintenance
    [FIGURE_ENGINEER]= PERMISSION_MAINTENANCE,
    [FIGURE_PREFECT]= PERMISSION_MAINTENANCE,

    // Entertainers
    [FIGURE_GLADIATOR] = PERMISSION_ENTERTAINER,
    [FIGURE_CHARIOTEER]= PERMISSION_ENTERTAINER,
    [FIGURE_ACTOR] = PERMISSION_ENTERTAINER,
    [FIGURE_LION_TAMER]= PERMISSION_ENTERTAINER,
    [FIGURE_BARKEEP] = PERMISSION_ENTERTAINER,

    // Medicine
    [FIGURE_SURGEON] = PERMISSION_MEDICINE,
    [FIGURE_DOCTOR]= PERMISSION_MEDICINE,
    [FIGURE_BARBER]= PERMISSION_MEDICINE,
    [FIGURE_BATHHOUSE_WORKER]= PERMISSION_MEDICINE,

    // Education
    [FIGURE_SCHOOL_CHILD]= PERMISSION_EDUCATION,
    [FIGURE_TEACHER] = PERMISSION_EDUCATION,
    [FIGURE_LIBRARIAN] = PERMISSION_EDUCATION,

    // Unique Walkers
    [FIGURE_PRIEST]= PERMISSION_PRIEST,
    [FIGURE_MARKET_TRADER] = PERMISSION_MARKET,
    [FIGURE_TAX_COLLECTOR] = PERMISSION_TAX_COLLECTOR,
    [FIGURE_LABOR_SEEKER]= PERMISSION_LABOR_SEEKER,
    [FIGURE_MISSIONARY]= PERMISSION_MISSIONARY,
    [FIGURE_WATCHMAN]= PERMISSION_WATCHMAN
};

// --- HELPER FUNCTIONS ---
// Determines the HP of a destroyable building
static int get_obstacle_hp(building_type type) {
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
static void attempt_obstacle_destruction(figure *f, int target_grid_offset) {
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

// Checks if a roaming figure is blocked by a building (Roadblock/Granary)
static int is_roaming_blocked_by_building(figure *f, int grid_offset) {
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

// Defines how a figure will move on a given tick (tiles take 15 ticks to move through)
static void advance_tick(figure *f)
{
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

// Assigns roadblock permissions for each figure type
roadblock_permission get_permission_for_figure_type(figure *f) {
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

// Calculates the next movement direction
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

static void advance_route_tile(figure *f, int roaming_enabled)
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

// Unclear?
static void walk_ticks(figure *f, int num_ticks, int roaming_enabled)
{
    // Terrain speed check
    int terrain = map_terrain_get(map_grid_offset(f->x, f->y));
    if (terrain & TERRAIN_HIGHWAY) {
        num_ticks *= 2;
    }

    // Figure moves until it runs out of ticks
    while (num_ticks-- > 0) {
        f->progress_on_tile++;
        // Figure hasn't fully moved onto the next tile
        if (f->progress_on_tile < TICKS_PER_TILE) {
            advance_tick(f);
        // Figure has reached the next tile
        } else {
            // Figure provides coverage to its surroundings
            if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
                figure_service_provide_coverage(f);
            }
            // Assigns a route to the figure
            if (f->routing_path_id <= 0) {
                figure_route_add(f);
            }  
            // Replaces the current direction by a new one
            set_next_route_tile_direction(f);
            // Checks if the figure can move into the next tile in the route
            advance_route_tile(f, roaming_enabled);
            // Stop moving at all (???) if they try to move to a non-adjacent tile somehow
            if (f->direction > DIR_MAX_MOVEMENT) { 
                break;
            }
            f->routing_path_current_tile++;
            f->previous_tile_direction = f->direction;
            f->progress_on_tile = 0; // Reset progress on tile as it is moving onto a next one
            // Finally moves into the next tile
            move_to_next_tile(f);
            if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
                advance_tick(f);
            }
        }
    }
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
        if (CARDINAL_CHECK(current_dir) && is_valid_road_for_roaming(grid_offset + map_grid_direction_delta((direction_type)current_dir), permission)) {
            cw_direction = (direction_type)current_dir;
            break;
        }
        // Correct Rotation: Use the lookup table for the next direction
        current_dir = DIR_CW_ROTOR[current_dir]; 
        cw_angular_offset++;
    }

    // Loop 2: Search counter-clockwise (ccw)
    for (int i = 0, current_dir = target_dir; i < DIR_INDEX_WRAP; i++) {
        // Check only cardinal directions (2, 4, 6, 8)
        if (CARDINAL_CHECK(current_dir) && is_valid_road_for_roaming(grid_offset + map_grid_direction_delta((direction_type)current_dir), permission)) {
            ccw_direction = (direction_type)current_dir;
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

void figure_movement_move_ticks(figure *f, int num_ticks)
{
    walk_ticks(f, num_ticks, 0);
}

void figure_movement_move_ticks_with_percentage(figure *f, int num_ticks, int tick_percentage)
{
    int progress = f->progress_to_next_tick + tick_percentage;

    if (progress >= 100) {
        progress -= 100;
        num_ticks++;
    } else if (progress <= -100) {
        progress += 100;
        num_ticks--;
    }
    f->progress_to_next_tick = (char) progress;

    walk_ticks(f, num_ticks, 0);
}

void figure_movement_move_ticks_tower_sentry(figure *f, int num_ticks)
{
    while (num_ticks > 0) {
        num_ticks--;
        f->progress_on_tile++;
        if (f->progress_on_tile < TICKS_PER_TILE) {
            advance_tick(f);
        } else {
            f->progress_on_tile = TICKS_PER_TILE;
        }
    }
}

void figure_movement_follow_ticks(figure *f, int num_ticks)
{
    const figure *leader = figure_get(f->leading_figure_id);
    if (f->x == f->source_x && f->y == f->source_y) {
        f->is_ghost = 1;
    }
    if (map_terrain_is(map_grid_offset(leader->x, leader->y), TERRAIN_HIGHWAY)) {
        num_ticks *= 2;
    }
    while (num_ticks > 0) {
        num_ticks--;
        f->progress_on_tile++;
        if (f->progress_on_tile < TICKS_PER_TILE) {
            advance_tick(f);
        } else {
            f->progress_on_tile = TICKS_PER_TILE;
            f->direction = calc_general_direction(f->x, f->y,
                leader->previous_tile_x, leader->previous_tile_y);
            if (f->direction >= 8) {
                break;
            }
            f->previous_tile_direction = f->direction;
            f->progress_on_tile = 0;
            move_to_next_tile(f);
            advance_tick(f);
        }
    }
}

void figure_movement_follow_ticks_with_percentage(figure *f, int num_ticks, int tick_percentage)
{
    int progress = f->progress_to_next_tick + tick_percentage;

    if (progress >= 100) {
        progress -= 100;
        num_ticks++;
    } else if (progress <= -100) {
        progress += 100;
        num_ticks--;
    }
    f->progress_to_next_tick = (char) progress;

    const figure *leader = figure_get(f->leading_figure_id);
    if (f->x == f->source_x && f->y == f->source_y) {
        f->is_ghost = 1;
    }
    if (map_terrain_is(map_grid_offset(leader->x, leader->y), TERRAIN_HIGHWAY)) {
        num_ticks *= 2;
    }

    while (num_ticks > 0) {
        num_ticks--;
        f->progress_on_tile++;
        if (f->progress_on_tile < TICKS_PER_TILE) {
            advance_tick(f);
        } else {
            f->progress_on_tile = TICKS_PER_TILE;
            f->direction = calc_general_direction(f->x, f->y,
                leader->previous_tile_x, leader->previous_tile_y);
            if (f->direction >= 8) {
                break;
            }
            f->previous_tile_direction = f->direction;
            f->progress_on_tile = 0;
            move_to_next_tile(f);
            advance_tick(f);
        }
    }
}

// Helper: Filters road options for complex shapes (Double-wide roads, Plazas)
// Returns the new count of valid roads.
static int filter_complex_road_topology(int grid_offset, int adjacent_count, int *road_tiles, direction_type came_from) {
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

// Helper: Decides the next direction based on available roads
static direction_type pick_next_roam_direction(figure *f, int *road_tiles, int adjacent_count, direction_type came_from, int permission) {
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

// Handles roaming movement
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
        int road_tiles[10] = {0}; 
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

void figure_movement_advance_attack(figure *f)
{
    if (f->progress_on_tile <= 5) {
        f->progress_on_tile++;
        advance_tick(f);
    }
}

// --- CROSS COUNTRY DIRECTION --- //
// Not sure what this does?
void figure_movement_set_cross_country_direction(figure *f, int x_src, int y_src, int x_dst, int y_dst, int is_missile) {
    // All x/y are in 1/15th of a tile
    f->cc_destination_x = x_dst;
    f->cc_destination_y = y_dst;

    // Calculate absolute deltas
    f->cc_delta_x = (x_src > x_dst) ? (x_src - x_dst) : (x_dst - x_src);
    f->cc_delta_y = (y_src > y_dst) ? (y_src - y_dst) : (y_dst - y_src);

    // Calculate initial Bresenham error term (2 * minor_delta - major_delta)
    if (f->cc_delta_x < f->cc_delta_y) {
        // Y is major axis, X is minor
        f->cc_delta_xy = 2 * f->cc_delta_x - f->cc_delta_y;
    } else if (f->cc_delta_y < f->cc_delta_x) {
        // X is major axis, Y is minor
        f->cc_delta_xy = 2 * f->cc_delta_y - f->cc_delta_x;
    } else { // Deltas are equal (perfect diagonal)
        f->cc_delta_xy = 0;
    }

    if (is_missile) {
        // Missiles use a continuous direction calculation
        f->direction = calc_missile_direction(x_src, y_src, x_dst, y_dst);
    } else {
        // Figures use snapped 9-way direction
        f->direction = calc_general_direction(x_src, y_src, x_dst, y_dst);
        
        // Direction Snapping Logic: If the path is highly vertical (Y-dominant)
        if (f->cc_delta_y > 2 * f->cc_delta_x) {
            switch (f->direction) {
                // Diagonal directions snap to vertical direction (Top or Bottom)
                case DIR_TOP_RIGHT: case DIR_TOP_LEFT: 
                    f->direction = DIR_TOP; 
                    break;
                case DIR_BOTTOM_RIGHT: case DIR_BOTTOM_LEFT: 
                    f->direction = DIR_BOTTOM; 
                    break;
            }
        }

        // Direction Snapping Logic: If the path is highly horizontal (X-dominant)
        if (f->cc_delta_x > 2 * f->cc_delta_y) {
            switch (f->direction) {
                // Diagonal directions snap to horizontal direction (Right or Left)
                case DIR_TOP_RIGHT: case DIR_BOTTOM_RIGHT: 
                    f->direction = DIR_RIGHT; 
                    break;
                case DIR_BOTTOM_LEFT: case DIR_TOP_LEFT: 
                    f->direction = DIR_LEFT; 
                    break;
            }
        }

    // Set the major axis for Bresenham's algorithm
    if (f->cc_delta_x >= f->cc_delta_y) {
        f->cc_direction = CC_MAJOR_X;
    } else {
        f->cc_direction = CC_MAJOR_Y;
    }
}
// ???
void figure_movement_set_cross_country_destination(figure *f, int x_dst, int y_dst)
{
    f->destination_x = x_dst;
    f->destination_y = y_dst;
    figure_movement_set_cross_country_direction(
        f, f->cross_country_x, f->cross_country_y,
        TICKS_PER_TILE * x_dst, TICKS_PER_TILE * y_dst, 0);
}
// Calculates the direction when travelling cross country (no roads)
static void cross_country_update_delta(figure *f)
{
    // The X-axis is the major axis
    if (f->cc_direction == CC_MAJOR_X) { 
        // Case 1: Error term is positive (or zero, meaning step diagonally)
        if (f->cc_delta_xy >= 0) {
            // Update error term: E_new = E_old + 2 * (dy - dx)
            f->cc_delta_xy += 2 * (f->cc_delta_y - f->cc_delta_x);
        } else {
            // Case 2: Error term is negative (meaning step only on major axis)
            // Update error term: E_new = E_old + 2 * dy
            f->cc_delta_xy += 2 * f->cc_delta_y;
        }
        // Decrement the total distance remaining on the major axis (X)
        f->cc_delta_x--;
        
    // The Y-axis is the major axis
    } else { // Implicitly f->cc_direction == CC_MAJOR_Y
        // Case 1: Error term is positive (or zero, meaning step diagonally)
        if (f->cc_delta_xy >= 0) {
            // Update error term: E_new = E_old + 2 * (dx - dy)
            f->cc_delta_xy += 2 * (f->cc_delta_x - f->cc_delta_y);
        } else {
            // Case 2: Error term is negative (meaning step only on major axis)
            // Update error term: E_new = E_old + 2 * dx
            f->cc_delta_xy += 2 * f->cc_delta_x;
        }
        // Decrement the total distance remaining on the major axis (Y)
        f->cc_delta_y--;
    }
}
// Moves the figure one sub-tile step in the x direction
static void cross_country_advance_x(figure *f)
{
    if (f->cross_country_x < f->cc_destination_x) {
        f->cross_country_x++;
    } else if (f->cross_country_x > f->cc_destination_x) {
        f->cross_country_x--;
    }
}
// Moves the figure one sub-tile step in the y direction
static void cross_country_advance_y(figure *f)
{
    if (f->cross_country_y < f->cc_destination_y) {
        f->cross_country_y++;
    } else if (f->cross_country_y > f->cc_destination_y) {
        f->cross_country_y--;
    }
}
// Calculates in which direction should the figure move towards
static void cross_country_advance(figure *f)
{
    cross_country_update_delta(f);
    
    // Check if the Y-axis is the major axis
    if (f->cc_direction == CC_MAJOR_Y) { 
        // Always advance the major axis (Y)
        cross_country_advance_y(f);
        
        // If the error term >= 0, also advance the minor axis (X)
        if (f->cc_delta_xy >= 0) {
            // Decrement the remaining distance on the minor axis
            f->cc_delta_x--; 
            cross_country_advance_x(f);
        }
    } else { // The X-axis is the major axis (f->cc_direction == CC_MAJOR_X)
        // Always advance the major axis (X)
        cross_country_advance_x(f);
        
        // If the error term >= 0, also advance the minor axis (Y)
        if (f->cc_delta_xy >= 0) {
            // Decrement the remaining distance on the minor axis
            f->cc_delta_y--; 
            cross_country_advance_y(f);
        }
    }
}
// What does this do?
int figure_movement_move_ticks_cross_country(figure *f, int num_ticks)
{
    map_figure_delete(f);
    int is_at_destination = 0;
    while (num_ticks > 0) {
        num_ticks--;
        if (f->missile_height > 0) {
            f->missile_height--;
        } else {
            f->missile_height = 0;
        }
        if (f->cc_delta_x + f->cc_delta_y <= 0) {
            is_at_destination = 1;
            break;
        }
        cross_country_advance(f);
    }
    f->x = f->cross_country_x / TICKS_PER_TILE;
    f->y = f->cross_country_y / TICKS_PER_TILE;
    f->grid_offset = map_grid_offset(f->x, f->y);
    if (map_terrain_is(f->grid_offset, TERRAIN_BUILDING)) {
        f->in_building_wait_ticks = 8;
    } else if (f->in_building_wait_ticks) {
        f->in_building_wait_ticks--;
    }
    map_figure_add(f);
    return is_at_destination;
}

// Checks if the figure can launch a missile (cross country)
int figure_movement_can_launch_cross_country_missile(int x_src, int y_src, int x_dst, int y_dst)
{
    int height = 0;
    figure *f = figure_get(0); // abuse unused figure 0 as scratch
    f->cross_country_x = TICKS_PER_TILE * x_src;
    f->cross_country_y = TICKS_PER_TILE * y_src;
    if (map_terrain_is(map_grid_offset(x_src, y_src), TERRAIN_WALL_OR_GATEHOUSE) ||
        building_get(map_building_at(map_grid_offset(x_src, y_src)))->type == BUILDING_WATCHTOWER) {
        height = 6;
    }
    figure_movement_set_cross_country_direction(f, TICKS_PER_TILE * x_src, TICKS_PER_TILE * y_src, TICKS_PER_TILE * x_dst, TICKS_PER_TILE * y_dst, 0);

    for (int guard = 0; guard < 1000; guard++) {
        for (int i = 0; i < 8; i++) {
            if (f->cc_delta_x + f->cc_delta_y <= 0) {
                return 1;
            }
            cross_country_advance(f);
        }
        f->x = f->cross_country_x / TICKS_PER_TILE;
        f->y = f->cross_country_y / TICKS_PER_TILE;
        if (height) {
            height--;
        } else {
            int grid_offset = map_grid_offset(f->x, f->y);
            if (map_terrain_is(grid_offset, TERRAIN_WALL | TERRAIN_GATEHOUSE | TERRAIN_TREE)) {
                break;
            }
            if (map_terrain_is(grid_offset, TERRAIN_BUILDING) && map_property_multi_tile_size(grid_offset) > 1) {
                building_type type = building_get(map_building_at(grid_offset))->type;
                if (type != BUILDING_FORT_GROUND) {
                    break;
                }
            }
        }
    }
    return 0;
}
