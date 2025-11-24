// Internal
#include "routing.h"
// External
#include "building/building.h"
#include "building/construction_building.h"
#include "core/direction.h"
#include "core/time.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/road_aqueduct.h"
#include "map/routing_data.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include <stdlib.h>
#include <string.h> // Required for memset





// --- ROUTE OFFSETS ---
// This can be easily improved to allow for custom grid sizes, but I have no idea how loading and scenario files work
static int ROUTE_OFFSETS[10];
static const int DIRECTION_X_OFFSETS[10] = {
    0, // [0] DIR_NULL
    -1, // [1] DIR_TOP_LEFT
    0, // [2] DIR_TOP
    +1, // [3] DIR_TOP_RIGHT
    -1, // [4] DIR_LEFT
    0, // [5] DIR_CENTER
    +1, // [6] DIR_RIGHT
    -1, // [7] DIR_BOTTOM_LEFT
    0, // [8] DIR_BOTTOM
    +1, // [9] DIR_BOTTOM_RIGHT
};
static const int DIRECTION_Y_OFFSETS[10] = {
    0, // [0] DIR_NULL
    1, // [1] DIR_TOP_LEFT
    1, // [2] DIR_TOP
    1, // [3] DIR_TOP_RIGHT
    0, // [4] DIR_LEFT
    0, // [5] DIR_CENTER
    0, // [6] DIR_RIGHT
    -1, // [7] DIR_BOTTOM_LEFT
    -1, // [8] DIR_BOTTOM
    -1 // [9] DIR_BOTTOM_RIGHT
};
int grid_size = 162 // default for vanilla maps
void map_routing_init_offsets(int grid_width)
{
    // We only need to calculate the offsets for the 8 movement directions (1 to 4, and 6 to 9)
    for (int i = 1; i <= 9; i++) {
        // Skip DIR_CENTER, as it's not a movement direction
        if (i == DIR_CENTER) continue;
        // The core calculation now uses the new DIRECTION_X/Y_OFFSETS
        ROUTE_OFFSETS[i] = DIRECTION_X_OFFSETS[i] + (DIRECTION_Y_OFFSETS[i] * grid_width);
    }
}

// --- GLOBAL STATS AND CACHE ---
// Global stats
static struct {
    int total_routes_calculated;
    int enemy_routes_calculated;
} stats;

// Fighting Data Grid
static struct {
    grid_u8 status; // 
    time_millis last_check;
} fighting_data;

// Defines routing rules
typedef struct {
    int cardinal_travel = 0; // forces travelling in only four directions
    // Entity
    int entity_type = ENTITY_TYPE_CITIZEN;
    int is_friendly = 0;
    int is_enemy = 0;
    int boost_highway = 0; // determines if the entity moves faster on highways
    // Citizen permissions
    int travel_roads = 0;
    int travel_highways = 0;
    int travel_passable = 0;
    int travel_land = 0;
    int travel_blocked = 0;
    int travel_aqueduct = 0;
    int travel_reservoir_connector = 0;
    int travel_walls = 0;
    // Buildings (drag build)
    int is_road = 0;
    int is_highway = 0;
    int is_aqueduct = 0;
    int is_wall = 0;
    // Misc
    int unblocking_rome = 0;
    int target_building_id;  // target building to attack
    int through_building_id; // building allowed to pass through
} TravelRules;

