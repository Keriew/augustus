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
    static int river_y[GRID_SIZE];
    int river_half_width = 1;

    river_y[0] = height / 2;
    for (int x = 1; x < width; x++) {
        int drift = random_between(-1, 2);
        river_y[x] = clamp_int(river_y[x - 1] + drift, 1, height - 2);
    }

    for (int x = 0; x < width; x++) {
        for (int dy = -river_half_width; dy <= river_half_width; dy++) {
            int y = river_y[x] + dy;
            if (y < 0 || y >= height) {
                continue;
            }
            map_terrain_set(map_grid_offset(x, y), TERRAIN_WATER);
            map_elevation_set(map_grid_offset(x, y), 0);
        }
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (map_terrain_is(grid_offset, TERRAIN_WATER)) {
                continue;
            }
            int dist = abs(y - river_y[x]);
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
