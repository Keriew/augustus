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
#include "map/sprite.h"
#include "map/terrain.h"

static void map_routing_update_access_noncitizen(void);

void map_routing_update_all(void)
{
    map_routing_update_access();
    map_routing_update_water();
    map_routing_access();
}

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

// Aqueducts
static int get_building_access_aqueduct(int grid_offset)
{
    int image_id = map_image_at(grid_offset) - image_group(GROUP_BUILDING_AQUEDUCT);
    switch (image_id) {
        case 0: case 1: case 2: case 3:
        case 8: case 9:
        case 15: case 16: case 17: case 18:
        case 23: case 24:
            return TERRAIN_ACCESS_AQUEDUCT;

            // All other sprites are blocked
        default:
            return TERRAIN_ACCESS_BLOCKED;
    }
}
void map_routing_access(void)
{
    int grid_offset = map_data.start_offset;

    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {

            // Assign access based on the helper result
            terrain_access.items[grid_offset] = determine_wall_access(grid_offset);
        }
    }
}





static int determine_wall_access(int grid_offset)
{
    // 1. Gatehouses: Always passable for wall walkers
    if (map_terrain_is(grid_offset, TERRAIN_GATEHOUSE)) {
        return TERRAIN_ACCESS_WALL_PASSABLE;
    }

    // 2. Walls: Passable only under specific adjacency conditions
    if (map_terrain_is(grid_offset, TERRAIN_WALL)) {
        if (count_adjacent_wall_tiles(grid_offset) == 3) {
            return TERRAIN_ACCESS_WALL_PASSABLE;
        }
        return TERRAIN_ACCESS_WALL_BLOCKED;
    }
}
// Counts adjacent wall tiles
static int is_wall_tile(int grid_offset)
{
    return map_terrain_is(grid_offset, TERRAIN_WALL_OR_GATEHOUSE) ? 1 : 0;
}
static int count_adjacent_wall_tiles(int grid_offset)
{
    int adjacent = 0;
    switch (city_view_orientation()) {
        case DIR_TOP:
            adjacent += is_wall_tile(grid_offset + map_grid_delta(0, 1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(1, 1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(1, 0));
            break;
        case DIR_RIGHT:
            adjacent += is_wall_tile(grid_offset + map_grid_delta(0, 1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(-1, 1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(-1, 0));
            break;
        case DIR_BOTTOM:
            adjacent += is_wall_tile(grid_offset + map_grid_delta(0, -1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(-1, -1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(-1, 0));
            break;
        case DIR_LEFT:
            adjacent += is_wall_tile(grid_offset + map_grid_delta(0, -1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(1, -1));
            adjacent += is_wall_tile(grid_offset + map_grid_delta(1, 0));
            break;
    }
    return adjacent;
}


static int wall_tile_in_radius(int x, int y, int radius, int *x_wall, int *y_wall)
{
    int size = 1;
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, size, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            if (map_routing_is_wall_passable(map_grid_offset(xx, yy))) {
                *x_wall = xx;
                *y_wall = yy;
                return 1;
            }
        }
    }
    return 0;
}
int map_routing_wall_tile_in_radius(int x, int y, int radius, int *x_wall, int *y_wall)
{
    for (int i = 1; i <= radius; i++) {
        if (wall_tile_in_radius(x, y, i, x_wall, y_wall)) {
            return 1;
        }
    }
    return 0;
}




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
    return map_terrain_is(grid_offset + map_grid_delta(0, -1), terrain_access) &&
        map_terrain_is(grid_offset + map_grid_delta(-1, 0), terrain_access) &&
        map_terrain_is(grid_offset + map_grid_delta(1, 0), terrain_access) &&
        map_terrain_is(grid_offset + map_grid_delta(0, 1), terrain_access);
}
// Calculates the terrain access for water tiles
void map_terrain_access_water_update(void)
{
    int grid_offset = map_data.start_offset;

    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
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
    }
}

// Calculates the terrain access for building tiles
static int get_building_access(int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));

    // Safety check (optional, but good practice)
    if (!b) return TERRAIN_ACCESS_BLOCKED;

    switch (b->type) {
        // --- SIMPLE ROAD ACCESS ---
        case BUILDING_WAREHOUSE:
        case BUILDING_ROADBLOCK:
        case BUILDING_LOW_BRIDGE: // Not sure if it should count as a road
        case BUILDING_SHIP_BRIDGE: // Not sure if it should count as a road
            return TERRAIN_ACCESS_ROAD;

            // --- PASSABLE GROUND ---
        case BUILDING_FORT_GROUND:
        case BUILDING_ROOFED_GARDEN_WALL_GATE:
        case BUILDING_LOOPED_GARDEN_GATE:
        case BUILDING_PANELLED_GARDEN_GATE:
        case BUILDING_HEDGE_GATE_DARK:
        case BUILDING_HEDGE_GATE_LIGHT:
            return TERRAIN_ACCESS_PASSABLE;

            // --- GATEHOUSE (Context Sensitive) ---
        case BUILDING_GATEHOUSE:
            if (map_terrain_get(grid_offset) & TERRAIN_HIGHWAY) {
                return TERRAIN_ACCESS_HIGHWAY;
            }
            return TERRAIN_ACCESS_ROAD;

            // --- AQUEDUCT ---
        case BUILDING_AQUEDUCT:
            return get_building_access_aqueduct(grid_offset);

            // --- WALL ---
            // Put the wall passable and blocked determiner here


                // --- GRANARY (Internal Cart Access) ---
        case BUILDING_GRANARY:
            // Granaries allow access in a "+" shape (Top, Left, Center, Right, Bottom)
            // They block access at the 4 corners.
            switch (map_property_multi_tile_xy(grid_offset)) {
                case EDGE_X1Y0: // Top Middle
                case EDGE_X0Y1: // Left Middle
                case EDGE_X1Y1: // Center
                case EDGE_X2Y1: // Right Middle
                case EDGE_X1Y2: // Bottom Middle
                    return TERRAIN_ACCESS_ROAD;
                default:
                    return TERRAIN_ACCESS_BLOCKED;
            }

            // --- MILITARY FORTS ---
        case BUILDING_FORT_ARCHERS:
        case BUILDING_FORT_LEGIONARIES:
        case BUILDING_FORT_JAVELIN:
        case BUILDING_FORT_MOUNTED:
        case BUILDING_FORT_AUXILIA_INFANTRY:
            return TERRAIN_ACCESS_FORT;

            // Native structures and ruins are obstacles that cannot be entered or walked through
        case BUILDING_BURNING_RUIN:
        case BUILDING_NATIVE_HUT:
        case BUILDING_NATIVE_HUT_ALT:
        case BUILDING_NATIVE_MEETING:
        case BUILDING_NATIVE_CROPS:
        case BUILDING_NATIVE_DECORATION:
        case BUILDING_NATIVE_MONUMENT:
        case BUILDING_NATIVE_WATCHTOWER:
            return TERRAIN_ACCESS_IMPASSABLE;

            // --- RESERVOIR (Pipe Connections) ---
        case BUILDING_RESERVOIR:
            // Reservoirs connect pipes at the 4 cardinal edges, but NOT the center.
            switch (map_property_multi_tile_xy(grid_offset)) {
                case EDGE_X1Y0: // Top Middle
                case EDGE_X0Y1: // Left Middle
                case EDGE_X2Y1: // Right Middle
                case EDGE_X1Y2: // Bottom Middle
                    return TERRAIN_ACCESS_RESERVOIR_CONNECTOR;
                default:
                    return TERRAIN_ACCESS_BLOCKED;
            }

            // --- DEFAULT / UNKNOWN ---
        default:
            return TERRAIN_ACCESS_BLOCKED;
    }
}

// Determines the access type of a tile
static int determine_access_type(int grid_offset, int terrain)
{
    // 1. Roads & Highways
    if (terrain & TERRAIN_ROAD) {
        return TERRAIN_ACCESS_ROAD;
    }
    if (terrain & TERRAIN_HIGHWAY) {
        return TERRAIN_ACCESS_HIGHWAY;
    }
    // 2. Passable Terrain (Rubble, Gardens, Ramps)
    if (terrain & (TERRAIN_RUBBLE | TERRAIN_ACCESS_RAMP | TERRAIN_GARDEN)) {
        // Note: standardized naming if TERRAIN_ACCESS_PASSABLE exists, otherwise keep CITIZEN_2...
        return TERRAIN_ACCESS_PASSABLE;
    }
    // 3. Buildings & Gatehouses
    if (terrain & (TERRAIN_BUILDING | TERRAIN_GATEHOUSE)) {
        // Critical Safety Check: Does the building actually exist?
        if (!map_building_at(grid_offset)) {
            repair_ghost_building(grid_offset);
            return TERRAIN_ACCESS_EMPTY;
        }
        return get_building_access(grid_offset);
    }
    // 4. Aqueducts
    if (terrain & TERRAIN_AQUEDUCT) {
        return get_building_access(grid_offset);
    }
    // 5. Blocked Terrain
    if (terrain & TERRAIN_NOT_CLEAR) {
        return TERRAIN_ACCESS_BLOCKED;
    }

    // 6. Default (Open Land)
    return TERRAIN_ACCESS_EMPTY;
}


// --- CORE FUNCTION ---
void map_routing_update_access(void)
{
    // Initialize the grid
    map_grid_init_i8(terrain_access.items, -1);

    int grid_offset = map_data.start_offset;

    // Iterate over the map
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {

            int terrain = map_terrain_get(grid_offset);

            // Assign the calculated access type
            terrain_access.items[grid_offset] = determine_access_type(grid_offset, terrain);
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