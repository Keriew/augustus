#include "routing.h"

#include "building/building.h"
#include "core/time.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/road_aqueduct.h"
#include "map/routing_data.h"
#include "map/terrain.h"
#include "map/tiles.h"

#include <stdlib.h>

#define MAX_QUEUE GRID_SIZE * GRID_SIZE
#define MAX_SEARCH_ITERATIONS 50000 // What does this do?

#define UNTIL_STOP 0
#define UNTIL_CONTINUE 1

// Contains the routing context (as opposed to a global object)
typedef struct {
    // Core Pathfinding Data
    map_routing_distance_grid distance;
    struct { int head; int tail; int items[MAX_QUEUE]; } queue;
    // Specialized Working Buffers
    map_routing_distance_grid water_drag;
    map_routing_distance_grid fighting_data;
    // ... possibly other single-use flags or temporary data ...
} RoutingContext;
// Used for callbacks (?)
typedef int (*RoutingCallback)(RoutingContext *ctx, int offset, int next_offset, int direction);

typedef enum {
    DIRECTIONS_NO_DIAGONALS = 4,
    DIRECTIONS_DIAGONALS = 8
} max_directions;

static const int ROUTE_OFFSETS[] = { -162, 1, 162, -1, -161, 163, 161, -163 };
static const int ROUTE_OFFSETS_X[] = { 0, 1, 0, -1,  1, 1, -1, -1 };
static const int ROUTE_OFFSETS_Y[] = { -1, 0, 1,  0, -1, 1,  1, -1 };
static const int HIGHWAY_DIRECTIONS[] = {
    TERRAIN_HIGHWAY_TOP_RIGHT | TERRAIN_HIGHWAY_BOTTOM_RIGHT, // up
    TERRAIN_HIGHWAY_BOTTOM_LEFT | TERRAIN_HIGHWAY_BOTTOM_RIGHT, // right
    TERRAIN_HIGHWAY_TOP_LEFT | TERRAIN_HIGHWAY_BOTTOM_LEFT, // down
    TERRAIN_HIGHWAY_TOP_LEFT | TERRAIN_HIGHWAY_TOP_RIGHT, // left
    0,
    0,
    0,
    0
};

static struct {
    int total_routes_calculated;
    int enemy_routes_calculated;
} stats;

static grid_u8 water_drag;

static struct {
    grid_u8 status;
    time_millis last_check;
} fighting_data;

static struct {
    int through_building_id;
    int dest_building_id;
} state;

static void reset_fighting_status(void)
{
    time_millis current_time = time_get_millis();
    if (current_time != fighting_data.last_check) {
        map_grid_clear_u8(fighting_data.status.items);
        fighting_data.last_check = current_time;
    }
}

const map_routing_distance_grid *map_routing_get_distance_grid(void)
{
    return &distance;
}

static void clear_data(RoutingContext *ctx)
{
    reset_fighting_status();
    map_grid_clear_i16(distance.possible.items);
    map_grid_clear_i16(distance.determined.items);
    ctx->queue.head = 0;
    ctx->queue.tail = 0;
}

// ???
static void enqueue(RoutingContext *ctx, int next_offset, int dist)
{
    // 1. Update the distance grid via the context
    ctx->distance.determined.items[next_offset] = dist;

    // 2. Add item to the queue via the context
    ctx->queue.items[ctx->queue.tail++] = next_offset;

    // 3. Ring Buffer Wrap-around (Must check the CONTEXT'S tail)
    if (ctx->queue.tail >= MAX_QUEUE) {
        ctx->queue.tail = 0;
    }
}

// ???
static inline int queue_pop(RoutingContext *ctx)
{
    // Read the result from the context queue
    int result = ctx->queue.items[ctx->queue.head];

    // Advance the head pointer
    ctx->queue.head++;

    // Check for wrap-around (using the context head)
    if (ctx->queue.head >= MAX_QUEUE) {
        ctx->queue.head = 0;
    }

    return result;
}