// Defines route configuration (indexed by the RouteType enum.)
static const TravelRules ROUTE_RULE_CONFIG[] = {
    // Moving entities
    [ROUTE_TYPE_CITIZEN] = {
        .entity_type = ENTITY_TYPE_CITIZEN,
        .travel_roads = 1,
        .travel_passable = 1,
    },
    [ROUTE_TYPE_TOWER_SENTRY] = {
        .entity_type = ENTITY_TYPE_CITIZEN,
        .travel_roads = 1,
        .travel_passable = 1,
        .travel_walls = 1,
    },
    [ROUTE_TYPE_FRIENDLY] = {
        .entity_type = ENTITY_TYPE_NONCITIZEN,
        .is_friendly
    },
    [ROUTE_TYPE_ENEMY] = {
        .entity_type = ENTITY_TYPE_NONCITIZEN,
        .is_enemy = 1,
    },
    [ROUTE_TYPE_BOAT] = {
        .entity_type = ENTITY_TYPE_BOAT,
        .is_boat = 1,
    },
    [ROUTE_TYPE_FLOTSAM] = {
        .entity_type = ENTITY_TYPE_FLOTSAM,
        .is_flotsam = 1,
    },
    // Buildings (drag build)
    [ROUTE_TYPE_ROAD] = {
        .entity_type = ENTITY_TYPE_ROAD,
        .is_road = 1,
    },
    [ROUTE_TYPE_HIGHWAY] = {
        .entity_type = ENTITY_TYPE_HIGHWAY,
        .is_highway = 1,
    },
    [ROUTE_TYPE_AQUEDUCT] = {
        .entity_type = ENTITY_TYPE_AQUEDUCT,
        .is_aqueduct = 1,
    },
    [ROUTE_TYPE_WALL] = {
        .entity_type = ENTITY_TYPE_WALL,
        .is_wall = 1,
    },
};

// Populates the fighting data cache inside the routing context
// This cache allows the pathfinder to quickly check if a tile is under attack. 
static void populate_fighting_data(RoutingContext *ctx)
{
    // 1. Clear the grid first (reset all tiles to 0)
    // Assuming 'fighting_data' inside ctx is a full grid array
    map_grid_clear_u8(ctx->fighting_data.status.items);

    // 2. Iterate over all figures
    // You need a way to loop all figures. Assuming 'figure_foreach' exists:
    int total_figures = figure_count();
    for (int i = 1; i < total_figures; i++) {
        // Check if the figure is alive
        figure *f = figure_get(i);
        if (f == NULL || f->state != FIGURE_STATE_ALIVE) continue;

        // Check if this figure is fighting
        int is_fighting = (f->action_state == FIGURE_ACTION_150_ATTACK);
        if (!is_fighting) continue;

        // Mark the tile
        int grid_offset = f->grid_offset;

        if (f->is_friendly) {
            // Set Bit 0 for Friendly Fighters
            ctx->fighting_data.items[grid_offset] |= 1;
        } else {
            // Set Bit 1 for Enemy Fighters
            ctx->fighting_data.items[grid_offset] |= 2;
        }
    }
}

// Reset the external global cache 'fighting_data' if a new frame/tick has started
static void reset_fighting_status(void)
{
    time_millis current_time = time_get_millis();
    if (current_time != fighting_data.last_check) {
        map_grid_clear_u8(fighting_data.status.items);
        fighting_data.last_check = current_time;
    }
}





// Global Routing Context
RoutingContext g_context;
// Get path id
iint get_path_id(void)
{
    RoutingContext *ctx = map_routing_get_context();

    // Iterate through all 32 possible map slots
    for (int i = 0; i < MAX_CONCURRENT_ROUTES; i++) {
        if (ctx->route_map_pool_is_reserved[i] == 0) {
            // Found a free map ID! Reserve it and return the ID.
            ctx->route_map_pool_is_reserved[i] = 1;
            return i;
        }
    }

    // No free path found, MAX_CONCURRENT_ROUTES is too low or something is clogging them
    return -1;
}
// Release path id
void release_path_id(int path_id)
{
    if (path_id >= 0 && path_id < MAX_CONCURRENT_ROUTES) {
        RoutingContext *ctx = map_routing_get_context();
        ctx->route_map_pool_is_reserved[path_id] = 0;
    }
}
// Clears the routing maps and resets the queue pointers
static void clear_data(RoutingContext *ctx, map_routing_distance_grid *dm) // dm = distance map
{
    reset_fighting_status();
    // Clears the specified map in the pool (dm)
    map_grid_clear_i16(dm->possible.items);
    map_grid_clear_i16(dm->determined.items);
    // Queue is context-specific, so it stays on ctx
    ctx->queue.head = 0;
    ctx->queue.tail = 0;
}



