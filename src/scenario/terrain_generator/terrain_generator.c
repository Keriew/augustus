#include "terrain_generator.h"

#include "terrain_generator_algorithms.h"s
#include "core/log.h"
#include "core/random.h"
#include "map/elevation.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/routing.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "scenario/editor_map.h"

#include <math.h>
#include <stdint.h>

static int use_fixed_seed = 0;
static unsigned int fixed_seed = 0;

static int terrain_tile_is_passable(int grid_offset) {
    return !map_terrain_is(grid_offset, TERRAIN_WATER | TERRAIN_ROCK);
}

int terrain_generator_random_between(int min_value, int max_value) {
    return random_between_from_stdlib(min_value, max_value);
}

static void clear_base_terrain(void) {
    int width = map_grid_width();
    int height = map_grid_height();
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            map_terrain_set(grid_offset, TERRAIN_CLEAR);
            map_elevation_set(grid_offset, 0);
        }
    }
}

static void choose_edge_point(int side, int width, int height, int *x, int *y) {
    switch (side) {
        case 0: // north
            *y = 0;
            *x = terrain_generator_random_between(1, width - 1);
            break;
        case 1: // south
            *y = height - 1;
            *x = terrain_generator_random_between(1, width - 1);
            break;
        case 2: // west
            *x = 0;
            *y = terrain_generator_random_between(1, height - 1);
            break;
        default: // east
            *x = width - 1;
            *y = terrain_generator_random_between(1, height - 1);
            break;
    }

    if (width > 2) {
        *x = calc_bound(*x, 0, width - 1);
    }
    if (height > 2) {
        *y = calc_bound(*y, 0, height - 1);
    }
}

static double point_distance_euclidean(int x1, int y1, int x2, int y2) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    return sqrt((double) (dx * dx + dy * dy));
}

typedef struct {
    int x;
    int y;
} point2i;

static int is_edge_tile(int x, int y, int width, int height) {
    return x == 0 || x == width - 1 || y == 0 || y == height - 1;
}

static int choose_two_random_interior_points(const uint16_t *segments, int width, int height, uint16_t segment_id,
                                             point2i *point1, point2i *point2) {
    int interior_count = 0;

    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {
            int offset = map_grid_offset(x, y);
            if (!terrain_tile_is_passable(offset) || segments[offset] != segment_id) {
                continue;
            }

            interior_count++;
            if (interior_count == 1) {
                point1->x = x;
                point1->y = y;
            } else if (interior_count == 2) {
                point2->x = x;
                point2->y = y;
            } else {
                int pick = terrain_generator_random_between(0, interior_count);
                if (pick == 0) {
                    point1->x = x;
                    point1->y = y;
                } else if (pick == 1) {
                    point2->x = x;
                    point2->y = y;
                }
            }
        }
    }

    return interior_count >= 2;
}

static int choose_random_edge_exit(const uint16_t *segments, int width, int height, uint16_t segment_id,
                                   point2i entry, double minimum_distance, point2i *exit_point) {
    int found_exit = 0;
    int exit_candidate_count = 0;

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if (!is_edge_tile(x, y, width, height) || (x == entry.x && y == entry.y)) {
                continue;
            }

            int offset = map_grid_offset(x, y);
            if (!terrain_tile_is_passable(offset) || segments[offset] != segment_id) {
                continue;
            }
            if (point_distance_euclidean(entry.x, entry.y, x, y) <= minimum_distance) {
                continue;
            }

            exit_candidate_count++;
            if (terrain_generator_random_between(0, exit_candidate_count) == 0) {
                exit_point->x = x;
                exit_point->y = y;
                found_exit = 1;
            }
        }
    }

    return found_exit;
}