static inline int ordered_queue_parent(int index)
{
    return (index - 1) / 2;
}

static inline void ordered_queue_swap(RoutingContext *ctx, int first, int second)
{
    int temp = ctx->queue.items[first];
    ctx->queue.items[first] = ctx->queue.items[second];
    ctx->queue.items[second] = temp;
}

static void ordered_queue_reorder(RoutingContext *ctx, int start_index)
{
    int left_child = 2 * start_index + 1;
    if (left_child >= ctx->queue.tail) {
        return;
    }
    int right_child = left_child + 1;
    int smallest = start_index;

    // Store the distance of the current parent (or smallest node)
    int smallest_dist = ctx->distance.possible.items[ctx->queue.items[smallest]];
    // Compare with Left Child
    int left_dist = ctx->distance.possible.items[ctx->queue.items[left_child]];
    if (left_dist < smallest_dist) {
        smallest = left_child;
        smallest_dist = left_dist; // Update the reference value
    }
    // Compare with Right Child
    if (right_child < ctx->queue.tail) {
        int right_dist = ctx->distance.possible.items[ctx->queue.items[right_child]];
        if (right_dist < smallest_dist) {
            smallest = right_child;
        }
    }

    if (smallest != start_index) {
        ordered_queue_swap(RoutingContext * ctx, start_index, smallest);
        ordered_queue_reorder(RoutingContext * ctx, smallest);
    }
}

// ??
static inline int ordered_queue_pop(RoutingContext *ctx)
{
    // 1. Store the smallest element (at index 0)
    int min_item_offset = ctx->queue.items[0];

    // 2. Decrement the tail pointer (queue size)
    ctx->queue.tail--;

    // 3. Move the item from the now-last position to the head (index 0)
    // The original last item is now at ctx->queue.tail
    ctx->queue.items[0] = ctx->queue.items[ctx->queue.tail];

    // 4. Reorder the heap to fix the heap property
    // MUST pass the context pointer to the helper function!
    ordered_queue_reorder(ctx, 0);

    return min_item_offset;
}

static inline void ordered_queue_reduce_index(RoutingContext *ctx, int index, int offset, int dist)
{
    ctx->queue.items[index] = offset;
    while (index && distance.possible.items[ctx->queue.items[ordered_queue_parent(index)]] > dist) {
        ordered_queue_swap(index, ordered_queue_parent(index));
        index = ordered_queue_parent(index);
    }
}

static void ordered_enqueue(RoutingContext *ctx, int next_offset, int current_dist, int remaining_dist)
{
    int possible_dist = remaining_dist + current_dist;
    int index = ctx->queue.tail;
    if (distance.possible.items[next_offset]) {
        if (distance.possible.items[next_offset] <= possible_dist) {
            return;
        } else {
            for (int i = 0; i < ctx->queue.tail; i++) {
                if (ctx->queue.items[i] == next_offset) {
                    index = i;
                    break;
                }
            }
        }
    } else {
        ctx->queue.tail++;
    }
    distance.determined.items[next_offset] = current_dist;
    distance.possible.items[next_offset] = possible_dist;

    ordered_queue_reduce_index(index, next_offset, possible_dist);
}

static inline int valid_offset(int grid_offset, int possible_dist)
{
    int determined = distance.determined.items[grid_offset];
    return map_grid_is_valid_offset(grid_offset) && (determined == 0 || possible_dist < determined);
}

static inline int distance_left(int x, int y)
{
    return abs(distance.dst_x - x) + abs(distance.dst_y - y);
}

static int receive_highway_bonus(int offset, int direction)
{
    int highway_directions = HIGHWAY_DIRECTIONS[direction];
    if (map_terrain_is(offset, highway_directions)) {
        return 1;
    }
    return 0;
}

