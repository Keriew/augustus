#include "movement.h"
#include "direction.h"

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
    if (f->direction < 8) {
        f->cross_country_x += DIR_DELTAS[f->direction].x;
        f->cross_country_y += DIR_DELTAS[f->direction].y;
    }

// Defines the sequence: Top (2), Left (4), Right (6), Bottom (8)
static const int ROAM_CARDINAL_DIRECTIONS[4] = {
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

static void set_next_route_tile_direction(figure *f)
{
    // Figure hasn't reached destination yet
    if (f->routing_path_id > 0) {
        if (f->routing_path_current_tile < f->routing_path_length) {
            f->direction = figure_route_get_direction(f->routing_path_id, f->routing_path_current_tile);
        } else {
            figure_route_remove(f);
            f->direction = DIR_FIGURE_AT_DESTINATION;
        }
    // Figure has reached its destination
    } else {
        f->direction = calc_general_direction(f->x, f->y, f->destination_x, f->destination_y);
        if (f->direction != DIR_FIGURE_AT_DESTINATION) {
            f->direction = DIR_FIGURE_LOST;
        }
    }
}

static void advance_route_tile(figure *f, int roaming_enabled)
{
    // The figure is trying to go through non-adjacent terrain
    if (f->direction > 8) { // >=8 was a bug right?
        return;
    }
    // Defines the target tile they're trying to move into
    int target_grid_offset = f->grid_offset + map_grid_direction_delta(f->direction);
    // Defines the tile immediately after the target tile they're trying to move into
    int next_target_grid_offset = target_grid_offset + map_grid_direction_delta(f->direction);
    // The figure is a boat
    if (f->is_boat) {
        if (!map_terrain_is(target_grid_offset, TERRAIN_WATER)) {
            f->direction = DIR_FIGURE_REROUTE;
        }
    // The figure is trying to go through enemy terrain (to them)
    } else if (f->terrain_usage == TERRAIN_USAGE_ENEMY) {
        if (!map_routing_noncitizen_is_passable(target_grid_offset)) {
            f->direction = DIR_FIGURE_REROUTE;
        } else if (map_routing_is_destroyable(target_grid_offset)) {
            int cause_damage = 1;
            int max_damage = 0;
            switch (map_routing_get_destroyable(target_grid_offset)) {
                case DESTROYABLE_BUILDING:
                {
                    building *b = building_get(map_building_at(target_grid_offset));
                    switch (b->type) {
                        case BUILDING_PALISADE:
                        case BUILDING_PALISADE_GATE:
                            max_damage = PALISADE_HP;
                            break;
                        default:
                            max_damage = BUILDING_HP;
                            break;
                    }
                    break;
                }
                case DESTROYABLE_AQUEDUCT_GARDEN:
                    if (map_terrain_is(target_grid_offset, TERRAIN_ACCESS_RAMP | TERRAIN_RUBBLE)) {
                        cause_damage = 0;
                    } else {
                        max_damage = BUILDING_HP;
                    }
                    break;
                case DESTROYABLE_WALL:
                    max_damage = WALL_HP;
                    break;
                case DESTROYABLE_GATEHOUSE:
                    max_damage = GATEHOUSE_HP;
                    break;
            }
            if (cause_damage) {
                f->attack_direction = f->direction;
                f->direction = DIR_FIGURE_ATTACK;
                if (!(game_time_tick() & 3)) {
                    building_destroy_increase_enemy_damage(target_grid_offset, max_damage);
                }
            }
        }
    // The figure is trying to go into a wall
    } else if (f->terrain_usage == TERRAIN_USAGE_WALLS) {
        if (!map_routing_is_wall_passable(target_grid_offset)) {
            f->direction = DIR_FIGURE_REROUTE;
        }
    // The figure is trying to go into a road, highway or access ramp e.g stairs
    } else if (map_terrain_is(target_grid_offset, TERRAIN_ROAD | TERRAIN_HIGHWAY | TERRAIN_ACCESS_RAMP)) {
        // The figure is trying to go through a building such as a roadblock, warehouse or granary
        if (roaming_enabled && map_terrain_is(target_grid_offset, TERRAIN_BUILDING)) {
            building *b = building_get(map_building_at(target_grid_offset));
            if (b->type == BUILDING_GRANARY) {
                if (map_road_get_granary_inner_road_tiles_count(b) < 3) {
                    f->direction = DIR_FIGURE_REROUTE; // do not roam into dead-end granaries
                }
            }
            if (building_type_is_roadblock(b->type)) {
                // do not allow roaming through roadblock without permissions
                int permission = get_permission_for_figure_type(f);
                if (!building_roadblock_get_permission(permission, b)) {
                    f->direction = DIR_FIGURE_REROUTE;
                }
            }
        }
    // The figure is trying to go into a building
    } else if (map_terrain_is(target_grid_offset, TERRAIN_BUILDING)) {
        if ((map_routing_citizen_is_passable_terrain(target_grid_offset) ||
            (map_routing_citizen_is_road(target_grid_offset) && !roaming_enabled))) {
            return; // passable terrain - no reroute
        }
        building *b = building_get(map_building_at(target_grid_offset));
        if (building_type_is_roadblock(b->type) && roaming_enabled) { //only block roaming
            int permission = get_permission_for_figure_type(f);
            if (!building_roadblock_get_permission(permission, b)) {
                f->direction = DIR_FIGURE_REROUTE;
            }
        } else {
            if (b->type != BUILDING_FORT_GROUND) {
                f->direction = DIR_FIGURE_REROUTE;
            }

        }
    // The figure is trying to go into impassable terrain
    } else if (map_terrain_is(target_grid_offset, TERRAIN_IMPASSABLE)) {
        f->direction = DIR_FIGURE_REROUTE;
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

// Determines roaming movement
void figure_movement_roam_ticks(figure *f, int num_ticks)
{
    // Does not need to choose destination, walks one tick then decides what to do
    if (f->roam_choose_destination == 0) {
        walk_ticks(f, num_ticks, 1);
        // Figure arrives
        if (f->direction == DIR_FIGURE_AT_DESTINATION) {
            f->roam_choose_destination = 1;
            f->roam_length = 0;
        // Figure is forced to reroute or is lost, changing destination
        } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
            f->roam_choose_destination = 1;
        }
        // The figure needs to choose destination
        if (f->roam_choose_destination) {
            f->roam_ticks_until_next_turn = 100;
            f->direction = f->previous_tile_direction; // unclear where?
        } else {
            return;
        }
    }
    // no destination: walk to end of tile and pick a direction
    while (num_ticks > 0) {
        num_ticks--;
        f->progress_on_tile++;
        // Moves to the end of the tile if it hasn't already
        if (f->progress_on_tile < TICKS_PER_TILE) {
            advance_tick(f);
        // Arrives at the next tile
        } else {
            f->progress_on_tile = TICKS_PER_TILE;
            f->roam_random_counter++;
            int came_from_direction = (f->previous_tile_direction + 4) % 8;
            if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
                if (figure_service_provide_coverage(f)) {
                    return;
                }
            }
            int road_tiles[8];
            int permission = get_permission_for_figure_type(f);
            int adjacent_road_tiles = map_get_adjacent_road_tiles_for_roaming(f->grid_offset, road_tiles, permission);
            if (adjacent_road_tiles == 3 && map_get_diagonal_road_tiles_for_roaming(f->grid_offset, road_tiles) >= 5) {
                // go in the straight direction of a double-wide road
                adjacent_road_tiles = 2;
                if (came_from_direction == DIR_0_TOP || came_from_direction == DIR_4_BOTTOM) {
                    if (road_tiles[0] && road_tiles[4]) {
                        road_tiles[2] = road_tiles[6] = 0;
                    } else {
                        road_tiles[0] = road_tiles[4] = 0;
                    }
                } else {
                    if (road_tiles[2] && road_tiles[6]) {
                        road_tiles[0] = road_tiles[4] = 0;
                    } else {
                        road_tiles[2] = road_tiles[6] = 0;
                    }
                }
            }
            if (adjacent_road_tiles == 4 && map_get_diagonal_road_tiles_for_roaming(f->grid_offset, road_tiles) >= 8) {
                // go straight on when all surrounding tiles are road
                adjacent_road_tiles = 2;
                if (came_from_direction == DIR_0_TOP || came_from_direction == DIR_4_BOTTOM) {
                    road_tiles[2] = road_tiles[6] = 0;
                } else {
                    road_tiles[0] = road_tiles[4] = 0;
                }
            }
            // Ends the roaming walk if there are no road tiles around
            if (adjacent_road_tiles <= 0) {
                f->roam_length = f->max_roam_length;
                return;
            }
            if (adjacent_road_tiles == 1) {
                int dir = 0;
                do {
                    f->direction = 2 * dir;
                } while (!road_tiles[f->direction] && dir++ < 4);
            } else if (adjacent_road_tiles == 2) {
                if (f->roam_ticks_until_next_turn == -1) {
                    roam_set_direction(f, permission);
                    came_from_direction = -1;
                }
                // 1. continue in the same direction
                // 2. turn in the direction given by roam_turn_direction
                int dir = 0;
                do {
                    if (road_tiles[f->direction] && f->direction != came_from_direction) {
                        break;
                    }
                    f->direction += f->roam_turn_direction;
                    if (f->direction > 6) {
                        f->direction = 0;
                    } else if (f->direction < 0) {
                        f->direction = 6;
                    }
                } while (dir++ < 4);
            } else { // > 2 road tiles
                f->direction = (f->roam_random_counter + map_random_get(f->grid_offset)) & 6;
                if (!road_tiles[f->direction] || f->direction == came_from_direction) {
                    f->roam_ticks_until_next_turn--;
                    if (f->roam_ticks_until_next_turn <= 0) {
                        roam_set_direction(f, permission);
                        came_from_direction = -1;
                    }
                    int dir = 0;
                    do {
                        if (road_tiles[f->direction] && f->direction != came_from_direction) {
                            break;
                        }
                        f->direction += f->roam_turn_direction;
                        if (f->direction > 6) {
                            f->direction = 0;
                        } else if (f->direction < 0) {
                            f->direction = 6;
                        }
                    } while (dir++ < 4);
                }
            }
            f->routing_path_current_tile++;
            f->previous_tile_direction = f->direction;
            f->progress_on_tile = 0;
            move_to_next_tile(f);
            if (f->faction_id != FIGURE_FACTION_ROAMER_PREVIEW) {
                advance_tick(f);
            }
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

void figure_movement_set_cross_country_direction(figure *f, int x_src, int y_src, int x_dst, int y_dst, int is_missile)
{
    // all x/y are in 1/15th of a tile
    f->cc_destination_x = x_dst;
    f->cc_destination_y = y_dst;
    f->cc_delta_x = (x_src > x_dst) ? (x_src - x_dst) : (x_dst - x_src);
    f->cc_delta_y = (y_src > y_dst) ? (y_src - y_dst) : (y_dst - y_src);
    if (f->cc_delta_x < f->cc_delta_y) {
        f->cc_delta_xy = 2 * f->cc_delta_x - f->cc_delta_y;
    } else if (f->cc_delta_y < f->cc_delta_x) {
        f->cc_delta_xy = 2 * f->cc_delta_y - f->cc_delta_x;
    } else { // equal
        f->cc_delta_xy = 0;
    }
    if (is_missile) {
        f->direction = calc_missile_direction(x_src, y_src, x_dst, y_dst);
    } else {
        f->direction = calc_general_direction(x_src, y_src, x_dst, y_dst);
        if (f->cc_delta_y > 2 * f->cc_delta_x) {
            switch (f->direction) {
                case DIR_1_TOP_RIGHT: case DIR_7_TOP_LEFT: f->direction = DIR_0_TOP; break;
                case DIR_3_BOTTOM_RIGHT: case DIR_5_BOTTOM_LEFT: f->direction = DIR_4_BOTTOM; break;
            }
        }
        if (f->cc_delta_x > 2 * f->cc_delta_y) {
            switch (f->direction) {
                case DIR_1_TOP_RIGHT: case DIR_3_BOTTOM_RIGHT: f->direction = DIR_2_RIGHT; break;
                case DIR_5_BOTTOM_LEFT: case DIR_7_TOP_LEFT: f->direction = DIR_6_LEFT; break;
            }
        }
    }
    if (f->cc_delta_x >= f->cc_delta_y) {
        f->cc_direction = 1;
    } else {
        f->cc_direction = 2;
    }
}

void figure_movement_set_cross_country_destination(figure *f, int x_dst, int y_dst)
{
    f->destination_x = x_dst;
    f->destination_y = y_dst;
    figure_movement_set_cross_country_direction(
        f, f->cross_country_x, f->cross_country_y,
        TICKS_PER_TILE * x_dst, TICKS_PER_TILE * y_dst, 0);
}

// Calculates the direction when travelling cross country (no roads)
// Be very careful with this, it is an algorithm from somewhere else
static void cross_country_update_delta(figure *f)
{
    if (f->cc_direction == 1) { // x
        if (f->cc_delta_xy >= 0) {
            f->cc_delta_xy += 2 * (f->cc_delta_y - f->cc_delta_x);
        } else {
            f->cc_delta_xy += 2 * f->cc_delta_y;
        }
        f->cc_delta_x--;
    } else { // y
        if (f->cc_delta_xy >= 0) {
            f->cc_delta_xy += 2 * (f->cc_delta_x - f->cc_delta_y);
        } else {
            f->cc_delta_xy += 2 * f->cc_delta_x;
        }
        f->cc_delta_y--;
    }
}
static void cross_country_advance_x(figure *f)
{
    if (f->cross_country_x < f->cc_destination_x) {
        f->cross_country_x++;
    } else if (f->cross_country_x > f->cc_destination_x) {
        f->cross_country_x--;
    }
}
static void cross_country_advance_y(figure *f)
{
    if (f->cross_country_y < f->cc_destination_y) {
        f->cross_country_y++;
    } else if (f->cross_country_y > f->cc_destination_y) {
        f->cross_country_y--;
    }
}


static void cross_country_advance(figure *f)
{
    cross_country_update_delta(f);
    if (f->cc_direction == 2) { // y
        cross_country_advance_y(f);
        if (f->cc_delta_xy >= 0) {
            f->cc_delta_x--;
            cross_country_advance_x(f);
        }
    } else {
        cross_country_advance_x(f);
        if (f->cc_delta_xy >= 0) {
            f->cc_delta_y--;
            cross_country_advance_y(f);
        }
    }
}

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
