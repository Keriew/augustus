#include "routing_terrain.h"

#include "building/building.h"
#include "city/view.h"
#include "core/direction.h"
#include "core/image.h"
#include "map/building.h"
#include "map/data.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/routing_data.h"
#include "map/routing.h"
#include "map/sprite.h"
#include "map/terrain.h"

// --- HELPER FUNCTIONS ---
// Repairs ghost buildings
static void repair_ghost_building(int grid_offset)
{
    // Remove the terrain flag
    map_terrain_remove(grid_offset, TERRAIN_BUILDING);

    // Reset visual appearance to grass
    int grass_image = (map_random_get(grid_offset) & 7) + image_group(GROUP_TERRAIN_GRASS_1);
    map_image_set(grid_offset, grass_image);

    // Update map properties
    map_property_mark_draw_tile(grid_offset);
    map_property_set_multi_tile_size(grid_offset, 1);
}

// --- CITIZEN ACCESS ---
// Citizen Access
static int get_building_access_citizen(int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    int terrain = map_terrain_get(grid_offset);
    int type = TERRAIN_ACCESS_CITIZEN_BLOCKED;
    switch (b->type) {
        case BUILDING_WAREHOUSE:
            type = TERRAIN_ACCESS_CITIZEN_ROAD;
            break;
        case BUILDING_GATEHOUSE:
            if (terrain & TERRAIN_HIGHWAY) {
                type = TERRAIN_ACCESS_CITIZEN_HIGHWAY;
            } else {
                type = TERRAIN_ACCESS_CITIZEN_ROAD;
            }
            break;
        case BUILDING_ROADBLOCK:
            type = TERRAIN_ACCESS_CITIZEN_ROAD;
            break;
        case BUILDING_FORT_GROUND:
            type = TERRAIN_ACCESS_CITIZEN_PASSABLE;
            break;
        case BUILDING_GRANARY:
            switch (map_property_multi_tile_xy(grid_offset)) {
                case EDGE_X1Y0:
                case EDGE_X0Y1:
                case EDGE_X1Y1:
                case EDGE_X2Y1:
                case EDGE_X1Y2:
                    type = TERRAIN_ACCESS_CITIZEN_ROAD;
                    break;
            }
            break;
        case BUILDING_RESERVOIR:
            switch (map_property_multi_tile_xy(grid_offset)) {
                case EDGE_X1Y0:
                case EDGE_X0Y1:
                case EDGE_X2Y1:
                case EDGE_X1Y2:
                    type = TERRAIN_ACCESS_CITIZEN_RESERVOIR_CONNECTOR; // aqueduct connect points
                    break;
            }
            break;
        default:
            return TERRAIN_ACCESS_CITIZEN_BLOCKED;
    }
    return type;
}
// Aqueducts (citizens)
static int get_citizen_access_aqueduct(int grid_offset)
{
    int image_id = map_image_at(grid_offset) - image_group(GROUP_BUILDING_AQUEDUCT);
    switch (image_id) {
        case 0: case 1: case 2: case 3:
        case 8: case 9:
        case 15: case 16: case 17: case 18:
        case 23: case 24:
            return TERRAIN_ACCESS_CITIZEN_AQUEDUCT;

            // All other sprites are blocked
        default:
            return TERRAIN_ACCESS_CITIZEN_BLOCKED;
    }
}
void map_terrain_access_citizen(int grid_offset)
{
    map_grid_init_i8(terrain_access_citizen.items, -1);
    int terrain = map_terrain_get(grid_offset);
    if (terrain & TERRAIN_ROAD) {
        terrain_access_citizen.items[grid_offset] = TERRAIN_ACCESS_CITIZEN_ROAD;
    } else if (terrain & TERRAIN_HIGHWAY) {
        terrain_access_citizen.items[grid_offset] = TERRAIN_ACCESS_CITIZEN_HIGHWAY;
    } else if (terrain & (TERRAIN_RUBBLE | TERRAIN_ACCESS_RAMP | TERRAIN_GARDEN)) {
        terrain_access_citizen.items[grid_offset] = TERRAIN_ACCESS_CITIZEN_PASSABLE;
    } else if (terrain & (TERRAIN_BUILDING | TERRAIN_GATEHOUSE)) {
        if (!map_building_at(grid_offset)) {
            // shouldn't happen
            terrain_access_citizen.items[grid_offset] = TERRAIN_ACCESS_CITIZEN_CLEAR;
            repair_ghost_building(grid_offset)
                continue;
        }
        terrain_access_citizen.items[grid_offset] = get_land_type_citizen_building(grid_offset);
    } else if (terrain & TERRAIN_AQUEDUCT) {
        terrain_access_citizen.items[grid_offset] = get_land_type_citizen_aqueduct(grid_offset);
    } else if (terrain & TERRAIN_NOT_CLEAR) {
        terrain_access_citizen.items[grid_offset] = TERRAIN_ACCESS_CITIZEN_BLOCKED;
    } else {
        terrain_access_citizen.items[grid_offset] = TERRAIN_ACCESS_CITIZEN_CLEAR;
    }
}

