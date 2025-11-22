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

#define MAX_QUEUE GRID_SIZE * GRID_SIZE // why is it this number instead of another?
#define MAX_SEARCH_ITERATIONS 50000 // higher? lower?
#define MAX_CONCURRENT_ROUTES 32
#define BASE_TERRAIN_COST(x) ((int)(x))

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
// Used for callbacks (?)
typedef int (*RoutingCallback)(RoutingContext *ctx, int offset, int next_offset, int direction);

static const int ROUTE_OFFSETS[] = { -162, 1, 162, -1, -161, 163, 161, -163 };
static const int ROUTE_OFFSETS_X[] = { 0, 1, 0, -1,  1, 1, -1, -1 };
static const int ROUTE_OFFSETS_Y[] = { -1, 0, 1,  0, -1, 1,  1, -1 };

static struct {
    int total_routes_calculated;
    int enemy_routes_calculated;
} stats;

static struct {
    grid_u8 status;
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
    int unblocking_rome = 0;
    int water_cost_multiplier = 3; // Base cost for water
    int target_building_id;  // target building to attack
    int through_building_id; // building allowed to pass through
} TravelRules;

// Static configuration table, indexed by the RouteType enum.
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
    }
    [ROUTE_TYPE_HIGHWAY] = {
        .is_road = 1,
    }
    [ROUTE_TYPE_AQUEDUCT] = {
        .is_road = 1,
    }
    [ROUTE_TYPE_WALL] = {
        .is_road = 1,
    }
};