static void route_queue_from_to()
{
    // ... setup ...
    while (ctx->queue.tail) {
        // ... pop logic ...
        if (callback(ctx, offset, next_offset, i)) {
            ordered_enqueue(ctx, next_offset, dist, remaining);
        }
    }
}

static void route_queue_from_to(RoutingContext *ctx, int src, int dst, RoutingCallback callback)
// Previous contents
// int src_x, int src_y, int dst_x, int dst_y, int num_directions, int max_tiles,
//    int (*callback)(int offset, int next_offset, int direction)
{
    clear_data();
    distance.dst_x = dst_x;
    distance.dst_y = dst_y;
    int dest = map_grid_offset(dst_x, dst_y);
    ordered_enqueue(ctx, map_grid_offset(src_x, src_y), 1, 0);
    int tiles = 0;
    while (ctx->queue.tail) {
        int offset = ordered_queue_pop();
        if (callback(ctx, offset, next_offset, i)) {
            ordered_enqueue(ctx, next_offset, dist, remaining);
        }
        // if (offset == dest || (max_tiles && ++tiles > max_tiles)) {
        //     break;
        // }
        int x = map_grid_offset_to_x(offset);
        int y = map_grid_offset_to_y(offset);
        distance.possible.items[offset] = 1;
        for (int i = 0; i < num_directions; i++) {
            int next_offset = offset + ROUTE_OFFSETS[i];
            int remaining_dist = distance_left(x + ROUTE_OFFSETS_X[i], y + ROUTE_OFFSETS_Y[i]);
            int dist = 2 + distance.determined.items[offset];
            if (receive_highway_bonus(next_offset, i)) {
                dist--;
            }
            if (valid_offset(next_offset, dist) && callback(offset, next_offset, i)) {
                ordered_enqueue(ctx, next_offset, dist, remaining_dist);
            }
        }
    }
}

static void route_queue_all_from(RoutingContext *ctx, int source, max_directions directions,
    int (*callback)(int next_offset, int dist, int direction), int is_boat)
{
    clear_data();
    map_grid_clear_u8(water_drag.items);
    enqueue(source, 1);
    int tiles = 0;
    while (ctx->queue.head != ctx->queue.tail) {
        if (++tiles > MAX_SEARCH_ITERATIONS) {
            break;
        }
        int offset = queue_pop();
        int drag = is_boat && terrain_water.items[offset] == WATER_N2_MAP_EDGE ? 4 : 0;
        if (water_drag.items[offset] < drag) {
            water_drag.items[offset]++;
            ctx->queue.items[ctx->queue.tail++] = offset;
            if (ctx->queue.tail >= MAX_QUEUE) {
                ctx->queue.tail = 0;
            }
        } else {
            int dist = 1 + distance.determined.items[offset];
            for (max_directions i = 0; i < directions; i++) {
                int route_offset = ROUTE_OFFSETS[i];
                int next_offset = offset + route_offset;
                if (valid_offset(next_offset, dist)) {
                    if (callback(next_offset, dist, i) == UNTIL_STOP) {
                        break;
                    }
                }
            }
        }
    }
}

static int callback_calc_distance(RoutingContext *ctx, int next_offset, int dist, int direction)
{
    if (terrain_land_citizen.items[next_offset] >= CITIZEN_0_ROAD) {
        enqueue(ctx, next_offset, dist);
    }
    return 1;
}

void map_routing_calculate_distances(int x, int y)
{
    ++stats.total_routes_calculated;
    route_queue_all_from(map_grid_offset(x, y), DIRECTIONS_NO_DIAGONALS, callback_calc_distance, 0);
}

static int callback_calc_distance_water_boat(int next_offset, int dist, int direction)
{
    if (terrain_water.items[next_offset] != WATER_N1_BLOCKED &&
        terrain_water.items[next_offset] != WATER_N3_LOW_BRIDGE) {
        enqueue(next_offset, dist);
        if (terrain_water.items[next_offset] == WATER_N2_MAP_EDGE) {
            distance.determined.items[next_offset] += 4;
        }
    }
    return 1;
}