int terrain_generator_flood_fill_reachable_land(int start_x, int start_y, uint8_t *reachable_land) {
    if (!reachable_land) {
        return 0;
    }

    map_grid_clear_u8(reachable_land);

    if (!map_grid_is_inside(start_x, start_y, 1)) {
        return 0;
    }

    int start_offset = map_grid_offset(start_x, start_y);
    if (!terrain_tile_is_passable(start_offset)) {
        return 0;
    }

    const int width = map_grid_width();
    const int height = map_grid_height();
    static const int dx[4] = {1, -1, 0, 0};
    static const int dy[4] = {0, 0, 1, -1};

    int queue[GRID_SIZE * GRID_SIZE];
    int queue_start = 0;
    int queue_end = 0;
    int reachable_count = 0;

    reachable_land[start_offset] = 1;
    queue[queue_end++] = start_offset;
    reachable_count = 1;

    while (queue_start < queue_end) {
        int current_offset = queue[queue_start++];
        int x = map_grid_offset_to_x(current_offset);
        int y = map_grid_offset_to_y(current_offset);

        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i];
            int ny = y + dy[i];

            if (nx < 0 || nx >= width || ny < 0 || ny >= height) {
                continue;
            }

            int next_offset = map_grid_offset(nx, ny);
            if (reachable_land[next_offset]) {
                continue;
            }
            if (!terrain_tile_is_passable(next_offset)) {
                continue;
            }

            reachable_land[next_offset] = 1;
            queue[queue_end++] = next_offset;
            reachable_count++;
        }
    }

    return reachable_count;
}

static void set_road_tile(int x, int y) {
    int grid_offset = map_grid_offset(x, y);
    map_terrain_remove(grid_offset,
                       TERRAIN_WATER | TERRAIN_TREE | TERRAIN_SHRUB | TERRAIN_ROCK | TERRAIN_MEADOW | TERRAIN_ELEVATION
                       | TERRAIN_ACCESS_RAMP);
    map_terrain_add(grid_offset, TERRAIN_ROAD);
    map_property_clear_constructing(grid_offset);
    map_property_set_multi_tile_size(grid_offset, 1);
    map_property_mark_draw_tile(grid_offset);
    map_tiles_update_area_roads(x, y, 3);
    map_tiles_update_region_empty_land(x - 1, y - 1, x + 1, y + 1);
}

static void add_road_between_points(int start_x, int start_y, int end_x, int end_y) {
    const uint16_t *segments = terrain_generator_segments();
    int start_offset = map_grid_offset(start_x, start_y);
    int end_offset = map_grid_offset(end_x, end_y);
    uint16_t road_segment_id = segments[start_offset];
    int path_offsets[GRID_SIZE * GRID_SIZE];
    int path_length = 0;
    int current_offset = end_offset;
    int guard = 0;
    static const int dir_x[4] = {1, -1, 0, 0};
    static const int dir_y[4] = {0, 0, 1, -1};

    if (!terrain_tile_is_passable(start_offset) || !terrain_tile_is_passable(end_offset)) {
        return;
    }
    if (road_segment_id == 0 || segments[end_offset] != road_segment_id) {
        return;
    }

    map_routing_update_land();
    if (!map_routing_calculate_distances_for_building(ROUTED_BUILDING_ROAD, start_x, start_y)) {
        return;
    }
    if (map_routing_distance(end_offset) <= 0) {
        return;
    }

    while (current_offset != start_offset && guard++ < GRID_SIZE * GRID_SIZE) {
        int cx = map_grid_offset_to_x(current_offset);
        int cy = map_grid_offset_to_y(current_offset);
        int current_distance = map_routing_distance(current_offset);
        int best_offset = -1;
        int best_distance = current_distance;

        if (path_length >= GRID_SIZE * GRID_SIZE) {
            return;
        }
        path_offsets[path_length++] = current_offset;

        for (int i = 0; i < 4; i++) {
            int nx = cx + dir_x[i];
            int ny = cy + dir_y[i];
            if (!map_grid_is_inside(nx, ny, 1)) {
                continue;
            }

            int next_offset = map_grid_offset(nx, ny);
            int next_distance = map_routing_distance(next_offset);
            if (next_distance <= 0 || next_distance >= best_distance) {
                continue;
            }
            if (!terrain_tile_is_passable(next_offset)) {
                continue;
            }
            if (segments[next_offset] != road_segment_id) {
                continue;
            }

            best_distance = next_distance;
            best_offset = next_offset;
        }

        if (best_offset < 0) {
            return;
        }
        current_offset = best_offset;
    }

    if (current_offset != start_offset) {
        return;
    }

    set_road_tile(start_x, start_y);
    for (int i = path_length - 1; i >= 0; i--) {
        int px = map_grid_offset_to_x(path_offsets[i]);
        int py = map_grid_offset_to_y(path_offsets[i]);
        set_road_tile(px, py);
    }
}