// --- NON-CITIZEN TERRAIN ACCESS ---
static int get_building_access_noncitizen(int grid_offset)
{
    int type = TERRAIN_ACCESS_NONCITIZEN_BUILDING;
    switch (building_get(map_building_at(grid_offset))->type) {
        default:
            return TERRAIN_ACCESS_NONCITIZEN_BUILDING; //add wall here
        case BUILDING_WAREHOUSE:
        case BUILDING_FORT_GROUND:
            type = TERRAIN_ACCESS_NONCITIZEN_PASSABLE;
            break;
        case BUILDING_BURNING_RUIN:
        case BUILDING_NATIVE_HUT:
        case BUILDING_NATIVE_HUT_ALT:
        case BUILDING_NATIVE_MEETING:
        case BUILDING_NATIVE_CROPS:
        case BUILDING_NATIVE_DECORATION:
        case BUILDING_NATIVE_MONUMENT:
        case BUILDING_NATIVE_WATCHTOWER:
            type = TERRAIN_ACCESS_NONCITIZEN_BLOCKED;
            break;
        case BUILDING_FORT_ARCHERS:
        case BUILDING_FORT_LEGIONARIES:
        case BUILDING_FORT_JAVELIN:
        case BUILDING_FORT_MOUNTED:
        case BUILDING_FORT_AUXILIA_INFANTRY:
            type = TERRAIN_ACCESS_NONCITIZEN_FORT;
            break;
        case BUILDING_GRANARY:
            switch (map_property_multi_tile_xy(grid_offset)) { //granary cross allways passable
                case EDGE_X1Y0:
                case EDGE_X0Y1:
                case EDGE_X1Y1:
                case EDGE_X2Y1:
                case EDGE_X1Y2:
                    type = TERRAIN_ACCESS_NONCITIZEN_PASSABLE;
                    break;
            }
            break;
        case BUILDING_ROOFED_GARDEN_WALL_GATE:
        case BUILDING_LOOPED_GARDEN_GATE:
        case BUILDING_PANELLED_GARDEN_GATE:
        case BUILDING_ROADBLOCK:
        case BUILDING_HEDGE_GATE_DARK:
        case BUILDING_HEDGE_GATE_LIGHT:
        case BUILDING_SHIP_BRIDGE:
        case BUILDING_LOW_BRIDGE:
            type = TERRAIN_ACCESS_NONCITIZEN_PASSABLE;
            break;
        case BUILDING_WALL:
            type = TERRAIN_ACCESS_NONCITIZEN_WALL;
            break;
    }
    return type;
}
static int map_terrain_access_noncitizen(int grid_offset)
{
    map_grid_init_i8(terrain_access_noncitizen.items, -1);
    int terrain = map_terrain_get(grid_offset);
    if (terrain & TERRAIN_GATEHOUSE) {
        terrain_access_noncitizen.items[grid_offset] = TERRAIN_ACCESS_NONCITIZEN_GATEHOUSE;
    } else if (terrain & TERRAIN_BUILDING) {
        terrain_access_noncitizen.items[grid_offset] = get_building_access_type_noncitizen(grid_offset);
    } else if (terrain & TERRAIN_ROAD) {
        terrain_access_noncitizen.items[grid_offset] = TERRAIN_ACCESS_NONCITIZEN_PASSABLE;
    } else if (terrain & TERRAIN_HIGHWAY) {
        terrain_access_noncitizen.items[grid_offset] = TERRAIN_ACCESS_NONCITIZEN_PASSABLE;
    } else if (terrain & (TERRAIN_GARDEN | TERRAIN_ACCESS_RAMP | TERRAIN_RUBBLE)) {
        terrain_access_noncitizen.items[grid_offset] = TERRAIN_ACCESS_NONCITIZEN_CLEARABLE;
    } else if (terrain & TERRAIN_AQUEDUCT) {
        terrain_access_noncitizen.items[grid_offset] = TERRAIN_ACCESS_NONCITIZEN_CLEARABLE;
    } else if (terrain & TERRAIN_WALL) {
        terrain_access_noncitizen.items[grid_offset] = TERRAIN_ACCESS_NONCITIZEN_WALL;
    } else if (terrain & TERRAIN_NOT_CLEAR) {
        terrain_access_noncitizen.items[grid_offset] = TERRAIN_ACCESS_NONCITIZEN_BLOCKED;
    } else {
        terrain_access_noncitizen.items[grid_offset] = TERRAIN_ACCESS_NONCITIZEN_PASSABLE;
    }
}