void map_routing_calculate_distances_water_boat(int x, int y)
{
    int grid_offset = map_grid_offset(x, y);
    if (terrain_water.items[grid_offset] == WATER_N1_BLOCKED) {
        clear_data();
    } else {
        route_queue_all_from(grid_offset, DIRECTIONS_NO_DIAGONALS, callback_calc_distance_water_boat, 1);
    }
}

static int callback_calc_distance_water_flotsam(RoutingContext *ctx, int next_offset, int dist, int direction)
{
    if (terrain_water.items[next_offset] != WATER_N1_BLOCKED) {
        enqueue(ctx, next_offset, dist);
    }
    return 1;
}

void map_routing_calculate_distances_water_flotsam(int x, int y)
{
    int grid_offset = map_grid_offset(x, y);
    if (terrain_water.items[grid_offset] == WATER_N1_BLOCKED) {
        clear_data();
    } else {
        route_queue_all_from(grid_offset, DIRECTIONS_DIAGONALS, callback_calc_distance_water_flotsam, 0);
    }
}

static int callback_calc_distance_build_wall(RoutingContext *ctx, int next_offset, int dist, int direction)
{
    if (terrain_land_citizen.items[next_offset] == CITIZEN_4_CLEAR_TERRAIN) {
        enqueue(ctx, next_offset, dist);
    }
    return 1;
}

static int can_build_highway(int next_offset, int check_highway_routing)
{
    int size = 2;
    for (int x = 0; x < size; x++) {
        for (int y = 0; y < size; y++) {
            int offset = next_offset + map_grid_delta(x, y);
            int terrain = map_terrain_get(offset);
            if (terrain & TERRAIN_NOT_CLEAR & ~TERRAIN_HIGHWAY & ~TERRAIN_AQUEDUCT & ~TERRAIN_ROAD) {
                return 0;
            } else if (!map_can_place_highway_under_aqueduct(offset, check_highway_routing)) {
                return 0;
            }
        }
    }

    return 1;
}

static int callback_calc_distance_build_highway(RoutingContext *ctx, int next_offset, int dist, int direction)
{
    if (can_build_highway(next_offset, 1)) {
        enqueue(ctx, next_offset, dist);
    }
    return 1;
}

static int callback_calc_distance_build_road(RoutingContext *ctx, int next_offset, int dist, int direction)
{
    int blocked = 0;
    switch (terrain_land_citizen.items[next_offset]) {
        case CITIZEN_N3_AQUEDUCT:
            if (!map_can_place_road_under_aqueduct(next_offset)) {
                distance.determined.items[next_offset] = -1;
                blocked = 1;
            }
            break;
        case CITIZEN_2_PASSABLE_TERRAIN: // rubble, garden, access ramp
        case CITIZEN_N1_BLOCKED: // non-empty land
            blocked = 1;
            break;
        default:
            if (map_terrain_is(next_offset, TERRAIN_BUILDING)) {
                blocked = 1;
            }
            break;
    }
    if (!blocked) {
        enqueue(ctx, next_offset, dist);
    }
    return 1;
}

static int callback_calc_distance_build_aqueduct(RoutingContext *ctx, int next_offset, int dist, int direction)
{
    // check for existing highway/aqueduct tiles that won't work with this one
    if (!map_can_place_aqueduct_on_highway(next_offset, 1)) {
        return 1;
    }

    int blocked = 0;
    switch (terrain_land_citizen.items[next_offset]) {
        case CITIZEN_N3_AQUEDUCT:
        case CITIZEN_2_PASSABLE_TERRAIN: // rubble, garden, access ramp
        case CITIZEN_N1_BLOCKED: // non-empty land
            blocked = 1;
            break;
        default:
            if (map_terrain_is(next_offset, TERRAIN_BUILDING)) {
                if (terrain_land_citizen.items[next_offset] != CITIZEN_N4_RESERVOIR_CONNECTOR) {
                    blocked = 1;
                }
            }
            break;
    }
    if (map_terrain_is(next_offset, TERRAIN_ROAD) && !map_can_place_aqueduct_on_road(next_offset)) {
        distance.determined.items[next_offset] = -1;
        blocked = 1;
    }
    if (!blocked) {
        enqueue(ctx, next_offset, dist);
    }
    return 1;
}

