#include "routing_path.h"

#include "core/calc.h"
#include "core/random.h"
#include "map/grid.h"
#include "map/random.h"
#include "map/routing.h"
#include "map/terrain.h"

#define PATH_SIZE_STEP 500
#define DIRECTION_BIT_OFFSET 5
#define DIRECTION_COUNT_MASK ((uint8_t) (1u << DIRECTION_BIT_OFFSET) - 1)

static struct {
    uint8_t *path;
    size_t total;
    size_t size;
} directions;

static void reset_directions(void)
{
    directions.total = 0;
}

static int increase_direction_path_size(void)
{
    if (directions.total >= directions.size) {
        size_t new_size = directions.size + PATH_SIZE_STEP;
        uint8_t *new_path = realloc(directions.path, new_size * sizeof(uint8_t));
        if (!new_path) {
            return 0;
        }
        directions.path = new_path;
        directions.size = new_size;
    }

    return 1;
}

static int add_direction_to_path(int direction)
{
    /**
     * How directions are stored:
     *
     * Since there are only 8 possible directions (0-7), each byte contains a direction
     * in the upper 3 bits (bits 5-7) and a count of how many times
     * this direction is repeated in the lower 5 bits (bits 0-4).
     *
     * For example, if a unit moves in direction 2 (right) for 10 tiles, this would be stored as:
     * Byte: 010 01001
     * where '010' is the binary representation of direction 2 and '01001' is the binary
     * representation of the count 9 (since we store count - 1, see below).
     *
     * Due to the 5 bits allocated for the count, the maximum number of consecutive moves
     * that can be stored in a single byte is 32 (and not 31 as might be expected).
     * Even though the count can represent values from 0 to 31,
     * we start counting from 1 (i.e., a count of 0 means 1 move, a count of 1 means 2 moves, etc.).
     *
     * If a unit moves more than 32 tiles in the same direction,
     * multiple bytes will be used for the same direction.
     *
     * This allows for efficient storage of paths with many consecutive moves in the same direction,
     * reducing the overall memory footprint of the path data.
     */
    if (directions.total > 0 &&
        direction == ((directions.path[directions.total - 1] >> DIRECTION_BIT_OFFSET) & 0x07) &&
        (directions.path[directions.total - 1] & DIRECTION_COUNT_MASK) < 31) {
        directions.path[directions.total - 1]++;
    } else {
        if (!increase_direction_path_size()) {
            return 0;
        }
        directions.total++;
        directions.path[directions.total - 1] = (direction << DIRECTION_BIT_OFFSET);
    }
    return 1;
}

static int fill_path_with_directions(figure_path_data *path)
{
    path->directions = malloc(directions.total * sizeof(uint8_t));
    if (!path->directions) {
        return 0;
    }
    for (int i = 0; i < directions.total; i++) {
        path->directions[i] = directions.path[directions.total - i - 1];
    }
    path->total_directions = directions.total;
    return 1;
}

static void adjust_tile_in_direction(int direction, int *x, int *y, int *grid_offset)
{
    switch (direction) {
        case DIR_0_TOP:
            -- *y;
            break;
        case DIR_1_TOP_RIGHT:
            ++ *x;
            -- *y;
            break;
        case DIR_2_RIGHT:
            ++ *x;
            break;
        case DIR_3_BOTTOM_RIGHT:
            ++ *x;
            ++ *y;
            break;
        case DIR_4_BOTTOM:
            ++ *y;
            break;
        case DIR_5_BOTTOM_LEFT:
            -- *x;
            ++ *y;
            break;
        case DIR_6_LEFT:
            -- *x;
            break;
        case DIR_7_TOP_LEFT:
            -- *x;
            -- *y;
            break;
    }
    *grid_offset += map_grid_direction_delta(direction);
}

static int is_equal_distance_but_better_direction(int distance, int next_distance, int direction, int next_direction)
{
    if (next_distance != distance) {
        return 0;
    } else if (direction == -1) {
        return 1;
    // prefer going in "straight" directions as opposed to diagonals if the distances are equal.
    // this helps prevent units from zig-zagging instead of moving in a straight line and makes
    // up for the removal of the general_direction calculation, which tended to make unit movement
    // look weird as units would try to move directly towards their destination even if there was
    // an obstacle in the way.
    } else if (direction % 2 == 1 && next_direction % 2 == 0) {
        return 1;
    }
    return 0;
}