// --- WALL TERRAIN ACCESS ---
static int is_wall_tile(int grid_offset)
{
    return map_terrain_is(grid_offset, TERRAIN_WALL_OR_GATEHOUSE);
}
static int count_adjacent_wall_tiles(int grid_offset)
{
    int adjacent = 0;
    // Check the 4 cardinal directions
    adjacent += is_wall_tile(grid_offset + map_grid_delta(0, -1)); // North
    adjacent += is_wall_tile(grid_offset + map_grid_delta(1, 0));  // East
    adjacent += is_wall_tile(grid_offset + map_grid_delta(0, 1));  // South
    adjacent += is_wall_tile(grid_offset + map_grid_delta(-1, 0)); // West
    return adjacent;
}
static int determine_wall_access(int grid_offset)
{
    // 1. Gatehouses are always passable
    if (map_terrain_is(grid_offset, TERRAIN_GATEHOUSE)) {
        return TERRAIN_ACCESS_WALL_PASSABLE;
    }
    // 2. Walls check connectivity
    if (map_terrain_is(grid_offset, TERRAIN_WALL)) {
        // CRITIQUE: This condition (== 3) limits access to T-junctions or specific clusters.
        // If you intended "thick walls" (2x2 blocks), you might need a different check.
        if (count_adjacent_wall_tiles(grid_offset) >= 3) {
            return TERRAIN_ACCESS_WALL_PASSABLE;
        }
        return TERRAIN_ACCESS_WALL_BLOCKED;
    }
    // 3. Default
    return TERRAIN_ACCESS_WALL_BLOCKED;
}
// Finds if there is ANY wall tile within the given radius
int map_routing_wall_tile_in_radius(int x, int y, int radius, int *x_wall, int *y_wall)
{
    int size = 1;
    int x_min, y_min, x_max, y_max;

    // Get the bounding box for the FULL radius immediately
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {

            // Check the calculated wall access map
            if (map_routing_is_wall_passable(map_grid_offset(xx, yy))) {
                *x_wall = xx;
                *y_wall = yy;
                return 1; // Found one!
            }
        }
    }
    return 0;
}
void map_terrain_access_wall(int grid_offset)
{
    // Initialize to default blocked state first (safer)
    map_grid_init_i8(terrain_walls.items, TERRAIN_ACCESS_WALL_BLOCKED);

    terrain_walls.items[grid_offset] = determine_wall_access(grid_offset);
}