static int map_can_place_initial_road_or_aqueduct(int grid_offset, int is_aqueduct)
{
    if (is_aqueduct && !map_can_place_aqueduct_on_highway(grid_offset, 0)) {
        return 0;
    }
    if (terrain_land_citizen.items[grid_offset] == CITIZEN_N1_BLOCKED) {
        // not open land, can only if:
        // - aqueduct should be placed, and:
        // - land is a reservoir building OR an aqueduct
        if (!is_aqueduct) {
            return 0;
        }
        if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
            return 1;
        }
        if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
            if (building_get(map_building_at(grid_offset))->type == BUILDING_RESERVOIR) {
                return 1;
            }
        }
        return 0;
    } else if (terrain_land_citizen.items[grid_offset] == CITIZEN_2_PASSABLE_TERRAIN) {
        // rubble, access ramp, garden
        return 0;
    } else if (terrain_land_citizen.items[grid_offset] == CITIZEN_N3_AQUEDUCT) {
        if (is_aqueduct) {
            return 0;
        }
        if (map_can_place_road_under_aqueduct(grid_offset)) {
            return 1;
        }
        return 0;
    } else {
        return 1;
    }
}

int map_routing_calculate_distances_for_building(routed_building_type type, int x, int y)
{
    int source_offset = map_grid_offset(x, y);
    if (type == ROUTED_BUILDING_WALL) {
        route_queue_all_from(source_offset, DIRECTIONS_NO_DIAGONALS, callback_calc_distance_build_wall, 0);
        return 1;
    }

    clear_data();

    if (type == ROUTED_BUILDING_HIGHWAY) {
        if (!can_build_highway(source_offset, 0)) {
            return 0;
        }
        route_queue_all_from(source_offset, DIRECTIONS_NO_DIAGONALS, callback_calc_distance_build_highway, 0);
        return 1;
    }

    if (!map_can_place_initial_road_or_aqueduct(source_offset, type != ROUTED_BUILDING_ROAD)) {
        return 0;
    }
    if (map_terrain_is(source_offset, TERRAIN_ROAD) &&
        type != ROUTED_BUILDING_ROAD && !map_can_place_aqueduct_on_road(source_offset)) {
        return 0;
    }
    ++stats.total_routes_calculated;
    if (type == ROUTED_BUILDING_ROAD) {
        route_queue_all_from(source_offset, DIRECTIONS_NO_DIAGONALS, callback_calc_distance_build_road, 0);
    } else {
        route_queue_all_from(source_offset, DIRECTIONS_NO_DIAGONALS, callback_calc_distance_build_aqueduct, 0);
    }
    return 1;
}

static int callback_delete_wall_aqueduct(RoutingContext *ctx, int next_offset, int dist, int direction)
{
    if (terrain_land_citizen.items[next_offset] < CITIZEN_0_ROAD) {
        if (map_terrain_is(next_offset, TERRAIN_AQUEDUCT | TERRAIN_WALL)) {
            map_terrain_remove(next_offset, TERRAIN_CLEARABLE);
            return UNTIL_STOP;
        }
    } else {
        enqueue(ctx, next_offset, dist);
    }
    return UNTIL_CONTINUE;
}

void map_routing_delete_first_wall_or_aqueduct(int x, int y)
{
    ++stats.total_routes_calculated;
    route_queue_all_from(map_grid_offset(x, y), DIRECTIONS_NO_DIAGONALS, callback_delete_wall_aqueduct, 0);
}