// Enqueues an item for a simple (non-ordered) queue (e.g., Breadth-First Search)
static void enqueue(RoutingContext *ctx, map_routing_distance_grid *dm, int next_offset, int dist)
{
    // 1. Update the distance grid via the context
    dm->determined.items[next_offset] = dist;

    // 2. Add item to the queue via the context
    ctx->queue.items[ctx->queue.tail++] = next_offset;

    // 3. Ring Buffer Wrap-around (Must check the CONTEXT'S tail)
    if (ctx->queue.tail >= MAX_QUEUE) {
        ctx->queue.tail = 0;
    }
}

// Pops an item for a simple (non-ordered) queue
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

// --- PRIORITY QUEUE (HEAP) HELPERS ---
// Gets the parent in the priority queue
static inline int get_ordered_queue_parent(int index)
{
    return (index - 1) / 2;
}
static inline void ordered_queue_swap(RoutingContext *ctx, int first, int second)
{
    int temp_offset = ctx->queue.items[first];

    // 1. Swap the offsets in the main queue array
    ctx->queue.items[first] = ctx->queue.items[second];
    ctx->queue.items[second] = temp_offset;

    // 2. Update the lookup table (node_index) with the NEW positions
    // This allows O(1) lookups to find an item's position in the queue
    ctx->node_index[ctx->queue.items[first]] = first;
    ctx->node_index[ctx->queue.items[second]] = second;
}

// Maintains the Min-Heap property (sifts down the value at start_index)
static void ordered_queue_reorder(RoutingContext *ctx, map_routing_distance_grid *dm, int start_index)
{
    int left_child = 2 * start_index + 1;
    if (left_child >= ctx->queue.tail) {
        return;
    }
    int right_child = left_child + 1;
    int smallest = start_index;

    int smallest_dist = dm->possible.items[ctx->queue.items[smallest]];

    // Compare with Left Child
    int left_dist = dm->possible.items[ctx->queue.items[left_child]];
    if (left_dist < smallest_dist) {
        smallest = left_child;
        smallest_dist = left_dist; // Update the reference value
    }
    // Compare with Right Child
    if (right_child < ctx->queue.tail) {
        int right_dist = dm->possible.items[ctx->queue.items[right_child]];
        if (right_dist < smallest_dist) {
            smallest = right_child;
        }
    }

    if (smallest != start_index) {
        ordered_queue_swap(ctx, start_index, smallest);
        ordered_queue_reorder(ctx, smallest);
    }
}

// Pops the smallest element (the next node to visit) from the Min-Heap
static inline int ordered_queue_pop(RoutingContext *ctx)
{
    // 1. Store the smallest element (at index 0)
    int min_item_offset = ctx->queue.items[0];

    // 2. Decrement the tail pointer (queue size)
    ctx->queue.tail--;

    // 3. Move the item from the now-last position to the head (index 0)
    ctx->queue.items[0] = ctx->queue.items[ctx->queue.tail];
    // 4. Update the lookup table for the new head position
    ctx->node_index[ctx->queue.items[0]] = 0;

    // 5. Reorder the heap to fix the heap property
    ordered_queue_reorder(ctx, 0);

    return min_item_offset;
}

// Reduces the key (distance) of an existing node and bubbles it up the heap (used in Dijkstra/A*)
static inline void ordered_queue_reduce_index(RoutingContext *ctx, map_routing_distance_grid *dm, int index, int offset, int dist)
{
    // The given 'offset' is already placed at 'index' in the queue by ordered_enqueue.
    // The parameter 'dist' is the new possible distance.

    // While not at the root (index > 0) AND new distance is smaller than parent's distance
    while (index > 0 && dm->possible.items[ctx->queue.items[get_ordered_queue_parent(index)]] > dist) {
        // FIX: The original was missing the 'ctx' argument.
        ordered_queue_swap(ctx, index, get_ordered_queue_parent(index));
        index = get_ordered_queue_parent(index);
    }
}

