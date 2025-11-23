// Internal
#include "routing.h"
// External
#include "building/building.h"
#include "building/construction_building.h"
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

// --- CONSTANTS AND OFFSETS ---
// Standard 8-directional movement offsets
static const int ROUTE_OFFSETS[] = { -162, 1, 162, -1, -161, 163, 161, -163 };
static const int ROUTE_OFFSETS_X[] = { 0, 1, 0, -1,  1, 1, -1, -1 };
static const int ROUTE_OFFSETS_Y[] = { -1, 0, 1,  0, -1, 1,  1, -1 };


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
    // Moving entities
    int is_citizen = 0;
    int is_tower_sentry = 0;
    int is_friendly = 0;
    int is_enemy = 0;
    int is_boat = 0;
    int is_flotsam = 0;
    // Citizen permissions
    int ignores_roadblocks = 0;
    int travel_roads = 0;
    int travel_highways = 0;
    int travel_gardens = 0;
    int travel_land = 0;
    int travel_blocked = 0;
    int travel_aqueduct = 0;
    int travel_reservoir_connector = 0;
    // Buildings (drag build)
    int is_road = 0;
    int is_highway = 0;
    int is_aqueduct = 0;
    int is_wall = 0;
    // Misc
    int boost_highway = 0;
    int unblocking_rome = 0;
    int water_cost_multiplier = 3; // Base cost for water
    int target_building_id;  // target building to attack
    int through_building_id; // building allowed to pass through
} TravelRules;