static int is_fighting_friendly(figure *f)
{
    return f->is_friendly && f->action_state == FIGURE_ACTION_150_ATTACK;
}

static inline int has_fighting_friendly(int grid_offset)
{
    if (!(fighting_data.status.items[grid_offset] & 0x80)) {
        fighting_data.status.items[grid_offset] |= 0x80 | map_figure_foreach_until(grid_offset, is_fighting_friendly);
    }
    return fighting_data.status.items[grid_offset] & 1;
}

static int is_fighting_enemy(figure *f)
{
    return !f->is_friendly && f->action_state == FIGURE_ACTION_150_ATTACK;
}

static inline int has_fighting_enemy(int grid_offset)
{
    if (!(fighting_data.status.items[grid_offset] & 0x40)) {
        fighting_data.status.items[grid_offset] |= 0x40 | (map_figure_foreach_until(grid_offset, is_fighting_enemy) << 1);
    }
    return fighting_data.status.items[grid_offset] & 2;
}

static int callback_travel_citizen_land(int offset, int next_offset, int direction)
{
    if (terrain_land_citizen.items[next_offset] >= 0 && !has_fighting_friendly(next_offset)) {
        return 1;
    }
    return 0;
}

int map_routing_citizen_can_travel_over_land(int src_x, int src_y, int dst_x, int dst_y, int num_directions)
{
    ++stats.total_routes_calculated;
    route_queue_from_to(src_x, src_y, dst_x, dst_y, num_directions, 0, callback_travel_citizen_land);
    return distance.determined.items[map_grid_offset(dst_x, dst_y)] != 0;
}

static int callback_travel_citizen_road_garden(int offset, int next_offset, int direction)
{
    if (terrain_land_citizen.items[next_offset] == CITIZEN_0_ROAD ||
        terrain_land_citizen.items[next_offset] == CITIZEN_2_PASSABLE_TERRAIN) {
        return 1;
    }
    return 0;
}

int map_routing_citizen_can_travel_over_road_garden(int src_x, int src_y, int dst_x, int dst_y, int num_directions)
{
    int dst_offset = map_grid_offset(dst_x, dst_y);
    if (terrain_land_citizen.items[dst_offset] != CITIZEN_0_ROAD &&
        terrain_land_citizen.items[dst_offset] != CITIZEN_2_PASSABLE_TERRAIN) {
        return 0;
    }
    ++stats.total_routes_calculated;
    route_queue_from_to(src_x, src_y, dst_x, dst_y, num_directions, 0, callback_travel_citizen_road_garden);
    return distance.determined.items[dst_offset] != 0;
}

static int callback_travel_citizen_road_garden_highway(int offset, int next_offset, int direction)
{
    if (terrain_land_citizen.items[next_offset] >= CITIZEN_0_ROAD &&
        terrain_land_citizen.items[next_offset] <= CITIZEN_2_PASSABLE_TERRAIN) {
        return 1;
    }
    return 0;
}

int map_routing_citizen_can_travel_over_road_garden_highway(int src_x, int src_y, int dst_x, int dst_y, int num_directions)
{
    int dst_offset = map_grid_offset(dst_x, dst_y);
    if (terrain_land_citizen.items[dst_offset] < CITIZEN_0_ROAD ||
        terrain_land_citizen.items[dst_offset] > CITIZEN_2_PASSABLE_TERRAIN) {
        return 0;
    }
    ++stats.total_routes_calculated;
    route_queue_from_to(src_x, src_y, dst_x, dst_y, num_directions, 0, callback_travel_citizen_road_garden_highway);
    return distance.determined.items[dst_offset] != 0;
}

static int callback_travel_walls(int offset, int next_offset, int direction)
{
    if (terrain_walls.items[next_offset] >= WALL_0_PASSABLE &&
        terrain_walls.items[next_offset] <= 2) {
        return 1;
    }
    return 0;
}