// Enqueues a new item into the Priority Queue (Min-Heap)
// Signature updated to accept map pointer 'dm'
static void ordered_enqueue(RoutingContext *ctx, map_routing_distance_grid *dm, int next_offset, int current_dist, int remaining_dist)
{
    int possible_dist = remaining_dist + current_dist;
    int index;

    if (dm->possible.items[next_offset]) {
        if (dm->possible.items[next_offset] <= possible_dist) {
            return;
        }
        index = ctx->node_index[next_offset];
    } else {
        index = ctx->queue.tail;
        ctx->queue.tail++;
        ctx->node_index[next_offset] = index;
    }

    // Write to dm->determined and dm->possible
    dm->determined.items[next_offset] = current_dist;
    dm->possible.items[next_offset] = possible_dist;

    // NOTE: ordered_queue_reduce_index must also be updated to accept 'dm'
    ordered_queue_reduce_index(ctx, dm, index, next_offset, possible_dist);
}

// Checks if an offset is valid AND if the path to it hasn't been determined yet, or the new path is better.
static inline int valid_offset(int grid_offset, map_routing_distance_grid *dm, int possible_dist)
{
    int determined = dm->determined.items[grid_offset];
    // FIX: Should compare against 'dm->possible' for A* (or just determined for Dijkstra)
    // Assuming 'dm->determined' stores the G-cost (path found so far)
    return map_grid_is_valid_offset(grid_offset) && (determined == 0 || possible_dist < determined);
}

// Manhattan distance (Heuristic for A*)
static inline int distance_left(int x, int y)
{
    return abs(dm->dst_x - x) + abs(dm->dst_y - y);
}

// Declaration of the master travel function
static int callback_travel_master(RoutingContext *ctx, int offset, int next_offset,
 int direction_index, const TravelRules *rules, int *out_cost);

// Finds the best route from A to B (A* Search)
static void route_queue_from_to(RoutingContext *ctx, map_routing_distance_grid *dm, int src_x, int src_y, int dst_x, int dst_y, const TravelRules *rules)
{
    // 1. Setup Phase (Clear queues, initialize distances, enqueue source)
    // FIX: MAX_SEARCH_ITERATIONS must be defined externally (or use the #define from routing.h)
    int guard_count = 0;
    int destination_offset = map_grid_offset(dst_x, dst_y);
    int source_offset = map_grid_offset(src_x, src_y);

    // FIX: Assign target position to global distance struct for heuristic calculation
    dm->dst_x = dst_x;
    dm->dst_y = dst_y;

    // Initial enqueue using A* heuristic
    ordered_enqueue(ctx, source_offset, 0, distance_left(src_x, src_y));

    // 2. Search Loop (While queue is not empty and guard count is not exceeded)
    while (ctx->queue.tail > 0 && guard_count++ < MAX_SEARCH_ITERATIONS) {

        // Pop the current best node (offset)
        int current_offset = ordered_queue_pop(ctx);
        int current_distance = dm->determined.items[current_offset];

        // Early exit if destination found
        if (current_offset == destination_offset) {
            return;
        }

        // 3. Iterate Neighbors
        for (int i = 0; i < num_directions; i++) {// replace num_directions by cardinal_travel however appropriate

            int next_offset = current_offset + ROUTE_OFFSETS[i];

            // Assuming map width is a constant like GRID_SIZE, not MAP_WIDTH
            int next_x = (current_offset % GRID_SIZE) + ROUTE_OFFSETS_X[i];
            int next_y = (current_offset / GRID_SIZE) + ROUTE_OFFSETS_Y[i];

            // Boundary Check (using a border size of 1 for safety)
            if (!map_grid_is_inside(next_x, next_y, 1)) {
                continue;
            }

            // --- The Refactored Logic: Use the master function ---
            int travel_cost = 0;
            // FIX: Remove redundant parameter and pass the pointer to the cost variable
            if (callback_travel_master(ctx, current_offset, next_offset, i, rules, &travel_cost)) {

                // If the master function returns success (1), calculate new cost
                int new_dist = current_distance + travel_cost;

                // Manhattan Heuristic (H) for A*
                int remaining_dist = distance_left(next_x, next_y);

                // If path is better, enqueue/update it
                // FIX: Use 'ordered_enqueue' which handles the path update
                ordered_enqueue(ctx, next_offset, new_dist, remaining_dist);
            }
        }
    }
}