// --- WATER TERRAIN ACCESS ---
static int resolve_water_type(int grid_offset)
{
    // Check if there is a special bridge sprite here
    int bridge_sprite = map_sprite_bridge_at(grid_offset);

    switch (bridge_sprite) {
        case 5:
        case 6:
            // Low bridges block large ships
            return TERRAIN_ACCESS_WATER_LOW_BRIDGE;
        case 13:
            // Bridge pillars block everything
            return TERRAIN_ACCESS_WATER_BLOCKED;
        default:
            // Standard open water
            return TERRAIN_ACCESS_WATER;
    }
}
// Checks if the tile is surrounded by water, since boats cannot sail directly next to land
static int is_surrounded_by_water(int grid_offset)
{
    return map_terrain_is(grid_offset + map_grid_delta(0, -1), TERRAIN_WATER) &&
        map_terrain_is(grid_offset + map_grid_delta(-1, 0), TERRAIN_WATER) &&
        map_terrain_is(grid_offset + map_grid_delta(1, 0), TERRAIN_WATER) &&
        map_terrain_is(grid_offset + map_grid_delta(0, 1), TERRAIN_WATER);
}
// Calculates the terrain access for water tiles
void map_terrain_access_water(int grid_offset)
{
    // 1. Check if it is water at all
    if (!map_terrain_is(grid_offset, TERRAIN_WATER)) {
        terrain_access.items[grid_offset] = TERRAIN_ACCESS_WATER_BLOCKED;
        continue;
    }
    // 2. Check Map Edges
    // If we are on the 1-tile border of the map, mark as Edge
    if (x == 0 || x == map_data.width - 1 || y == 0 || y == map_data.height - 1) {
        terrain_access.items[grid_offset] = TERRAIN_ACCESS_WATER_EDGE;
        continue;
    }
    // 3. Optional: Check for validity
    if (!is_surrounded_by_water(grid_offset)) {
        terrain_access.items[grid_offset] = TERRAIN_ACCESS_WATER_BLOCKED;
        continue;
    }

    // 4. Resolve specific water type (Bridges vs Open Water)
    terrain_access.items[grid_offset] = resolve_water_type(grid_offset);
}

// --- CORE FUNCTION ---
void map_routing_update_terrain_access(int citizen, int noncitizen, int wall, int water)
{
    int grid_offset = map_data.start_offset;

    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (citizen) {
                map_terrain_access_citizen(grid_offset)
            }
            if (noncitizen) {
                map_terrain_access_noncitizen(grid_offset)
            }
            if (wall) {
                map_terrain_access_wall(grid_offset)
            }
            if (water) {
                map_terrain_access_water(grid_offset)
            }
        }
    }
}


// --- WRAPPERS ---
int map_routing_citizen_is_passable(int grid_offset)
{
    return terrain_access.items[grid_offset] >= TERRAIN_ACCESS_ROAD &&
        terrain_access.items[grid_offset] <= TERRAIN_ACCESS_PASSABLE;
}
int map_routing_citizen_is_road(int grid_offset)
{
    return terrain_access.items[grid_offset] == TERRAIN_ACCESS_ROAD;
}
int map_routing_citizen_is_highway(int grid_offset)
{
    return terrain_access.items[grid_offset] == TERRAIN_ACCESS_HIGHWAY;
}
int map_routing_citizen_is_passable_terrain(int grid_offset)
{
    return terrain_access.items[grid_offset] == TERRAIN_ACCESS_PASSABLE;
}
int map_routing_noncitizen_is_passable(int grid_offset)
{
    return terrain_access.items[grid_offset] >= TERRAIN_ACCESS_PASSABLE;
}
int map_routing_is_destroyable(int grid_offset)
{
    return terrain_access.items[grid_offset] > TERRAIN_ACCESS_PASSABLE &&
        terrain_access.items[grid_offset] != TERRAIN_ACCESS_FORT;
}
int map_routing_is_wall_passable(int grid_offset)
{
    return terrain_access.items[grid_offset] == TERRAIN_ACCESS_WALL_PASSABLE;
}
// Specific terrain access updates
int map_routing_update_terrain_access_citizen(void)
{
    return map_routing_update_terrain_access(1, 0, 0, 0)
}
int map_routing_update_terrain_access_noncitizen(void)
{
    return map_routing_update_terrain_access(0, 1, 0, 0)
}
int map_routing_update_terrain_access_wall(void)
{
    return map_routing_update_terrain_access(0, 0, 1, 0)
}
int map_routing_update_terrain_access_water(void)
{
    return map_routing_update_terrain_access(0, 0, 0, 1)
}