// Internal
#include "route.h"
// External
#include "core/array.h"
#include "core/log.h"
#include "core/direction.h"
#include "map/routing.h"
#include "map/routing_path.h"
// Defines
#define ARRAY_SIZE_STEP 600
#define MAX_PATH_LENGTH 500
typedef struct {
    unsigned int id;
    int figure_id;
    uint8_t directions[MAX_PATH_LENGTH];
} figure_path_data;

// -- PRIVATE FUNCTIONS --
// Calculate the current path assigned to the figure
int figure_path_distance(figure *f, int grid_offset)
{
    RoutingContext *ctx = f-> // ???

        // Reads the distance from its own reserved map in the global pool
        return ctx->route_map_pool[f->route_map_id].items[grid_offset];
}
// Contains every path that ???
static array(figure_path_data) paths;
// Creates a new path ???
static void create_new_path(figure_path_data *path, unsigned int position)
{
    path->id = position;
}
// Determines if a path is used ???
static int path_is_used(const figure_path_data *path)
{
    return path->figure_id != 0;
}
// Calculates the length of a path
static in calculate_path_length(figure *f, const figure_path_data *path)
{
    // Limits directions to just adjacent ones so they don't try to teleport
    int direction_limit = DIR_MAX_MOVEMENT;

    if (f->disallow_diagonal) {
        direction_limit = 4; // Clearly not how it works
    }

    int direction_limit = 8;
    // Forbids diagonal movement, as is the case for roamers
    if (f->disallow_diagonal) {
        direction_limit = 4;
    }

    // --- CASE: BOAT MOVEMENT ---
    if (f->is_boat) {
        int flotsam_mode = (f->is_boat == 2);
        if (flotsam_mode) {
            map_routing_calculate_distances_water_flotsam(f->x, f->y);
        } else {
            map_routing_calculate_distances_water_boat(f->x, f->y);
        }
        return map_routing_get_path_on_water(path->directions,
                f->destination_x, f->destination_y, 1);

        // --- CASE: LAND MOVEMENT (Switch by terrain usage) ---
    } else {
        int can_travel = 0;

        switch (f->terrain_usage) {
            case TERRAIN_USAGE_ENEMY: // Figure is an enemy
                // check to see if we can reach our destination by going around the city walls
                can_travel = map_routing_noncitizen_can_travel_over_land(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit, f->destination_building_id, 5000);
                if (!can_travel) {
                    can_travel = map_routing_noncitizen_can_travel_over_land(f->x, f->y,
                        f->destination_x, f->destination_y, direction_limit, 0, 25000);
                    if (!can_travel) {
                        can_travel = map_routing_noncitizen_can_travel_through_everything(
                            f->x, f->y, f->destination_x, f->destination_y, direction_limit);
                    }
                }
                break;
            case TERRAIN_USAGE_WALLS: // Figure is a tower sentry walking over a wall
                can_travel = map_routing_can_travel_over_walls(f->x, f->y,
                    f->destination_x, f->destination_y, 4);
                break;
            case TERRAIN_USAGE_ANIMAL: // Figure is an animal
                can_travel = map_routing_noncitizen_can_travel_over_land(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit, -1, 5000);
                break;
            case TERRAIN_USAGE_PREFER_ROADS: // Figure can travel over roads and gardens, but may also travel directly over land
                can_travel = map_routing_citizen_can_travel_over_road_garden(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                if (!can_travel) {
                    can_travel = map_routing_citizen_can_travel_over_land(f->x, f->y,
                        f->destination_x, f->destination_y, direction_limit);
                }
                break;
            case TERRAIN_USAGE_ROADS: // Figure only travels over roads and gardens
                can_travel = map_routing_citizen_can_travel_over_road_garden(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                break;
            case TERRAIN_USAGE_PREFER_ROADS_HIGHWAY: // Figure can travel over roads, gardens and highways, but may also travel directly over land
                can_travel = map_routing_citizen_can_travel_over_road_garden_highway(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                if (!can_travel) {
                    can_travel = map_routing_citizen_can_travel_over_land(f->x, f->y,
                        f->destination_x, f->destination_y, direction_limit);
                }
                break;
            case TERRAIN_USAGE_ROADS_HIGHWAY: // Figure can only travel over roads, gardens and highways
                can_travel = map_routing_citizen_can_travel_over_road_garden_highway(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                break;
            default:
                can_travel = map_routing_citizen_can_travel_over_land(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                break;
        }
        // Converts the boolean 'can_travel' result into the path directions and length (???)
        int path_length;
        if (can_travel) {
            // Special case handling for Wall Sentries
            if (f->terrain_usage == TERRAIN_USAGE_WALLS) {
                path_length = map_routing_get_path(path->directions, f->destination_x, f->destination_y, 4);
                if (path_length <= 0) {
                    path_length = map_routing_get_path(path->directions,
                        f->destination_x, f->destination_y, direction_limit);
                }
            } else {
                path_length = map_routing_get_path(path->directions,
                    f->destination_x, f->destination_y, direction_limit);
            }
        } else {
            path_length = 0;
        }
    }
    return path_length;
}

// -- CORE FUNCTIONS --
// Adds a new route to the figure
void figure_route_add(figure *f)
{
    // 1. Reset state
    f->routing_path_id = 0;
    f->routing_path_current_tile = 0;
    f->routing_path_length = 0;

    // 2. Setup paths array (Check for initial allocation)
    if (!paths.blocks && !array_init(paths, ARRAY_SIZE_STEP, create_new_path, path_is_used)) {
        log_error("Unable to create paths array. The game will likely crash.", 0, 0);
        return;
    }

    // 3. Get new slot
    figure_path_data *path;
    array_new_item_after_index(paths, 1, path);
    if (!path) {
        return;
    }

    // 4. Calculate route length
    int path_length = figure_path_distance(f, path);

    // 5. Commit route to figure state
    if (path_length) {
        path->figure_id = f->id;
        f->routing_path_id = path->id;
        f->routing_path_length = path_length;
    }
}

// Removes the route from the figure
void figure_route_remove(figure *f)
{
    // 1. Early exit if no route is assigned.
    if (f->routing_path_id <= 0) {
        return;
    }

    // 2. Safety Check: Validate path ID and ownership before modification.
    // Prevents issues if the path ID was recycled before this figure was cleaned up.
    const short path_id = f->routing_path_id;
    const int is_path_id_valid = (path_id < (short) paths.size);
    if (is_path_id_valid) {
        // Ensures the path slot is owned by this figure before marking it free. 
        figure_path_data *path_slot = array_item(paths, path_id);
        if (path_slot->figure_id == (int) f->id) {
            path_slot->figure_id = 0; // Mark the slot as free (soft deallocation).
        }
    }

    // 3. Clear the path ID from the figure's state.
    f->routing_path_id = 0;

    // 4. Attempt to reclaim memory (this can be performance heavy if called often)
    array_trim(paths);
}

// -- CLEANUP AND UTILITY FUNCTIONS --
// Clears all routes from the figure
void figure_route_clear_all(void)
{
    paths.size = 0;
    array_trim(paths);
}
// Cleans a specific route???
void figure_route_clean(void)
{
    figure_path_data *path;
    array_foreach(paths, path)
    {
        int figure_id = path->figure_id;
        if (figure_id > 0 && figure_id < figure_count()) {
            const figure *f = figure_get(figure_id);
            if (f->state != FIGURE_STATE_ALIVE || f->routing_path_id != (short) array_index) {
                path->figure_id = 0;
            }
        }
    }
    array_trim(paths);
}

// -- SAVE/LOAD STATE FUNCTIONS --
// Saves the state?
void figure_route_save_state(buffer *figures, buffer *buf_paths)
{
    int size = paths.size * sizeof(int);
    uint8_t *buf_data = malloc(size);
    buffer_init(figures, buf_data, size);

    size = paths.size * sizeof(uint8_t) * MAX_PATH_LENGTH;
    buf_data = malloc(size);
    buffer_init(buf_paths, buf_data, size);

    figure_path_data *path;
    array_foreach(paths, path)
    {
        buffer_write_i16(figures, path->figure_id);
        buffer_write_raw(buf_paths, path->directions, MAX_PATH_LENGTH);
    }
}
// Loads the state?
void figure_route_load_state(buffer *figures, buffer *buf_paths)
{
    int elements_to_load = (int) buf_paths->size / MAX_PATH_LENGTH;

    if (!array_init(paths, ARRAY_SIZE_STEP, create_new_path, path_is_used) ||
        !array_expand(paths, elements_to_load)) {
        log_error("Unable to create paths array. The game will likely crash.", 0, 0);
        return;
    }

    int highest_id_in_use = 0;

    for (int i = 0; i < elements_to_load; i++) {
        figure_path_data *path = array_next(paths);
        path->figure_id = buffer_read_i16(figures);
        buffer_read_raw(buf_paths, path->directions, MAX_PATH_LENGTH);
        if (path->figure_id) {
            highest_id_in_use = i;
        }
    }
    paths.size = highest_id_in_use + 1;
}

// --- WRAPPERS ---
int figure_route_get_direction(int path_id, int index)
{
    return array_item(paths, path_id)->directions[index];
}
