#include "terrain_generator_algorithms.h"

#include "map/elevation.h"
#include "map/grid.h"
#include "map/terrain.h"

enum {
    SIDE_NORTH = 0,
    SIDE_SOUTH = 1,
    SIDE_WEST = 2,
    SIDE_EAST = 3,
    DIR_COUNT = 8,
};

static const int DIR_X[DIR_COUNT] = { 0, 1, 1, 1, 0, -1, -1, -1 };
static const int DIR_Y[DIR_COUNT] = { -1, -1, 0, 1, 1, 1, 0, -1 };

static int abs_int(int value)
{
    return (value < 0) ? -value : value;
}

static int sign_int(int value)
{
    return (value > 0) - (value < 0);
}

static int tile_is_on_side(int x, int y, int width, int height, int side)
{
    switch (side) {
        case SIDE_NORTH:
            return y == 0;
        case SIDE_SOUTH:
            return y == height - 1;
        case SIDE_WEST:
            return x == 0;
        default:
            return x == width - 1;
    }
}

static void choose_edge_point(int side, int width, int height, int *x, int *y)
{
    switch (side) {
        case SIDE_NORTH:
            *x = terrain_generator_random_between(0, width);
            *y = 0;
            break;
        case SIDE_SOUTH:
            *x = terrain_generator_random_between(0, width);
            *y = height - 1;
            break;
        case SIDE_WEST:
            *x = 0;
            *y = terrain_generator_random_between(0, height);
            break;
        default:
            *x = width - 1;
            *y = terrain_generator_random_between(0, height);
            break;
    }
}

static int direction_moves_away_from_side(int dir, int side)
{
    switch (side) {
        case SIDE_NORTH:
            return DIR_Y[dir] > 0;
        case SIDE_SOUTH:
            return DIR_Y[dir] < 0;
        case SIDE_WEST:
            return DIR_X[dir] > 0;
        default:
            return DIR_X[dir] < 0;
    }
}

static int direction_moves_toward_side(int dir, int side)
{
    switch (side) {
        case SIDE_NORTH:
            return DIR_Y[dir] < 0;
        case SIDE_SOUTH:
            return DIR_Y[dir] > 0;
        case SIDE_WEST:
            return DIR_X[dir] < 0;
        default:
            return DIR_X[dir] > 0;
    }
}

static void carve_river_tile(int x, int y)
{
    int grid_offset = map_grid_offset(x, y);
    map_terrain_set_with_tile_update(grid_offset, TERRAIN_WATER);
    map_elevation_set(grid_offset, 0);
}

static void carve_river_tile_brush(int x, int y, int r)
{
    if (r <= 0) {
        carve_river_tile(x, y);
        return;
    }

    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if ((dx * dx) + (dy * dy) > (r * r)) {
                continue;
            }

            int nx = x + dx;
            int ny = y + dy;
            if (!map_grid_is_inside(nx, ny, 1)) {
                continue;
            }

            carve_river_tile(nx, ny);
        }
    }
}

typedef struct {
    int x;
    int y;
} river_tile;

static river_tile generated_river_tiles[GRID_SIZE * GRID_SIZE];
static int generated_river_tile_count = 0;

static void record_river_tile(int x, int y)
{
    if (generated_river_tile_count <= 0) {
        generated_river_tiles[generated_river_tile_count].x = x;
        generated_river_tiles[generated_river_tile_count].y = y;
        generated_river_tile_count++;
        return;
    }

    river_tile *last = &generated_river_tiles[generated_river_tile_count - 1];
    if (last->x == x && last->y == y) {
        return;
    }

    if (generated_river_tile_count >= GRID_SIZE * GRID_SIZE) {
        return;
    }

    generated_river_tiles[generated_river_tile_count].x = x;
    generated_river_tiles[generated_river_tile_count].y = y;
    generated_river_tile_count++;
}



static void replay_river_tiles(void)
{
    const int river_min_radius = 1;
    const int river_max_radius = 10;
    int current_river_radius = river_min_radius;

    for (int i = 0; i < generated_river_tile_count; i++) {
        // Adjust the river_radius by randomly increasing or decreasing it by 1 step
        current_river_radius += terrain_generator_random_between(-1, 2);
        if (current_river_radius < river_min_radius) {
            current_river_radius = river_min_radius;
        } else if (current_river_radius > river_max_radius) {
            current_river_radius = river_max_radius;
        }

        carve_river_tile_brush(generated_river_tiles[i].x, generated_river_tiles[i].y, current_river_radius);
    }
}