int map_routing_can_travel_over_walls(int src_x, int src_y, int dst_x, int dst_y, int num_directions)
{
    ++stats.total_routes_calculated;
    route_queue_from_to(src_x, src_y, dst_x, dst_y, num_directions, 0, callback_travel_walls);
    return distance.determined.items[map_grid_offset(dst_x, dst_y)] != 0;
}

static int callback_travel_noncitizen_land_through_building(int offset, int next_offset, int direction)
{
    if (has_fighting_enemy(next_offset)) {
        return 0;
    }
    int8_t terrain = terrain_land_noncitizen.items[next_offset];
    if (terrain == NONCITIZEN_0_PASSABLE || terrain == NONCITIZEN_2_CLEARABLE) {
        return 1;
    }
    int map_building_id = map_building_at(next_offset);
    if (terrain == NONCITIZEN_1_BUILDING && (map_building_id == state.through_building_id || map_building_id == state.dest_building_id)) {
        return 1;
    }
    return 0;
}

static int callback_travel_noncitizen_land(int offset, int next_offset, int direction)
{
    if (has_fighting_enemy(next_offset)) {
        return 0;
    }
    uint8_t terrain = terrain_land_noncitizen.items[next_offset];
    if (terrain < NONCITIZEN_5_FORT) {
        return 1;
    }
    return 0;
}

int map_routing_noncitizen_can_travel_over_land(
    int src_x, int src_y, int dst_x, int dst_y, int num_directions, int only_through_building_id, int max_tiles
)
{
    ++stats.total_routes_calculated;
    ++stats.enemy_routes_calculated;
    if (only_through_building_id) {
        state.through_building_id = only_through_building_id;
        // due to formation offsets, the destination building may not be the same as the "through building" (a.k.a. target building)
        state.dest_building_id = map_building_at(map_grid_offset(dst_x, dst_y));
        route_queue_from_to(src_x, src_y, dst_x, dst_y, num_directions, 0, callback_travel_noncitizen_land_through_building);
    } else {
        route_queue_from_to(src_x, src_y, dst_x, dst_y, num_directions, max_tiles, callback_travel_noncitizen_land);
    }
    return distance.determined.items[map_grid_offset(dst_x, dst_y)] != 0;
}

static int callback_travel_noncitizen_through_everything(int offset, int next_offset, int direction)
{
    if (terrain_land_noncitizen.items[next_offset] >= NONCITIZEN_0_PASSABLE) {
        return 1;
    }
    return 0;
}

int map_routing_noncitizen_can_travel_through_everything(int src_x, int src_y, int dst_x, int dst_y, int num_directions)
{
    ++stats.total_routes_calculated;
    route_queue_from_to(src_x, src_y, dst_x, dst_y, num_directions, 0, callback_travel_noncitizen_through_everything);
    return distance.determined.items[map_grid_offset(dst_x, dst_y)] != 0;
}

void map_routing_block(int x, int y, int size)
{
    if (!map_grid_is_inside(x, y, size)) {
        return;
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            distance.determined.items[map_grid_offset(x + dx, y + dy)] = 0;
        }
    }
}

int map_routing_distance(int grid_offset)
{
    return distance.determined.items[grid_offset];
}

void map_routing_save_state(buffer *buf)
{
    buffer_write_i32(buf, 0); // unused counter
    buffer_write_i32(buf, stats.enemy_routes_calculated);
    buffer_write_i32(buf, stats.total_routes_calculated);
    buffer_write_i32(buf, 0); // unused counter
}

void map_routing_load_state(buffer *buf)
{
    buffer_skip(buf, 4); // unused counter
    stats.enemy_routes_calculated = buffer_read_i32(buf);
    stats.total_routes_calculated = buffer_read_i32(buf);
    buffer_skip(buf, 4); // unused counter
}