// Finds the best route from A to ALL (Dijkstra's Search)
// Slow but guarantees best route by removing the remaining_dist heuristic
void route_queue_all_from(RoutingContext *ctx, map_routing_distance_grid *dm, int source_offset, const TravelRules *rules)
{
    int tiles_processed = 0;
    int num_directions = rules->num_directions ? 4 : 8;

    // Initial enqueue: Pass the map pointer 'dm'
    ordered_enqueue(ctx, dm, source_offset, 0, 0);

    while (ctx->queue.tail > 0) {

        // Guards against excessively long searches
        if (++tiles_processed > MAX_SEARCH_ITERATIONS) {
            break;
        }

        // Pop the next best tile from the priority queue
        int current_offset = ordered_queue_pop(ctx);
        // Get current path cost from the determined distance map
        int current_distance = dm->determined.items[current_offset];

        // Get the allowed directions to use (cardinal or all)
        const int *directions_to_use = rules->cardinal_travel ? CARDINAL_DIRECTIONS : MOVEMENT_DIRECTIONS;
        const int num_directions = rules->cardinal_travel ? 4 : 8;

        // Iterate through all neighbors
        for (int i = 0; i < num_directions; i++) {
            // Get the direction enum value (e.g., 2 for DIR_TOP)
            const int direction_enum = directions_to_use[i];

            // Calculate neighbor tile offset
            int next_offset = current_offset + ROUTE_OFFSETS[direction_enum];

            // Determine Cost and Passability using the Master function
            int travel_cost = 0;
            // callback_travel_master does not read/write map data, so its signature is fine.
            if (callback_travel_master(ctx, current_offset, next_offset, direction_enum, rules, &travel_cost)) {
                // Calculate new path distance
                int new_dist = current_distance + travel_cost;

                // Standard Dijkstra Check (ie is this a better path?)
                // If new_dist is better, it will be added/updated in the priority queue (heuristic is 0)
                ordered_enqueue(ctx, dm, next_offset, new_dist, 0);
            }
        }
    }
}




// ???
static inline int valid_offset(map_routing_distance_grid *dm, int grid_offset, int possible_dist)
{
    // Use dm-> determined map for the check.
    int determined = dm->determined.items[grid_offset];
    return map_grid_is_valid_offset(grid_offset) && (determined == 0 || possible_dist < determined);
}