// Defines route configuration (indexed by the RouteType enum.)
static const TravelRules ROUTE_RULE_CONFIG[] = {
    // Moving entities
    [ROUTE_TYPE_CITIZEN] = {
        .is_citizen = 1,
        .travel_roads = 1,
        .travel_gardens = 1,
    },
    [ROUTE_TYPE_TOWER_SENTRY] = {
        .is_citizen = 1,
        .is_tower_sentry = 1,
        .travel_roads = 1,
        .travel_gardens = 1,
    },
    [ROUTE_TYPE_FRIENDLY] = {
        .is_friendly = 1,
    },
    [ROUTE_TYPE_ENEMY] = {
        .is_enemy = 1,
    },
    [ROUTE_TYPE_BOAT] = {
        .is_boat = 1,
    },
    [ROUTE_TYPE_FLOTSAM] = {
        .is_flotsam = 1,
    },
    // Buildings (drag build)
    [ROUTE_TYPE_ROAD] = {
        .is_road = 1,
    },
    [ROUTE_TYPE_HIGHWAY] = {
        .is_highway = 1,
    },
    [ROUTE_TYPE_AQUEDUCT] = {
        .is_aqueduct = 1,
    },
    [ROUTE_TYPE_WALL] = {
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



// Returns the distance grid (should point to the global distance struct)
const map_routing_distance_grid *map_routing_get_distance_grid(void)
{
    // FIX: Using the global 'distance' struct (must be defined in routing.c or similar)
    return &distance;
}

// Clears the fighting status map and resets the queue pointers
static void clear_data(RoutingContext *ctx)
{
    reset_fighting_status();
    ctx->queue.head = 0;
    ctx->queue.tail = 0;
}

// Enqueues an item for a simple (non-ordered) queue (e.g., Breadth-First Search)
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

static inline int ordered_queue_parent(int index)
{
    // FIX: Use integer division (no need for float cast)
    return (index - 1) / 2;
}


// Corrected ordered_queue_swap
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
static void ordered_queue_reorder(RoutingContext *ctx, int start_index)
{
    int left_child = 2 * start_index + 1;
    if (left_child >= ctx->queue.tail) {
        return;
    }
    int right_child = left_child + 1;
    int smallest = start_index;

    // NOTE: Accessing distances via the global 'distance' struct here
    int smallest_dist = distance.possible.items[ctx->queue.items[smallest]];
    // int smallest_dist = ctx->distance.possible.items[ctx->queue.items[smallest]];
    // which of these two?

    // Compare with Left Child
    int left_dist = distance.possible.items[ctx->queue.items[left_child]];
    if (left_dist < smallest_dist) {
        smallest = left_child;
        smallest_dist = left_dist; // Update the reference value
    }
    // Compare with Right Child
    if (right_child < ctx->queue.tail) {
        int right_dist = distance.possible.items[ctx->queue.items[right_child]];
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
static inline void ordered_queue_reduce_index(RoutingContext *ctx, int index, int offset, int dist)
{
    // The given 'offset' is already placed at 'index' in the queue by ordered_enqueue.
    // The parameter 'dist' is the new possible distance.

    // While not at the root (index > 0) AND new distance is smaller than parent's distance
    while (index > 0 && distance.possible.items[ctx->queue.items[ordered_queue_parent(index)]] > dist) {
        // FIX: The original was missing the 'ctx' argument.
        ordered_queue_swap(ctx, index, ordered_queue_parent(index));
        index = ordered_queue_parent(index);
    }
}

// Enqueues a new item into the Priority Queue (Min-Heap)
static void ordered_enqueue(RoutingContext *ctx, int next_offset, int current_dist, int remaining_dist)
{
    int possible_dist = remaining_dist + current_dist; // G + H (A* total distance)
    int index;

    // Check if the node is ALREADY in the priority queue
    if (distance.possible.items[next_offset]) {
        // Node is in the queue. Is the new path better?
        if (distance.possible.items[next_offset] <= possible_dist) {
            return; // New path is worse or equal, do nothing
        }
        // New path is better: Get its current position in the heap
        index = ctx->node_index[next_offset];
    } else {
        // Node is NOT in the queue (or distance is 0). Add it to the end.
        index = ctx->queue.tail;
        ctx->queue.tail++;
        // Set the index in the lookup table
        ctx->node_index[next_offset] = index;
    }

    // Set the new (G) and possible (G+H) distances
    distance.determined.items[next_offset] = current_dist;
    distance.possible.items[next_offset] = possible_dist;

    // Use the reduce index function to bubble the new (or updated) node up the heap
    ordered_queue_reduce_index(ctx, index, next_offset, possible_dist);
}

// Checks if an offset is valid AND if the path to it hasn't been determined yet, or the new path is better.
static inline int valid_offset(int grid_offset, int possible_dist)
{
    // FIX: 'distance' must be accessed globally
    int determined = distance.determined.items[grid_offset];
    // FIX: Should compare against 'distance.possible' for A* (or just determined for Dijkstra)
    // Assuming 'distance.determined' stores the G-cost (path found so far)
    return map_grid_is_valid_offset(grid_offset) && (determined == 0 || possible_dist < determined);
}

// Manhattan distance (Heuristic for A*)
static inline int distance_left(int x, int y)
{
    // FIX: 'distance' must be accessed globally
    // Wait, from one of the 32 buffers or however many you want right?
    return abs(distance.dst_x - x) + abs(distance.dst_y - y);
}

// Declaration of the master travel function
static int callback_travel_master(RoutingContext *ctx, int offset, int next_offset,
 int direction_index, const TravelRules *rules, int *out_cost);

// Finds the best route from A to B (A* Search)
static void route_queue_from_to(RoutingContext *ctx, int src_x, int src_y, int dst_x, int dst_y, const TravelRules *rules)
{
    // 1. Setup Phase (Clear queues, initialize distances, enqueue source)
    // FIX: MAX_SEARCH_ITERATIONS must be defined externally (or use the #define from routing.h)
    int guard_count = 0;
    int destination_offset = map_grid_offset(dst_x, dst_y);
    int source_offset = map_grid_offset(src_x, src_y);

    // FIX: Assign target position to global distance struct for heuristic calculation
    distance.dst_x = dst_x;
    distance.dst_y = dst_y;

    // Initial enqueue using A* heuristic
    ordered_enqueue(ctx, source_offset, 0, distance_left(src_x, src_y));

    // Determine number of directions (4 for cardinal, 8 for all)
    int num_directions = rules->cardinal_travel ? 4 : 8;

    // 2. Search Loop (While queue is not empty and guard count is not exceeded)
    while (ctx->queue.tail > 0 && guard_count++ < MAX_SEARCH_ITERATIONS) {

        // Pop the current best node (offset)
        int current_offset = ordered_queue_pop(ctx);
        // FIX: The cost is now stored in 'distance.determined' not 'ctx->distance'
        int current_distance = distance.determined.items[current_offset];

        // Early exit if destination found
        if (current_offset == destination_offset) {
            return;
        }

        // 3. Iterate Neighbors
        for (int i = 0; i < num_directions; i++) {

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
void route_queue_all_from(RoutingContext *ctx, int source_offset, const TravelRules *rules)
{
    int tiles_processed = 0;
    int num_directions = rules->cardinal_travel ? 4 : 8; // Determine number of directions

    // Initial enqueue (Dijkstra does not need the remaining_dist heuristic, so it's 0)
    ordered_enqueue(ctx, source_offset, 0, 0);

    // Use the queue tail to check if the queue is empty (head is always 0 in a Min-Heap)
    while (ctx->queue.tail > 0) {

        if (++tiles_processed > MAX_SEARCH_ITERATIONS) {
            break; // Guard against excessively long searches
        }

        // 1. Pop the next best tile from the priority queue
        int current_offset = ordered_queue_pop(ctx);
        // FIX: Get current path cost from the determined distance map
        int current_distance = distance.determined.items[current_offset];

        // 2. Iterate through all neighbors
        for (int i = 0; i < num_directions; i++) {

            // Calculate neighbor tile offset
            int next_offset = current_offset + ROUTE_OFFSETS[i];

            // Check if the tile is already determined (if so, any path through it is done)
            // FIX: This check is redundant with the check at step 5/inside ordered_enqueue, 
            // but is a common optimization. Let's keep it simple and rely on the distance check.

            // 3. Determine Cost and Passability using the Master function
            int travel_cost = 0;
            if (callback_travel_master(ctx, current_offset, next_offset, i, rules, &travel_cost)) {

                // 4. Calculate new path distance
                int new_dist = current_distance + travel_cost;

                // 5. Standard Dijkstra Check (Is this a better path?)
                // If new_dist is better, it will be added/updated in the priority queue (heuristic is 0)
                ordered_enqueue(ctx, next_offset, new_dist, 0);
            }
        }
    }
}

// --- PASSABILITY AND COST HELPER FUNCTIONS ---
// Returns the cost of a citizen passing through a tile, returns -1 if impassable
// NOTE: This function needs access to the rules (pass them in or use the global rules).
static inline int check_citizen_passability(int next_offset, const TravelRules *rules) // Added rules pointer
{
    int base_cost = 100; // Default base cost

    // Common Terrain Access Checks
    switch (terrain_access.items[next_offset]) {
        case TERRAIN_ACCESS_ROAD:
            if (!rules->travel_roads) return -1;
            break;
        case TERRAIN_ACCESS_HIGHWAY:
            if (!rules->travel_highways) return -1;
            break;
        case TERRAIN_ACCESS_PASSABLE: // Gardens, rubble, access ramps
            if (!rules->travel_gardens) return -1;
            break;
        case TERRAIN_ACCESS_EMPTY: // Open land
            if (!rules->travel_land) return -1;
            break;
        case TERRAIN_ACCESS_BLOCKED: // Completely impassable terrain
            if (!rules->travel_blocked) return -1;
            return -1; // Should always return -1 regardless of flag
        case TERRAIN_ACCESS_AQUEDUCT: // Top of an aqueduct
            if (!rules->travel_aqueduct) return -1;
            break;
        case TERRAIN_ACCESS_RESERVOIR_CONNECTOR: // Reservoir structure
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
            if (!rules->is_tower_sentry) {
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




/** @brief Helper function to calculate the travel cost and check permission
* Returns 1 if travel is permitted, 0 otherwise
* @param RoutingContext
* @param current_offset
* @param next_offset
* @param direction_index
* @param rules
* @param out_cost
**/
static int callback_travel_master(RoutingContext *ctx, int current_offset, int next_offset,
int direction_index, const TravelRules *rules, int *out_cost)
{
    // Defaults
    int base_cost = 0;
    int cost_multiplier = 1;
    // Incorrect, instead this must detect if the direction is cardinal or not then apply
    int direction_cost_multiplier = (direction_index < 4) ? 100 : 141;

    // If impassable, return
    if (!rules->terrain_passable_func(next_offset)) {
        return 0;
    }

    // If not allowed through, return (roadblocks)
    if (rules->ignores_roadblocks == 0 &&
        // You will need to create this function
        !map_roadblock_check(next_offset, rules->ignores_roadblocks)) {
        return 0;
    }

    // If there's anyone fighting in the tile, return
    // Bit 0 = Friendly Fighting, Bit 1 = Enemy Fighting
    uint8_t fight_status = ctx->fighting_data.items[next_offset];
    if (fight_status & 1) { /* Avoid Friendly Fight */ return 0; }
    if (fight_status & 2) { /* Avoid Enemy Fight */ return 0; }

    // Citizen
    if (rules->is_citizen) {
        base_cost = check_citizen_passability(ctx, next_offset);
        if (base_cost == -1) { return 0 };
    }

    // Noncitizen (traders, enemies, etc.)
    if (rules->is_noncitizen) {
        // Friendly noncitizens may pass through all terrain except fort and blocked
        switch (terrain_access.items[next_offset]) {
            // --- Strictly passable/attackable terrain --- ///
            case TERRAIN_ACCESS_PASSABLE:
            case NONCITIZEN_2_CLEARABLE:
            case TERRAIN_ACCESS_AQUEDUCT:
            case TERRAIN_ACCESS_WALL_PASSABLE:
            case TERRAIN_ACCESS_WALL_BLOCKED:
            case TERRAIN_ACCESS_GATEHOUSE:
                break;
            case TERRAIN_ACCESS_BUILDING:
                // Friendlies can pass through fine
                if (rules->is_friendly) { break; }
                // If your enemies can't pass through or attack, return
                int map_building_id = map_building_at(next_offset);
                if (map_building_id != ctx->through_building_id && map_building_id != ctx->target_building_id) {
                    return 0;
                }
            case TERRAIN_ACCESS_FORT:
                if (rules->is_friendly) { return 0 }; // Friendlies can't travel through forts
            case TERRAIN_ACCESS_BLOCKED:
                return 0; // May never pass through impassable terrain
        }
        // Base Cost
        base_cost = BASE_TERRAIN_COST(terrain_access.items[next_offset]);
    }

    // Boats
    if (rules->is_boat) {
        // Passability Check
        // Next position must be water AND must not be blocked (low bridge or explicit block).
        if (!map_terrain_is_water(next_offset) ||
            terrain_access.items[next_offset] == TERRAIN_ACCESS_WATER_BLOCKED ||
            terrain_access.items[next_offset] == TERRAIN_ACCESS_WATER_LOW_BRIDGE) {
            return 0;
        }

        // Base Cost
        base_cost = BASE_TERRAIN_COST(terrain_access.items[next_offset]);

        // Water drag (in the form of a cost multiplier for simplicity)
        cost_multiplier = 3;
        // Discourages boats from sailing across the map's edge
        if (terrain_access.items[next_offset] == TERRAIN_ACCESS_WATER_EDGE) {
            cost_multiplier += 12;
        }
    }

    // Speed Modifiers (e.g highways)
    // MUST be walking in a cardinal direction, or this bonus won't apply
    if (rules->boost_highway && map_terrain_is_highway(next_offset)) {
        if is_cardinal(direction) // Mockup placeholder, actually make it
        {
            cost_multiplier = 0.5;
        }
    }
    // Calculate cost
    if (base_cost == -1) { return 0 }
    *out_cost = base_cost * direction_cost_multiplier * cost_multiplier;
    return 1;

    // --- FLOTSAM --- (no cost it just floats towards non-blocked tiles)
    // Flotsam
    if (rules->is_flotsam) {
        // Next position must be water and not blocked
        if (map_terrain_is_water(next_offset) && terrain_access.items[next_offset] != = TERRAIN_ACCESS_WATER_BLOCKED) {
            return 1;
        }
        return 0;
    }

    // --- BUILDINGS --- (drag build)
    // Road
    if (rules->is_road) {
        switch (terrain_access.items[next_offset]) {
            case TERRAIN_ACCESS_PASSABLE: // rubble, garden, access ramp
            case TERRAIN_ACCESS_BLOCKED: // non-empty land
                return 0;
            case TERRAIN_ACCESS_AQUEDUCT:
                if (!map_can_place_road_under_aqueduct(next_offset)) {
                    distance.determined.items[next_offset] = -1;
                    return 0;
                }
                break;
            default:
                if (map_terrain_is(next_offset, TERRAIN_BUILDING)) {
                    return 0;
                }
        }
        return 1;
    }

    // Highway
    if (rules->is_highway) {
        // Next position must be a valid placement for a highway
        if (can_build_highway(next_offset, 1)) {
            return 1;
        }
        return 0;
    }
    // Aqueduct
    if (rules->is_aqueduct) {
        // If the aqueduct cannot be placed on this specific road tile, return
        if (map_terrain_is(next_offset, TERRAIN_ROAD) && !map_can_place_aqueduct_on_road(next_offset)) {
            distance.determined.items[next_offset] = -1;
            return 0;
        }
        // If the aqueduct cannot be placed on this specific highway tile, return
        if (map_terrain_is(next_offset, TERRAIN_HIGHWAY) && !map_can_place_aqueduct_on_highway(next_offset, 1)) {
            return 0;
        }
        // Check for other invalid terrain
        switch (terrain_access.items[next_offset]) {
            case TERRAIN_ACCESS_AQUEDUCT:
            case TERRAIN_ACCESS_PASSABLE: // rubble, garden, access ramp
            case TERRAIN_ACCESS_BLOCKED: // non-empty land
                return 0;
        }
        // Check if the terrain is a building, in which case only reservoir connectors are allowed
        if (map_terrain_is(next_offset, TERRAIN_BUILDING)) {
            if (terrain_access.items[next_offset] != TERRAIN_ACCESS_RESERVOIR_CONNECTOR) {
                return 0;
            }
        }
        return 1;
    }
    // Wall
    if (rules->is_wall) {
        // Next position must be clear terrain
        if (terrain_access.items[next_offset] == TERRAIN_ACCESS_EMPTY) {
            return 1;
        }
        return 0;
    }

    // --- SPECIAL --- (causes side effects from the search itself, such as destroying buildings)
    // Unblock Path To Rome (delete walls/aqueducts blocking the path to rome)
    if (rules->unblocking_rome) {
        // Check if the current tile contains a blockage (wall/aqueduct)
        if (map_terrain_is(next_offset, TERRAIN_AQUEDUCT | TERRAIN_WALL)) {

            // ACTION: Remove the blockage (side effect)
            map_terrain_remove(next_offset, TERRAIN_CLEARABLE);
        }
    }
}

// Internal helper function that handles all routing types
static void map_routing_calculate_distances(int x, int y, RouteType type)
{
    int grid_offset = map_grid_offset(x, y);

    // Blocked Tile Check (We can use the rules pointer for this check)
    // Assuming the array lookup is safe (i.e., you check 'type' is valid)
    const TravelRules *rules = &ROUTE_RULE_CONFIG[type];

    // We must initialize the source tile's distance and enqueue it.
    // Assuming 0 is the start distance and ordered_enqueue is used for the first node.
    // ctx.distance.determined.items[grid_offset] = 0; // If using Dijkstra/A*
    // ordered_enqueue(&ctx, grid_offset, 0); 

    // --- Initialize Context & Load Rules ---
    RoutingContext ctx;
    memset(&ctx, 0, sizeof(RoutingContext));

    // NOTE: 'rules' is already a pointer to the static config data.
    // We pass this pointer directly to the core routing function.

    // Populate Fighting Data
    populate_fighting_data(&ctx)

        // --- Start Search ---
        ++stats.total_routes_calculated;
    if (rules->is_enemy) {
        ++stats.enemy_routes_calculated
    }

    // Call Generic Core Routing Function, passing the static rules
    route_queue_all_from(&ctx, grid_offset, rules);

    // --- Copy Results ---
    // The result is in 'ctx.distance'. It MUST be copied out 
    // to the external storage (the global/figure distance map).
    // map_routing_copy_results(&ctx);
}

// What to do about this
if (only_through_building_id) {
    ctx->through_building_id = only_through_building_id;
    // due to formation offsets, the destination building may not be the same as the "through building" (a.k.a. target building)
    ctx->target_building_id = map_building_at(map_grid_offset(dst_x, dst_y));
    route_queue_from_to(src_x, src_y, dst_x, dst_y, num_directions, 0, callback_travel_noncitizen_land_through_building);
} else {
    route_queue_from_to(src_x, src_y, dst_x, dst_y, num_directions, max_tiles, callback_travel_noncitizen_land);
}

// Marks an area as the start for the search
void map_routing_add_source_area(int x, int y, int size)
{
    if (!map_grid_is_inside(x, y, size)) {
        return;
    }
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            distance.determined.items[map_grid_offset(x + dx, y + dy)] = 0; // Replace distance by whatever appropriate
        }
    }
}




// Fighting related
static int is_fighting_friendly(figure * f)
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
static int is_fighting_enemy(figure * f)
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

// Save/Load Buffers
void map_routing_save_state(buffer * buf)
{
    buffer_write_i32(buf, 0); // unused counter
    buffer_write_i32(buf, stats.enemy_routes_calculated);
    buffer_write_i32(buf, stats.total_routes_calculated);
    buffer_write_i32(buf, 0); // unused counter
}

void map_routing_load_state(buffer * buf)
{
    buffer_skip(buf, 4); // unused counter
    stats.enemy_routes_calculated = buffer_read_i32(buf);
    stats.total_routes_calculated = buffer_read_i32(buf);
    buffer_skip(buf, 4); // unused counter
}