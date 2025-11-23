#ifndef MAP_ROUTING_H
#define MAP_ROUTING_H

#include "core/buffer.h"
#include "map/grid.h"

#define MAX_QUEUE GRID_SIZE * GRID_SIZE // why is it this number instead of another?
#define MAX_SEARCH_ITERATIONS 50000 // higher? lower?
#define BASE_TERRAIN_COST(x) ((int)(x))
#define MAX_CONCURRENT_ROUTES 32

typedef struct map_routing_distance_grid {
    grid_i16 possible;
    grid_i16 determined;
    int dst_x;
    int dst_y;
} map_routing_distance_grid;

// Routing Context
typedef struct {
    // Core Pathfinding Data
    map_routing_distance_grid route_map_pool[MAX_CONCURRENT_ROUTES];
    struct { int head; int tail; int items[MAX_QUEUE]; } queue;
    // Specialized Working Buffers
    map_routing_distance_grid fighting_data;
    // Stores the position (index) within the queue.items array for every grid offset.
    int node_index[GRID_SIZE * GRID_SIZE];
    // ... possibly other single-use flags or temporary data ...
} RoutingContext;
// Provides context to ctx (?)
typedef int (*RoutingCallback)(RoutingContext *ctx, int offset, int next_offset, int direction);

typedef enum {
    ROUTED_BUILDING_ROAD = 0,
    ROUTED_BUILDING_WALL = 1,
    ROUTED_BUILDING_AQUEDUCT = 2,
    ROUTED_BUILDING_AQUEDUCT_WITHOUT_GRAPHIC = 4,
    ROUTED_BUILDING_HIGHWAY = 5,
    ROUTED_BUILDING_DRAGGABLE_RESERVOIR = 6
} routed_building_type;


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

// Extern
extern map_routing_distance_grid distance; // Global Distance Map --> Shouldn't there be multiple???
extern RoutingContext g_context; // NOTE: Global Routing Context (required by fix for previous error)

#endif // MAP_ROUTING_H
