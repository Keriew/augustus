#include "terrain_generator.h"

#include "core/random.h"
#include "map/elevation.h"
#include "map/grid.h"
#include "map/terrain.h"

#include <stdlib.h>

static int clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int random_between(int min_value, int max_value)
{
    return random_between_from_stdlib(min_value, max_value);
}

static int use_fixed_seed = 0;
static unsigned int fixed_seed = 0;

static void clear_base_terrain(void)
{
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

static void generate_flat_plains(void)
{
    int width = map_grid_width();
    int height = map_grid_height();

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            int roll = random_between(0, 100);
            if (roll < 6) {
                map_terrain_set(grid_offset, TERRAIN_TREE);
            } else if (roll < 8) {
                map_terrain_set(grid_offset, TERRAIN_ROCK);
            } else if (roll < 18) {
                map_terrain_set(grid_offset, TERRAIN_SHRUB);
            } else if (roll < 35) {
                map_terrain_set(grid_offset, TERRAIN_MEADOW);
            }
        }
    }
}

static void generate_river_valley(void)
{
    int width = map_grid_width();
    int height = map_grid_height();
    int river_half_width = 1;
    int guard = 0;
    int max_steps = width * height * 2;
    int start_side = random_between(0, 4);
    int end_side = random_between(0, 4);
    while (end_side == start_side) {
        end_side = random_between(0, 4);
    }

    int start_x = 0;
    int start_y = 0;
    int end_x = 0;
    int end_y = 0;

    switch (start_side) {
        case 0: // north
            start_y = 0;
            start_x = random_between(1, width - 1);
            break;
        case 1: // south
            start_y = height - 1;
            start_x = random_between(1, width - 1);
            break;
        case 2: // west
            start_x = 0;
            start_y = random_between(1, height - 1);
            break;
        default: // east
            start_x = width - 1;
            start_y = random_between(1, height - 1);
            break;
    }

    switch (end_side) {
        case 0: // north
            end_y = 0;
            end_x = random_between(1, width - 1);
            break;
        case 1: // south
            end_y = height - 1;
            end_x = random_between(1, width - 1);
            break;
        case 2: // west
            end_x = 0;
            end_y = random_between(1, height - 1);
            break;
        default: // east
            end_x = width - 1;
            end_y = random_between(1, height - 1);
            break;
    }

    if (width > 2) {
        start_x = clamp_int(start_x, 0, width - 1);
        end_x = clamp_int(end_x, 0, width - 1);
    }
    if (height > 2) {
        start_y = clamp_int(start_y, 0, height - 1);
        end_y = clamp_int(end_y, 0, height - 1);
    }

    typedef struct {
        int x;
        int y;
    } river_point;

    static river_point river_points[GRID_SIZE * GRID_SIZE];
    int river_count = 0;

    int x = start_x;
    int y = start_y;
    while ((x != end_x || y != end_y) && guard++ < max_steps) {
        int dx = 0;
        int dy = 0;
        if (end_x > x) {
            dx = 1;
        } else if (end_x < x) {
            dx = -1;
        }
        if (end_y > y) {
            dy = 1;
        } else if (end_y < y) {
            dy = -1;
        }

        int step_x = 0;
        int step_y = 0;
        int meander_roll = random_between(0, 100);
        if (dx != 0 && dy != 0) {
            if (meander_roll < 35) {
                step_x = dx;
            } else if (meander_roll < 70) {
                step_y = dy;
            } else if (meander_roll < 85) {
                step_x = 0;
                step_y = dy;
            } else {
                step_x = dx;
                step_y = 0;
            }
        } else if (dx != 0) {
            if (meander_roll < 20) {
                step_y = random_between(0, 2) ? 1 : -1;
            } else {
                step_x = dx;
            }
        } else if (dy != 0) {
            if (meander_roll < 20) {
                step_x = random_between(0, 2) ? 1 : -1;
            } else {
                step_y = dy;
            }
        }

        int next_x = clamp_int(x + step_x, 0, width - 1);
        int next_y = clamp_int(y + step_y, 0, height - 1);
        if (next_x == x && next_y == y) {
            if (dx != 0) {
                next_x = clamp_int(x + dx, 0, width - 1);
            }
            if (dy != 0) {
                next_y = clamp_int(y + dy, 0, height - 1);
            }
        }

        x = next_x;
        y = next_y;
        if (river_count < (GRID_SIZE * GRID_SIZE)) {
            river_points[river_count].x = x;
            river_points[river_count].y = y;
            river_count++;
        }
    }

    if (river_count == 0) {
        river_points[river_count].x = start_x;
        river_points[river_count].y = start_y;
        river_count++;
    }

    for (int i = 0; i < river_count; i++) {
        int rx = river_points[i].x;
        int ry = river_points[i].y;
        for (int dy = -river_half_width; dy <= river_half_width; dy++) {
            for (int dx = -river_half_width; dx <= river_half_width; dx++) {
                int wx = rx + dx;
                int wy = ry + dy;
                if (wx < 0 || wx >= width || wy < 0 || wy >= height) {
                    continue;
                }
                int grid_offset = map_grid_offset(wx, wy);
                map_terrain_set(grid_offset, TERRAIN_WATER);
                map_elevation_set(grid_offset, 0);
            }
        }
    }

    for (int yy = 0; yy < height; yy++) {
        for (int xx = 0; xx < width; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (map_terrain_is(grid_offset, TERRAIN_WATER)) {
                continue;
            }
            int dist = width + height;
            for (int i = 0; i < river_count; i++) {
                int d = abs(xx - river_points[i].x) + abs(yy - river_points[i].y);
                if (d < dist) {
                    dist = d;
                    if (dist <= 1) {
                        break;
                    }
                }
            }
            int elevation = 0;
            if (dist > 2) {
                elevation = (dist - 2) / 6;
            }
            if (elevation > 0 && random_between(0, 4) == 0) {
                elevation++;
            }
            elevation = clamp_int(elevation, 0, 4);
            map_elevation_set(grid_offset, elevation);

            int roll = random_between(0, 100);
            if (dist <= 2 && roll < 30) {
                map_terrain_set(grid_offset, TERRAIN_MEADOW);
            } else if (elevation >= 2 && roll < 12) {
                map_terrain_set(grid_offset, TERRAIN_ROCK);
            } else if (roll < 8) {
                map_terrain_set(grid_offset, TERRAIN_TREE);
            } else if (roll < 16) {
                map_terrain_set(grid_offset, TERRAIN_SHRUB);
            }
        }
    }
}

void terrain_generator_generate(terrain_generator_algorithm algorithm)
{
    if (use_fixed_seed) {
        random_set_stdlib_seed(fixed_seed);
    } else {
        random_clear_stdlib_seed();
    }
    clear_base_terrain();

    switch (algorithm) {
        case TERRAIN_GENERATOR_RIVER_VALLEY:
            generate_river_valley();
            break;
        case TERRAIN_GENERATOR_FLAT_PLAINS:
        default:
            generate_flat_plains();
            break;
    }

    random_clear_stdlib_seed();
}

void terrain_generator_set_seed(int enabled, unsigned int seed)
{
    use_fixed_seed = enabled != 0;
    fixed_seed = seed;
}