// --- PASSABILITY AND COST HELPER FUNCTIONS ---
// Returns the cost of a citizen passing through a tile, returns -1 if impassable
static inline int get_citizen_base_cost(int next_offset, const TravelRules *rules)
{
    int base_cost = 100; // Default base cost

    // Common Terrain Access Checks
    switch (terrain_access_citizen.items[next_offset]) {
        case TERRAIN_ACCESS_CITIZEN_ROAD:
            if (!rules->travel_roads) return -1;
            break;
        case TERRAIN_ACCESS_CITIZEN_HIGHWAY:
            if (!rules->travel_highways) return -1;
            break;
        case TERRAIN_ACCESS_CITIZEN_PASSABLE: // Gardens, rubble, access ramps
            if (!rules->travel_passable) return -1;
            break;
        case TERRAIN_ACCESS_CITIZEN_EMPTY: // Open land
            if (!rules->travel_land) return -1;
            break;
        case TERRAIN_ACCESS_CITIZEN_BLOCKED: // Completely impassable terrain
            if (!rules->travel_blocked) return -1;
            return -1; // Should always return -1 regardless of flag
        case TERRAIN_ACCESS_CITIZEN_AQUEDUCT: // Top of an aqueduct
            if (!rules->travel_aqueduct) return -1;
            break;
        case TERRAIN_ACCESS_CITIZEN_RESERVOIR_CONNECTOR: // Reservoir structure
            if (!rules->travel_reservoir_connector) return -1;
            break;
        default:
            // If it's a building and none of the above, assume blocked
            if (map_terrain_is(next_offset, TERRAIN_BUILDING)) {
                return -1;
            }
    }

    // Walls Check
    switch (terrain_wall.items[next_offset]) {
        case TERRAIN_ACCESS_WALL_PASSABLE: // Walkable wall (like a gatehouse top)
            if (!rules->travel_walls) {
                return -1; // Only sentries/specific units allowed
            }
            break;

        case TERRAIN_ACCESS_WALL_BLOCKED: // Impassable wall
            return -1;
    }

    // FIX: You had duplicated BASE_TERRAIN_COST calls. Use the base cost of 100 or a formula.
    // Assuming the base cost for movement is a fixed value (e.g., 100 for a tile step)
    // You can adjust this based on the terrain type if needed.
    return base_cost;
}
// Returns the cost of a noncitizen passing through a tile, returns -1 if impassable
static inline int get_noncitizen_base_cost(RoutingContext *ctx, int next_offset)
{
    // Friendly noncitizens may pass through all terrain except fort and blocked
    switch (terrain_access_noncitizen.items[next_offset]) {
        case TERRAIN_ACCESS_NONCITIZEN_PASSABLE:
        case TERRAIN_ACCESS_NONCITIZEN_CLEARABLE: // Assuming this is defined
        case TERRAIN_ACCESS_GATEHOUSE:
            break;
        case TERRAIN_ACCESS_BUILDING:
            // Friendlies can pass through fine
            if (rules->is_friendly) { break; }
            // Enemies must be able to pass attack their target building
            int map_building_id = map_building_at(next_offset);
            if (map_building_id != ctx->target_building_id) {
                return -1;
            }
            break;
        case TERRAIN_ACCESS_FORT:
            if (rules->is_friendly) { return -1; } // Friendlies can't travel through forts
            break;
        case TERRAIN_ACCESS_BLOCKED:
            return -1; // May never pass through impassable terrain
    }
    // Base Cost
    base_cost = BASE_TERRAIN_COST(terrain_access_noncitizen.items[next_offset]);
    return base_cost
}
// Returns the cost of a boat passing through a tile, returns -1 if impassable
static inline int get_boat_base_cost(int next_offset)
{
    // Next position must be water AND must not be blocked (low bridge or explicit block).
    if (!map_terrain_is_water(next_offset) ||
        terrain_access.items[next_offset] == TERRAIN_ACCESS_WATER_BLOCKED ||
        terrain_access.items[next_offset] == TERRAIN_ACCESS_WATER_LOW_BRIDGE) {
        return -1;
    }
    // Small boats can be easily added here as an extra condition
    // Base Cost
    base_cost = BASE_TERRAIN_COST(terrain_access_water.items[next_offset]);
}
// Flotsam
static inline int get_flotsam_base_cost(int next_offset)
{
    // Next position must be water and not blocked
    if (map_terrain_is_water(next_offset) && terrain_access_water.items[next_offset] != = TERRAIN_ACCESS_WATER_BLOCKED) {
        return 1;
    }
    return 100; // flotsam ignores terrain costs
}
// --- BUILDING BASE COST --- (building routing exists for drag build)
// Road
static inline int get_road_base_cost(map_routing_distance_grid *dm, int next_offset)
{
    switch (terrain_access_citizen.items[next_offset]) {
        case TERRAIN_ACCESS_CITIZEN_AQUEDUCT:
            if (!map_can_place_road_under_aqueduct(next_offset)) {
                dm->determined.items[next_offset] = -1;
                return -1;
            }
        case TERRAIN_ACCESS_CITIZEN_EMPTY:
            return 100; // drag build ignores terrain costs
    }
    return -1;
}
// Highway
static inline int get_highway_base_cost(int next_offset)
{
    // Next position must be a valid placement for a highway
    int size = 2;
    for (int x = 0; x < size; x++) {
        for (int y = 0; y < size; y++) {
            int offset = next_offset + map_grid_delta(x, y);
            int terrain = map_terrain_get(offset);
            if (terrain & TERRAIN_NOT_CLEAR & ~TERRAIN_HIGHWAY & ~TERRAIN_AQUEDUCT & ~TERRAIN_ROAD) {
                return -1;
            } else if (!map_can_place_highway_under_aqueduct(offset, check_highway_routing)) {
                return -1;
            }
        }
    }
    return 100; // drag build ignores terrain costs
}
// Aqueduct
static inline int get_aqueduct_base_cost(map_routing_distance_grid *dm, int next_offset)
{
    // If the aqueduct cannot be placed on this specific road tile, return
    if (map_terrain_is(next_offset, TERRAIN_ROAD) && !map_can_place_aqueduct_on_road(next_offset)) {
        dm->determined.items[next_offset] = -1;
        return -1;
    }
    // If the aqueduct cannot be placed on this specific highway tile, return
    if (map_terrain_is(next_offset, TERRAIN_HIGHWAY) && !map_can_place_aqueduct_on_highway(next_offset, 1)) {
        return -1;
    }
    // Check for other invalid terrain
    switch (terrain_access_citizen.items[next_offset]) {
        case TERRAIN_ACCESS_CITIZEN_AQUEDUCT:
        case TERRAIN_ACCESS_CITIZEN_PASSABLE: // rubble, garden, access ramp
        case TERRAIN_ACCESS_CITIZEN_BLOCKED: // non-empty land
            return -1;
    }
    // Check if the terrain is a building, in which case only reservoir connectors are allowed
    if (map_terrain_is(next_offset, TERRAIN_BUILDING)) {
        if (terrain_access_citizen.items[next_offset] != TERRAIN_ACCESS_CITIZEN_RESERVOIR_CONNECTOR) {
            return -1;
        }
    }
    return 100; // drag build ignores terrain costs
}
// Wall
static inline int get_wall_base_cost(int next_offset)
{
    // Next position must be clear terrain
    if (terrain_access_wall.items[next_offset] != TERRAIN_ACCESS_WALL_EMPTY) {
        return -1;
    }
    return 100; // drag build ignores terrain costs
}


