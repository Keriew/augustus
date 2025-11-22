#ifndef MAP_ROUTING_H
#define MAP_ROUTING_H

#include "core/buffer.h"
#include "map/grid.h"

typedef enum {
    ROUTED_BUILDING_ROAD = 0,
    ROUTED_BUILDING_WALL = 1,
    ROUTED_BUILDING_AQUEDUCT = 2,
    ROUTED_BUILDING_AQUEDUCT_WITHOUT_GRAPHIC = 4,
    ROUTED_BUILDING_HIGHWAY = 5,
    ROUTED_BUILDING_DRAGGABLE_RESERVOIR = 6
} routed_building_type;

typedef struct map_routing_distance_grid {
    grid_i16 possible;
    grid_i16 determined;
    int dst_x;
    int dst_y;
} map_routing_distance_grid;

typedef enum {
    // Land
    ROUTE_TYPE_CITIZEN, // placeholder?
    ROUTE_TYPE_TOWER_SENTRY,
    ROUTE_TYPE_FRIENDLY,
    ROUTE_TYPE_ENEMY,
    // Water
    ROUTE_TYPE_BOAT,
    ROUTE_TYPE_FLOTSAM,
    // Buildings
    ROUTE_TYPE_ROAD,
    ROUTE_TYPE_HIGHWAY,
    ROUTE_TYPE_WALL,
    ROUTE_TYPE_AQUEDUCT,
    // Misc
    ROUTE_TYPE_UNBLOCKING_ROME,
} RouteType;

enum { // Includes movement cost
    CITIZEN_0_ROAD = 100,
    CITIZEN_1_HIGHWAY = 50,
    CITIZEN_2_PASSABLE_TERRAIN = 100,
    CITIZEN_4_CLEAR_TERRAIN = 200,
    CITIZEN_N1_BLOCKED = 200,
    CITIZEN_N3_AQUEDUCT = 200,
    CITIZEN_N4_RESERVOIR_CONNECTOR = 200,

    NONCITIZEN_0_PASSABLE = 100,
    NONCITIZEN_1_BUILDING = 200,
    NONCITIZEN_2_CLEARABLE = 200,
    NONCITIZEN_3_WALL = 200,
    NONCITIZEN_4_GATEHOUSE = 200,
    NONCITIZEN_5_FORT = 200,
    NONCITIZEN_N1_BLOCKED = 200,

    WATER_0_PASSABLE = 100,
    WATER_N3_LOW_BRIDGE = 100, // Small boats can be added!
    WATER_N1_BLOCKED = 100,
    WATER_N2_MAP_EDGE = 200,

    WALL_0_PASSABLE = 100,
    WALL_N1_BLOCKED = 200,
};
extern grid_i8 terrain_type

const map_routing_distance_grid *map_routing_get_distance_grid(void);

void map_routing_calculate_distances(int x, int y);
void map_routing_calculate_distances_water_boat(int x, int y);
void map_routing_calculate_distances_water_flotsam(int x, int y);

int map_routing_calculate_distances_for_building(routed_building_type type, int x, int y);

void map_routing_delete_first_wall_or_aqueduct(int x, int y);

int map_routing_distance(int grid_offset);

int map_routing_citizen_can_travel_over_land(int src_x, int src_y, int dst_x, int dst_y, int num_directions);
int map_routing_citizen_can_travel_over_road_garden(int src_x, int src_y, int dst_x, int dst_y, int num_directions);
int map_routing_citizen_can_travel_over_road_garden_highway(int src_x, int src_y, int dst_x, int dst_y, int num_directions);
int map_routing_can_travel_over_walls(int src_x, int src_y, int dst_x, int dst_y, int num_directions);

int map_routing_noncitizen_can_travel_over_land(
    int src_x, int src_y, int dst_x, int dst_y, int num_directions, int only_through_building_id, int max_tiles);
int map_routing_noncitizen_can_travel_through_everything(int src_x, int src_y, int dst_x, int dst_y, int num_directions);

void map_routing_add_source_area(int x, int y, int size);

void map_routing_save_state(buffer *buf);

void map_routing_load_state(buffer *buf);

#endif // MAP_ROUTING_H
