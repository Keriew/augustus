#include "route.h"

#include "core/array.h"
#include "core/log.h"
#include "game/save_version.h"
#include "map/routing.h"
#include "map/routing_path.h"

#define ARRAY_SIZE_STEP 600
#define MAX_ORIGINAL_PATH_LENGTH 500

static array(figure_path_data) paths;

static void create_new_path(figure_path_data *path, unsigned int position)
{
    path->id = position;
}

static int path_is_used(const figure_path_data *path)
{
    return path->figure_id != 0;
}

void figure_route_clear_all(void)
{
    figure_path_data *path;

    array_foreach(paths, path) {
        free(path->directions);
        path->directions = 0;
        path->directions = 0;
        path->total_directions = 0;
        path->current_step = 0;
        path->same_direction_count = 0;

    }
    paths.size = 0;
}

void figure_route_clean(void)
{
    figure_path_data *path;
    array_foreach(paths, path) {
        unsigned int figure_id = path->figure_id;
        if (figure_id > 0 && figure_id < figure_count()) {
            const figure *f = figure_get(figure_id);
            if (f->state != FIGURE_STATE_ALIVE || f->routing_path_id != array_index) {
                path->figure_id = 0;
                free(path->directions);
                path->directions = 0;
                path->total_directions = 0;
                path->current_step = 0;
                path->same_direction_count = 0;
            }
        }
    }
    array_trim(paths);
}