static int choose_next_direction(
    int x,
    int y,
    int target_x,
    int target_y,
    int previous_dir,
    int phase,
    int start_side,
    int end_side,
    int width,
    int height,
    int step,
    int min_exit_steps)
{
    int total_weight = 0;
    int weights[DIR_COUNT] = { 0 };

    int target_step_x = sign_int(target_x - x);
    int target_step_y = sign_int(target_y - y);

    for (int dir = 0; dir < DIR_COUNT; dir++) {
        int nx = x + DIR_X[dir];
        int ny = y + DIR_Y[dir];
        if (!map_grid_is_inside(nx, ny, 1)) {
            continue;
        }

        int weight = 1;

        if (previous_dir >= 0) {
            int delta = dir - previous_dir;
            if (delta < 0) {
                delta = -delta;
            }
            if (delta > DIR_COUNT / 2) {
                delta = DIR_COUNT - delta;
            }

            if (delta == 4) {
                continue;
            } else if (delta == 0) {
                weight += 8;
            } else if (delta == 1) {
                weight += 3;
            } else if (delta == 2) {
                weight += 1;
            }
        }

        int alignment = DIR_X[dir] * target_step_x + DIR_Y[dir] * target_step_y;
        if (alignment >= 2) {
            weight += (phase == 0) ? 5 : 4;
        } else if (alignment == 1) {
            weight += 2;
        }

        if (phase == 0 && direction_moves_away_from_side(dir, start_side)) {
            weight += 3;
        }
        if (phase == 1 && direction_moves_toward_side(dir, end_side)) {
            weight += 2;
        }

        if (step < min_exit_steps && tile_is_on_side(nx, ny, width, height, start_side)) {
            weight -= 4;
        }

        weight += terrain_generator_random_between(0, 3);

        if (weight <= 0) {
            continue;
        }

        weights[dir] = weight;
        total_weight += weight;
    }

    if (total_weight <= 0) {
        return -1;
    }

    int pick = terrain_generator_random_between(0, total_weight);
    for (int dir = 0; dir < DIR_COUNT; dir++) {
        if (weights[dir] <= 0) {
            continue;
        }
        if (pick < weights[dir]) {
            return dir;
        }
        pick -= weights[dir];
    }

    return -1;
}



void terrain_generator_straight_river(void)
{
    int width = map_grid_width();
    int height = map_grid_height();
    if (width <= 1 || height <= 1) {
        return;
    }

    generated_river_tile_count = 0;

    int start_side = terrain_generator_random_between(0, 4);
    int x = 0;
    int y = 0;
    choose_edge_point(start_side, width, height, &x, &y);

    int end_x = x;
    int end_y = y;
    switch (start_side) {
        case SIDE_NORTH:
            end_y = height - 1;
            break;
        case SIDE_SOUTH:
            end_y = 0;
            break;
        case SIDE_WEST:
            end_x = width - 1;
            break;
        default:
            end_x = 0;
            break;
    }

    int step_x = sign_int(end_x - x);
    int step_y = sign_int(end_y - y);
    int guard = 0;
    int max_steps = width + height + 4;

    while ((x != end_x || y != end_y) && guard++ < max_steps) {
        record_river_tile(x, y);
        x += step_x;
        y += step_y;
    }

    record_river_tile(end_x, end_y);
    replay_river_tiles();
}

void terrain_generator_generate_river(void)
{
    int width = map_grid_width();
    int height = map_grid_height();
    if (width <= 1 || height <= 1) {
        return;
    }

    generated_river_tile_count = 0;

    int start_side = terrain_generator_random_between(0, 4);
    int end_side = terrain_generator_random_between(0, 4);
    while (end_side == start_side) {
        end_side = terrain_generator_random_between(0, 4);
    }

    int x = 0;
    int y = 0;
    int end_x = 0;
    int end_y = 0;
    choose_edge_point(start_side, width, height, &x, &y);
    choose_edge_point(end_side, width, height, &end_x, &end_y);

    int center_x = width / 2;
    int center_y = height / 2;
    int center_radius = terrain_generator_clamp_int((width + height) / 16, 4, 12);
    int min_exit_steps = terrain_generator_clamp_int((width + height) / 6, 8, 80);

    int previous_dir = -1;
    int phase = 0;
    int max_steps = width * height;

    for (int step = 0; step < max_steps; step++) {
        record_river_tile(x, y);

        if (phase == 0) {
            int distance_to_center = abs_int(x - center_x) + abs_int(y - center_y);
            if (distance_to_center <= center_radius) {
                phase = 1;
            }
        }

        if (phase == 1 && step >= min_exit_steps && tile_is_on_side(x, y, width, height, end_side)) {
            break;
        }

        int target_x = (phase == 0) ? center_x : end_x;
        int target_y = (phase == 0) ? center_y : end_y;
        int dir = choose_next_direction(
            x,
            y,
            target_x,
            target_y,
            previous_dir,
            phase,
            start_side,
            end_side,
            width,
            height,
            step,
            min_exit_steps);
        if (dir < 0) {
            break;
        }

        x += DIR_X[dir];
        y += DIR_Y[dir];
        previous_dir = dir;
    }

    for (int guard = 0; guard < width + height; guard++) {
        if (tile_is_on_side(x, y, width, height, end_side)) {
            break;
        }

        record_river_tile(x, y);

        int step_x = sign_int(end_x - x);
        int step_y = sign_int(end_y - y);
        if (abs_int(end_x - x) >= abs_int(end_y - y)) {
            x += step_x;
        } else {
            y += step_y;
        }
        x = terrain_generator_clamp_int(x, 0, width - 1);
        y = terrain_generator_clamp_int(y, 0, height - 1);
    }

    record_river_tile(x, y);

    replay_river_tiles();
}