void set_entry_exit_points(void) {
    const int width = map_grid_width();
    const int height = map_grid_height();
    const uint16_t *segments = terrain_generator_segments();


    point2i entry = {0, 0};
    point2i point1 = {0, 0};
    point2i point2 = {0, 0};
    point2i exit_point = {0, 0};

    int has_fallback_entry = 0;
    point2i fallback_entry = {0, 0};

    int found_full_route = 0;
    const int max_attempts = width * height;

    for (int attempt = 0; attempt < max_attempts && !found_full_route; attempt++) {
        int entry_side = terrain_generator_random_between(0, 4);
        choose_edge_point(entry_side, width, height, &entry.x, &entry.y);

        int entry_offset = map_grid_offset(entry.x, entry.y);
        if (!terrain_tile_is_passable(entry_offset)) {
            continue;
        }

        uint16_t entry_segment_id = segments[entry_offset];
        if (entry_segment_id == 0) {
            continue;
        }

        has_fallback_entry = 1;
        fallback_entry = entry;

        if (!choose_two_random_interior_points(segments, width, height, entry_segment_id, &point1, &point2)) {
            continue;
        }

        const double entry_to_point1 = point_distance_euclidean(entry.x, entry.y, point1.x, point1.y);

        if (choose_random_edge_exit(segments, width, height, entry_segment_id, entry, entry_to_point1, &exit_point)) {
            found_full_route = 1;
        }
    }

    if (!found_full_route) {
        if (has_fallback_entry) {
            entry = fallback_entry;
        }
        exit_point = entry;
        point1 = entry;
        point2 = entry;
    }

    scenario_editor_set_entry_point(entry.x, entry.y);
    scenario_editor_set_exit_point(exit_point.x, exit_point.y);

    double route_entry_p1_p2_exit =
            point_distance_euclidean(entry.x, entry.y, point1.x, point1.y) +
            point_distance_euclidean(point1.x, point1.y, point2.x, point2.y) +
            point_distance_euclidean(point2.x, point2.y, exit_point.x, exit_point.y);

    double route_entry_p2_p1_exit =
            point_distance_euclidean(entry.x, entry.y, point2.x, point2.y) +
            point_distance_euclidean(point2.x, point2.y, point1.x, point1.y) +
            point_distance_euclidean(point1.x, point1.y, exit_point.x, exit_point.y);

    if (route_entry_p2_p1_exit < route_entry_p1_p2_exit) {
        point2i tmp = point1;
        point1 = point2;
        point2 = tmp;

        route_entry_p1_p2_exit = route_entry_p2_p1_exit;
    }

    double route_entry_p1_exit =
            point_distance_euclidean(entry.x, entry.y, point1.x, point1.y) +
            point_distance_euclidean(point1.x, point1.y, exit_point.x, exit_point.y);

    if (route_entry_p1_p2_exit < route_entry_p1_exit) {
        add_road_between_points(entry.x, entry.y, point1.x, point1.y);
        add_road_between_points(point1.x, point1.y, point2.x, point2.y);
        add_road_between_points(point2.x, point2.y, exit_point.x, exit_point.y);
    } else {
        add_road_between_points(entry.x, entry.y, point1.x, point1.y);
        add_road_between_points(point1.x, point1.y, exit_point.x, exit_point.y);
    }

    // map_terrain_set(map_grid_offset(point1.x, point1.y), TERRAIN_AQUEDUCT);
    // map_terrain_set(map_grid_offset(point2.x, point2.y), TERRAIN_GARDEN);
}

void terrain_generator_generate(terrain_generator_algorithm algorithm) {
    if (use_fixed_seed) {
        random_set_stdlib_seed(fixed_seed);
    } else {
        fixed_seed = terrain_generator_random_between(1, 0x7fffffff);
        use_fixed_seed = 1;
        random_set_stdlib_seed(fixed_seed);
    }

    clear_base_terrain();

    switch (algorithm) {
        case TERRAIN_GENERATOR_RIVER:
            log_info("Starting river generator", 0, 1);
            terrain_generator_river_map(fixed_seed);
            // There is a bug where the road is not rendered in the minimap if this function is called inside terrain_generator_river_map
            // set_entry_exit_points();

            break;
        case TERRAIN_GENERATOR_RANDOM:
        default:
            terrain_generator_random_terrain();
            set_entry_exit_points();
            break;
    }

    random_clear_stdlib_seed();
}

void terrain_generator_set_seed(int enabled, unsigned int seed) {
    use_fixed_seed = enabled != 0;
    fixed_seed = seed;
}