void figure_route_add(figure *f)
{
    f->routing_path_id = 0;
    f->routing_path_current_tile = 0;
    f->routing_path_length = 0;
    int direction_limit = 8;
    if (f->disallow_diagonal) {
        direction_limit = 4;
    }
    if (!paths.blocks && !array_init(paths, ARRAY_SIZE_STEP, create_new_path, path_is_used)) {
        log_error("Unable to create paths array. The game will likely crash.", 0, 0);
        return;
    }
    figure_path_data *path;
    array_new_item_after_index(paths, 1, path);
    if (!path) {
        return;
    }
    int path_length;
    if (f->is_boat) {
        if (f->is_boat == 2) { // flotsam
            map_routing_calculate_distances_water_flotsam(f->x, f->y);
            path_length = map_routing_get_path_on_water(path, f->destination_x, f->destination_y, 1);
        } else {
            map_routing_calculate_distances_water_boat(f->x, f->y);
            path_length = map_routing_get_path_on_water(path, f->destination_x, f->destination_y, 0);
        }
    } else {
        // land figure
        int can_travel;
        switch (f->terrain_usage) {
            case TERRAIN_USAGE_ENEMY:
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
            case TERRAIN_USAGE_WALLS:
                can_travel = map_routing_can_travel_over_walls(f->x, f->y,
                    f->destination_x, f->destination_y, 4);
                break;
            case TERRAIN_USAGE_ANIMAL:
                can_travel = map_routing_noncitizen_can_travel_over_land(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit, -1, 5000);
                break;
            case TERRAIN_USAGE_PREFER_ROADS:
                can_travel = map_routing_citizen_can_travel_over_road_garden(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                if (!can_travel) {
                    can_travel = map_routing_citizen_can_travel_over_land(f->x, f->y,
                        f->destination_x, f->destination_y, direction_limit);
                }
                break;
            case TERRAIN_USAGE_ROADS:
                can_travel = map_routing_citizen_can_travel_over_road_garden(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                break;
            case TERRAIN_USAGE_PREFER_ROADS_HIGHWAY:
                can_travel = map_routing_citizen_can_travel_over_road_garden_highway(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                if (!can_travel) {
                    can_travel = map_routing_citizen_can_travel_over_land(f->x, f->y,
                        f->destination_x, f->destination_y, direction_limit);
                }
                break;
            case TERRAIN_USAGE_ROADS_HIGHWAY:
                can_travel = map_routing_citizen_can_travel_over_road_garden_highway(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                break;
            default:
                can_travel = map_routing_citizen_can_travel_over_land(f->x, f->y,
                    f->destination_x, f->destination_y, direction_limit);
                break;
        }
        if (can_travel) {
            if (f->terrain_usage == TERRAIN_USAGE_WALLS) {
                path_length = map_routing_get_path(path, f->destination_x, f->destination_y, 4);
                if (path_length <= 0) {
                    path_length = map_routing_get_path(path, f->destination_x, f->destination_y, direction_limit);
                }
            } else {
                path_length = map_routing_get_path(path, f->destination_x, f->destination_y, direction_limit);
            }
        } else { // cannot travel
            path_length = 0;
        }
    }
    if (path_length) {
        path->figure_id = f->id;
        f->routing_path_id = path->id;
        f->routing_path_length = path_length;
    }
}

void figure_route_remove(figure *f)
{
    if (f->routing_path_id > 0) {
        figure_path_data *path = array_item(paths, f->routing_path_id);
        if (path->figure_id == f->id) {
            path->figure_id = 0;
            free(path->directions);
            path->directions = 0;
            path->total_directions = 0;
            path->current_step = 0;
            path->same_direction_count = 0;
        }
        f->routing_path_id = 0;
    }
    array_trim(paths);
}

int figure_route_get_next_direction(int path_id)
{
    figure_path_data *path = array_item(paths, path_id);
    if (path->current_step >= path->total_directions) {
        return 8;
    }
    int direction = path->directions[path->current_step] >> ROUTING_PATH_DIRECTION_BIT_OFFSET;
    int tiles_in_direction = (path->directions[path->current_step] & ROUTING_PATH_DIRECTION_COUNT_BIT_MASK) + 1;

    path->same_direction_count++;
    if (path->same_direction_count >= tiles_in_direction) {
        path->current_step++;
        path->same_direction_count = 0;
    }

    return direction;
}

void figure_route_save_state(buffer *figures, buffer *buf_paths)
{
    size_t size = paths.size * sizeof(uint32_t);
    uint8_t *buf_data = malloc(size);
    buffer_init(figures, buf_data, size);

    size_t paths_memory_size = 0;

    figure_path_data *path;
    array_foreach(paths, path) {
        paths_memory_size += sizeof(uint32_t) + path->total_directions * sizeof(uint8_t);
    }

    size = sizeof(uint32_t) + paths_memory_size;
    buf_data = malloc(size);
    buffer_init(buf_paths, buf_data, size);
    buffer_write_u32(buf_paths, paths.size);

    array_foreach(paths, path) {
        buffer_write_u32(figures, path->figure_id);
        buffer_write_u32(buf_paths, path->total_directions);
        buffer_write_raw(buf_paths, path->directions, path->total_directions * sizeof(uint8_t));
    }
}

int convert_old_directions_to_new_format(figure_path_data *path, const uint8_t *directions)
{
    figure *f = figure_get(path->figure_id);

    // Invalid figure or no path
    if (!f || f->id == 0 || f->routing_path_length == 0) {
        return 1;
    }

    uint8_t new_directions[MAX_ORIGINAL_PATH_LENGTH] = { 0 };

    int current_direction = -1;
    unsigned int total_direction_changes = 0;
    uint8_t current_count = 0;

    for (unsigned int index = 0; index < f->routing_path_length; index++) {
        if (directions[index] != current_direction || current_count == ROUTING_PATH_DIRECTION_COUNT_BIT_MASK) {
            new_directions[total_direction_changes] = (directions[index] << ROUTING_PATH_DIRECTION_BIT_OFFSET);
            total_direction_changes++;
            current_direction = directions[index];
            current_count = 0;
        } else {
            new_directions[total_direction_changes - 1]++;
            current_count++;
        }
    }

    path->total_directions = total_direction_changes;
    path->directions = malloc(path->total_directions * sizeof(uint8_t));

    if (!path->directions) {
        log_error("Unable to allocate memory for routing path directions. The game will likely crash.", 0, 0);
        return 0;
    }

    memcpy(path->directions, new_directions, path->total_directions * sizeof(uint8_t));

    return 1;
}

static void update_current_tile(figure_path_data *path)
{
    figure *f = figure_get(path->figure_id);

    // Invalid figure or no path
    if (!f || f->id == 0 || f->routing_path_length == 0) {
        return;
    }

    unsigned int index = f->routing_path_current_tile + 1;

    for (unsigned int i = 0; i < path->total_directions; i++) {
        unsigned int tiles_in_direction = (path->directions[i] & ROUTING_PATH_DIRECTION_COUNT_BIT_MASK) + 1;
        if (tiles_in_direction >= index) {
            path->current_step = i;
            path->same_direction_count = index - 1;
            return;
        }
        index -= tiles_in_direction;
    }

    path->current_step = path->total_directions;
    path->same_direction_count = 0;
}


void figure_route_load_state(buffer *figures, buffer *buf_paths, int version)
{
    size_t elements_to_load;

    if (version <= SAVE_GAME_LAST_STATIC_PATHS_AND_ROUTES) {
        elements_to_load = buf_paths->size / MAX_ORIGINAL_PATH_LENGTH;
    } else {
        elements_to_load = buffer_read_u32(buf_paths);
    }

    if (!array_init(paths, ARRAY_SIZE_STEP, create_new_path, path_is_used) ||
        !array_expand(paths, elements_to_load)) {
        log_error("Unable to create paths array. The game will likely crash.", 0, 0);
        return;
    }

    unsigned int highest_id_in_use = 0;

    for (size_t i = 0; i < elements_to_load; i++) {
        figure_path_data *path = array_next(paths);
        if (version <= SAVE_GAME_LAST_STATIC_PATHS_AND_ROUTES) {
            path->figure_id = buffer_read_i16(figures);
            if (path->figure_id) {
                uint8_t directions[MAX_ORIGINAL_PATH_LENGTH];
                buffer_read_raw(buf_paths, directions, MAX_ORIGINAL_PATH_LENGTH);

                if (!convert_old_directions_to_new_format(path, directions)) {
                    log_error("Unable to convert old routing path directions. The game will likely crash.", 0, 0);
                    return;
                }
            } else {
                buffer_skip(buf_paths, MAX_ORIGINAL_PATH_LENGTH);
            }
        } else {
            path->figure_id = buffer_read_u32(figures);
            path->total_directions = buffer_read_u32(buf_paths);
            if (path->figure_id) {
                path->directions = malloc(path->total_directions * sizeof(uint8_t));
                if (!path->directions) {
                    log_error("Unable to allocate memory for routing path directions. The game will likely crash.", 0, 0);
                    return;
                }
                buffer_read_raw(buf_paths, path->directions, path->total_directions);
            } else {
                buffer_skip(buf_paths, path->total_directions);
            }
        }
        if (path->figure_id) {
            update_current_tile(path);
            highest_id_in_use = i;
        }
    }
    paths.size = highest_id_in_use + 1;
}