// Populates the fighting data thing (?) inside the routing context
static void populate_fighting_data(RoutingContext *ctx)
{
    // 1. Clear the grid first (reset all tiles to 0)
    // Assuming 'fighting_data' inside ctx is a full grid array
    map_grid_clear_u8(ctx->fighting_data.items);

    // 2. Iterate over all figures
    // You need a way to loop all figures. Assuming 'figure_foreach' exists:
    int total_figures = figure_count();
    for (int i = 1; i < total_figures; i++) {
        // Check if the figure is alive
        figure *f = figure_get(i);
        if (f->state != FIGURE_STATE_ALIVE) continue;

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

// Clears the... map grid?
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


// Corrected ordered_queue_swap (assuming you add a helper that uses this)
static inline void ordered_queue_swap(RoutingContext *ctx, int first, int second)
{
    int temp_offset = ctx->queue.items[first];

    // 1. Swap the offsets in the main queue array
    ctx->queue.items[first] = ctx->queue.items[second];
    ctx->queue.items[second] = temp_offset;

    // 2. Update the lookup table with the NEW positions
    ctx->node_index[ctx->queue.items[first]] = first;
    ctx->node_index[ctx->queue.items[second]] = second;
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

// Using the simplified master travel function based on your flags
// Put this instead of the others
// Wai tthat's what this does right???
static int callback_travel_master(RoutingContext *ctx, int offset, int next_offset,
 int direction_index, const TravelRules *rules);


// Finds the best route? (???)
static void route_queue_from_to(RoutingContext *ctx, int src_x, int src_y, int dst_x, int dst_y, const TravelRules *rules)
{
    // 1. Setup Phase (Clear queues, initialize distances, enqueue source)
    int MAX_SEARCH_ITERATIONS = 0;
    int destination_offset = map_grid_offset(dst_x, dst_y);
    // Assign the number of directions based on the rules (wait... does this really handle the diagonal stuff???)
    int num_directions = rules->num_directions;

    // Wait where's the guard count int???
    while (ctx->queue.head != ctx->queue.tail && guard_count++ < MAX_SEARCH_ITERATIONS) {

        // 2. Pop the current best node (offset)
        int current_offset = ordered_queue_pop(ctx);
        int current_distance = ctx->distance.determined.items[current_offset];

        // Early exit if destination found
        if (current_offset == destination_offset) {
            return;
        }

        // 3. Iterate Neighbors
        for (int i = 0; i < num_directions; i++) {

            int next_offset = current_offset + ROUTE_OFFSETS[i];
            int next_x = current_offset % MAP_WIDTH + ROUTE_OFFSETS_X[i]; // Assuming map width is used
            int next_y = current_offset / MAP_WIDTH + ROUTE_OFFSETS_Y[i];

            // Boundary Check
            if (!map_grid_is_inside(next_x, next_y, 1)) {
                continue;
            }

            // --- The Refactored Logic: Use the master function ---
            int travel_cost = 0;
            if (callback_travel_master(ctx, current_offset, next_offset, i, rules, &travel_cost)) {

                // If the master function returns success (1), calculate new cost
                int new_dist = current_distance + travel_cost;

                // If path is better, enqueue it
                if (new_dist < ctx->distance.determined.items[next_offset]) {
                    ordered_enqueue(ctx, next_offset, new_dist);
                }
            }
        }
    }
}

// ???
void route_queue_all_from(RoutingContext *ctx, int source, const TravelRules *rules)
{
    int tiles_processed = 0;
    int num_directions = rules->num_directions;

    // Use the queue head/tail to check if the queue is empty
    while (ctx->queue.head != ctx->queue.tail) {

        if (++tiles_processed > MAX_SEARCH_ITERATIONS) {
            break; // Guard against excessively long searches
        }

        // 1. Pop the next best tile from the priority queue
        int offset = ordered_queue_pop(ctx);
        int current_distance = ctx->distance.determined.items[offset]; // Get current path cost

        // 2. Iterate through all neighbors
        for (int i = 0; i < num_directions; i++) {

            // Calculate neighbor tile offset
            int next_offset = offset + ROUTE_OFFSETS[i];

            // Boundary/Basic Validity Check
            if (!valid_offset(next_offset, current_distance)) {
                continue;
            }

            // 3. Determine Cost and Passability using the Master function
            int travel_cost = 0;
            if (callback_travel_master(ctx, offset, next_offset, i, rules, &travel_cost)) {

                // 4. Calculate new path distance
                int new_dist = current_distance + travel_cost;

                // 5. Standard A* / Dijkstra Check (Is this a better path?)
                if (new_dist < ctx->distance.determined.items[next_offset]) {

                    // Update distance map in the context
                    ctx->distance.determined.items[next_offset] = new_dist;

                    // Enqueue/Re-enqueue (ordered_enqueue handles the priority queue update)
                    ordered_enqueue(ctx, next_offset, new_dist);
                }
            }
        }
    }
}

// Returns the cost of a citizen passing through a tile, returns -1 if impassable
static inline int check_citizen_passability(RoutingContext *ctx, int next_offset)
{
    // Common 
    switch (terrain_land_citizen.items[next_offset]) {
        case CITIZEN_0_ROAD:
            if (!rules->travel_roads) return -1;
            break;
        case CITIZEN_1_HIGHWAY:
            if (!rules->travel_highways) return -1;
            break;
        case CITIZEN_2_PASSABLE_TERRAIN:
            if (!rules->travel_gardens) return -1;
            break;
        case CITIZEN_4_CLEAR_TERRAIN:
            if (!rules->travel_land) return -1;
            break;
        case CITIZEN_N1_BLOCKED:
            if (!rules->travel_blocked) return -1; // Nobody allowed
            break;
        case CITIZEN_N3_AQUEDUCT:
            if (!rules->travel_aqueduct) return -1; // Nobody allowed
            break;
        case CITIZEN_N4_RESERVOIR_CONNECTOR:
            if (!rules->travel_reservoir_connector) return -1; // Nobody allowed
            break;
    }
    // Walls
    switch (terrain_wall.items[next_offset]) {
        case WALL_0_PASSABLE:
            if (!rules->is_tower_sentry) {
                return -1;
            }

        case WALL_N1_BLOCKED:
            return -1;
    }
    // Base Cost (if valid)
    base_cost = BASE_TERRAIN_COST(terrain_land_citizen.items[next_offset]);
    base_cost = BASE_TERRAIN_COST(terrain_wall.items[next_offset]);
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
        switch (terrain_land_noncitizen.items[next_offset]) {
            // --- Strictly passable/attackable terrain --- ///
            case NONCITIZEN_0_PASSABLE:
            case NONCITIZEN_2_CLEARABLE:
            case NONCITIZEN_3_WALL:
            case NONCITIZEN_4_GATEHOUSE:
                break;
            case NONCITIZEN_1_BUILDING:
                // Friendlies can pass through fine
                if (rules->is_friendly) { break; }
                // If your enemies can't pass through or attack, return
                int map_building_id = map_building_at(next_offset);
                if (map_building_id != ctx->through_building_id && map_building_id != ctx->target_building_id) {
                    return 0;
                }
            case NONCITIZEN_5_FORT:
                if (rules->is_friendly) { return 0 }; // Friendlies can't travel through forts
            case NONCITIZEN_N1_BLOCKED:
                return 0; // May never pass through blocked terrain
        }
        // Base Cost
        base_cost = BASE_TERRAIN_COST(terrain_land_noncitizen.items[next_offset]);
    }

    // Boats
    if (rules->is_boat) {
        // Passability Check
        // Next position must be water AND must not be blocked (low bridge or explicit block).
        if (!map_terrain_is_water(next_offset) ||
            terrain_water.items[next_offset] == WATER_N1_BLOCKED ||
            terrain_water.items[next_offset] == WATER_N3_LOW_BRIDGE) {
            return 0;
        }

        // Base Cost
        base_cost = BASE_TERRAIN_COST(terrain_water.items[next_offset]);

        // Water drag (in the form of a cost multiplier for simplicity)
        cost_multiplier = 3;
        // Discourages boats from sailing across the map's edge
        if (terrain_water.items[next_offset] == WATER_N2_MAP_EDGE) {
            cost_multiplier += 12;
        }
    }

    // Speed Modifiers (e.g highways)
    // MUST be walking in a cardinal direction, or this bonus won't apply
    if (rules->allow_highway && map_terrain_is_highway(next_offset)) {
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
        if (map_terrain_is_water(next_offset) && terrain_water.items[next_offset] != = WATER_N1_BLOCKED) {
            return 1;
        }
        return 0;
    }

    // --- BUILDINGS --- (drag build)
    // Road
    if (rules->is_road) {
        switch (terrain_land_citizen.items[next_offset]) {
            case CITIZEN_2_PASSABLE_TERRAIN: // rubble, garden, access ramp
            case CITIZEN_N1_BLOCKED: // non-empty land
                return 0;
            case CITIZEN_N3_AQUEDUCT:
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
        switch (terrain_land_citizen.items[next_offset]) {
            case CITIZEN_N3_AQUEDUCT:
            case CITIZEN_2_PASSABLE_TERRAIN: // rubble, garden, access ramp
            case CITIZEN_N1_BLOCKED: // non-empty land
                return 0;
        }
        // Check if the terrain is a building, in which case only reservoir connectors are allowed
        if (map_terrain_is(next_offset, TERRAIN_BUILDING)) {
            if (terrain_land_citizen.items[next_offset] != CITIZEN_N4_RESERVOIR_CONNECTOR) {
                return 0;
            }
        }
        return 1;
    }
    // Wall
    if (rules->is_wall) {
        // Next position must be clear terrain
        if (terrain_land_citizen.items[next_offset] == CITIZEN_4_CLEAR_TERRAIN) {
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

// Save/Load (unused)
void map_routing_save_state(buffer * buf)
{
    buffer_write_i32(buf, 0); // unused counter
    buffer_write_i32(buf, 0); // unused counter
    buffer_write_i32(buf, 0); // unused counter
    buffer_write_i32(buf, 0); // unused counter
}

void map_routing_load_state(buffer * buf)
{
    buffer_skip(buf, 4); // unused counter
    buffer_skip(buf, 4); // unused counter
    buffer_skip(buf, 4); // unused counter    
    buffer_skip(buf, 4); // unused counter
}