static int next_is_better(
    int base_distance,
    int distance,
    int next_distance,
    int direction,
    int next_direction,
    int is_highway,
    int next_is_highway
)
{
    // always prefer highways so walkers don't cut across empty land
    if (!is_highway && next_is_highway && next_distance < base_distance) {
        return 1;
    } else if (is_highway && !next_is_highway) {
        return 0;
    } else if (next_distance < distance) {
        return 1;
    } else if (is_equal_distance_but_better_direction(distance, next_distance, direction, next_direction)) {
        return 1;
    }
    return 0;
}

int map_routing_get_path(figure_path_data *path, int dst_x, int dst_y, int num_directions)
{
    int dst_grid_offset = map_grid_offset(dst_x, dst_y);
    int distance = map_routing_distance(dst_grid_offset);
    if (distance <= 0) {
        return 0;
    }

    reset_directions();

    int num_tiles = 0;
    int last_direction = -1;
    int x = dst_x;
    int y = dst_y;
    int grid_offset = dst_grid_offset;
    int step = num_directions == 8 ? 1 : 2;

    while (distance > 1) {
        int base_distance = map_routing_distance(grid_offset);
        distance = base_distance;
        int direction = -1;
        int is_highway = 0;
        for (int next_direction = 0; next_direction < 8; next_direction += step) {
            if (next_direction != last_direction) {
                int next_offset = grid_offset + map_grid_direction_delta(next_direction);
                int next_distance = map_routing_distance(next_offset);
                int next_is_highway = map_terrain_is(next_offset, TERRAIN_HIGHWAY);
                if (next_distance && next_is_better(base_distance, distance, next_distance,
                    direction, next_direction, is_highway, next_is_highway)) {
                    distance = next_distance;
                    direction = next_direction;
                    is_highway = next_is_highway;
                }
            }
        }
        if (direction == -1) {
            return 0;
        }
        adjust_tile_in_direction(direction, &x, &y, &grid_offset);
        int forward_direction = (direction + 4) % 8;
        if (path && !add_direction_to_path(forward_direction)) {
            return 0;
        }
        last_direction = forward_direction;
        num_tiles++;
    }
    if (path && !fill_path_with_directions(path)) {
        return 0;
    }
    return num_tiles;
}

int map_routing_get_path_on_water(figure_path_data *path, int dst_x, int dst_y, int is_flotsam)
{
    int rand = random_byte() & 3;
    int dst_grid_offset = map_grid_offset(dst_x, dst_y);
    int distance = map_routing_distance(dst_grid_offset);
    if (distance <= 0) {
        return 0;
    }

    reset_directions();

    int num_tiles = 0;
    int last_direction = -1;
    int x = dst_x;
    int y = dst_y;
    int grid_offset = dst_grid_offset;
    while (distance > 1) {
        int current_rand = rand;
        distance = map_routing_distance(grid_offset);
        if (is_flotsam) {
            current_rand = map_random_get(grid_offset) & 3;
        }
        int direction = -1;
        for (int d = 0; d < 8; d++) {
            if (d != last_direction) {
                int next_offset = grid_offset + map_grid_direction_delta(d);
                int next_distance = map_routing_distance(next_offset);
                if (next_distance) {
                    if (next_distance < distance) {
                        distance = next_distance;
                        direction = d;
                    } else if (next_distance == distance && rand == current_rand) {
                        // allow flotsam to wander
                        distance = next_distance;
                        direction = d;
                    }
                }
            }
        }
        if (direction == -1) {
            return 0;
        }
        adjust_tile_in_direction(direction, &x, &y, &grid_offset);
        int forward_direction = (direction + 4) % 8;
        if (path && !add_direction_to_path(forward_direction)) {
            return 0;
        }
        last_direction = forward_direction;
        num_tiles++;
    }
    if (path && !fill_path_with_directions(path)) {
        return 0;
    }
    return num_tiles;
}