/** @brief Helper function to calculate the travel cost and check permission
* Returns 1 if travel is permitted, 0 otherwise
* @param ctx
* @param current_offset
* @param next_offset
* @param direction_index
* @param rules
* @param out_cost - Calculated cost is stored here
**/
static int callback_travel_master(RoutingContext *ctx, int current_offset, int next_offset,
int direction_index, const TravelRules *rules, int *out_cost)
{
    // Defaults
    int base_cost = 0;
    int cost_multiplier = 100; // Better than using float
    // Incorrect, instead this must detect if the direction is cardinal or not then apply
    int direction_cost_multiplier = is_cardinal(direction_index) ? 100 : 141;

    // --- IMPASSABILITY CHECKS ---

    // If impassable, return
    if (!rules->terrain_passable_func(next_offset)) {
        return 0;
    }

    // If there's anyone fighting in the tile, return
    // Bit 0 = Friendly Fighting, Bit 1 = Enemy Fighting
    uint8_t fight_status = ctx->fighting_data.status.items[next_offset];
    if (fight_status & 1) { /* Avoid Friendly Fight */ return 0; }
    if (fight_status & 2) { /* Avoid Enemy Fight */ return 0; }

    // --- RULES-BASED COST/PASSABILITY ---
    // Get the base cost
    switch (rules->entity_type) {
        case ENTITY_CITIZEN:
            base_cost = get_citizen_base_cost(next_offset, rules);
            break;
        case ENTITY_NONCITIZEN: // traders, enemies, etc.
            base_cost = get_noncitizen_base_cost(next_offset);
            break;
        case ENTITY_BOAT:
            base_cost = get_boat_base_cost(next_offset);
            break;
        case ENTITY_FLOTSAM:
            base_cost = get_flotsam_base_cost(next_offset)
                break;
        case ENTITY_ROAD:
            base_cost = get_road_base_cost(next_offset)
        case ENTITY_HIGHWAY:
            base_cost = get_highway_base_cost(next_offset)
        case ENTITY_AQUEDUCT:
            base_cost = get_aqueduct_base_cost(next_offset)
        case ENTITY_WALL:
            base_cost = get_wall_base_cost(next_offset)
    }

    // Return if blocked
    if (base_cost == -1) { return 0 };
    // Speed Modifiers (e.g highways)
    if (rules->boost_highway && map_terrain_is_highway(next_offset)) {
        if is_cardinal_direction(direction) // cannot move fast diagonally on highways
        {
            cost_multiplier = 50;
        }
    }
    // Calculate cost
    *out_cost = base_cost * direction_cost_multiplier * cost_multiplier;
    return 1;

    // --- SPECIAL ---
    // These routes cause side-effects, such as destroying buildings
    // Unblock Path To Rome (delete walls/aqueducts blocking the path to rome)
    if (rules->unblocking_rome) {
        // Check if the current tile contains a blockage (wall/aqueduct)
        if (map_terrain_is(next_offset, TERRAIN_AQUEDUCT | TERRAIN_WALL)) {
            // ACTION: Remove the blockage (side effect)
            map_terrain_remove(next_offset, TERRAIN_CLEARABLE);
        }
        *out_cost = 100; // Just to be safe
        return 1;
    }

    // Unsupported rule type
    return 0;
}






// Internal helper function that handles all routing types
// Signature updated to accept path_id
void map_routing_calculate_distances(int path_id, int x, int y, RouteType type)
{
    // 1. Safety check (ensures only paths 0 to 31 are accessed)
    if (path_id < 0 || path_id >= MAX_CONCURRENT_ROUTES) return;

    // 2. Load Rules
    const TravelRules *rules = &ROUTE_RULE_CONFIG[type];

    // 3. Get Context and Map Pointer
    RoutingContext *ctx = map_routing_get_context();
    // CRITICAL: Get a pointer to the specific map slot in the pool
    map_routing_distance_grid *dist_map = &ctx->route_map_pool[path_id];

    // 4. Clear Data (Queue and the specific map)
    clear_data(ctx, dist_map);

    // 5. Populate Fighting Data (cache)
    populate_fighting_data(ctx);

    // 6. Start Search Stats
    ++stats.total_routes_calculated;
    if (rules->is_enemy) {
        ++stats.enemy_routes_calculated;
    }

    // 7. Assign Target Coordinates (if this were an A* search)
    // Even for Dijkstra, setting these can be useful for debugging or later use.
    dist_map->dst_x = x;
    dist_map->dst_y = y;

    // 8. Call Generic Core Routing Function, passing the specific map pointer
    // Signature of route_queue_all_from must be updated!
    route_queue_all_from(ctx, dist_map, map_grid_offset(x, y), rules);
}












// Marks an area as the start for the search (for multi-source pathfinding)
void map_routing_add_source_area(map_routing_distance_grid *dm, int x, int y, int size)
{
    if (!map_grid_is_inside(x, y, size)) {
        return;
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            dm->determined.items[map_grid_offset(x + dx, y + dy)] = 0;
        }
    }
}

// Save/Load Buffers
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