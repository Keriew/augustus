#include "terrain_generator_algorithms.h"

#include "map/elevation.h"
#include "map/grid.h"
#include "map/terrain.h"

#include <math.h>
#include <stdint.h>

static unsigned int perlin_seed = 1;
static unsigned int mountain_seed = 1;
static unsigned int meadow_seed = 1;

static uint32_t hash_2d(int x, int y, unsigned int seed)
{
    uint32_t h = (uint32_t) x;
    h ^= (uint32_t) y * 374761393u;
    h ^= seed * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float hash_value(int x, int y, unsigned int seed)
{
    return (hash_2d(x, y, seed) & 0xffffu) / 65535.0f;
}

static float fade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float value_noise(float x, float y, unsigned int seed)
{
    int x0 = (int) floorf(x);
    int y0 = (int) floorf(y);
    int x1 = x0 + 1;
    int y1 = y0 + 1;

    float sx = fade(x - (float) x0);
    float sy = fade(y - (float) y0);

    float n00 = hash_value(x0, y0, seed);
    float n10 = hash_value(x1, y0, seed);
    float n01 = hash_value(x0, y1, seed);
    float n11 = hash_value(x1, y1, seed);

    float ix0 = lerp(n00, n10, sx);
    float ix1 = lerp(n01, n11, sx);
    return lerp(ix0, ix1, sy);
}

static float fbm(float x, float y, unsigned int seed)
{
    float sum = 0.0f;
    float amp = 0.5f;
    float freq = 0.02f;
    float norm = 0.0f;

    for (int i = 0; i < 4; i++) {
        sum += amp * value_noise(x * freq, y * freq, seed + (unsigned int) i * 101u);
        norm += amp;
        amp *= 0.5f;
        freq *= 2.0f;
    }

    if (norm > 0.0f) {
        sum /= norm;
    }
    return sum;
}

static void generate_grassland(void)
{
    int width = map_grid_width();
    int height = map_grid_height();

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            map_terrain_set_with_tile_update(grid_offset, TERRAIN_CLEAR);
            map_elevation_set(grid_offset, 0);
        }
    }
}

static void add_forests(void)
{
    int width = map_grid_width();
    int height = map_grid_height();

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (map_terrain_get(grid_offset) != TERRAIN_CLEAR) {
                continue;
            }

            float n = fbm((float) x, (float) y, perlin_seed);
            if (n > 0.58f) {
                map_terrain_set_with_tile_update(grid_offset, TERRAIN_TREE);
            }
        }
    }
}

static void add_mountains(void)
{
    int width = map_grid_width();
    int height = map_grid_height();

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (map_terrain_get(grid_offset) != TERRAIN_CLEAR) {
                continue;
            }
            float n = fbm((float) x + 1000.0f, (float) y + 1000.0f, mountain_seed);
            if (n > 0.68f) {
                // int elevation = 2 + (int) ((n - 0.68f) * 10.0f);
                // elevation = terrain_generator_clamp_int(elevation, 2, 4);
                // map_elevation_set(map_grid_offset(x, y), elevation);
                map_terrain_set_with_tile_update(map_grid_offset(x, y), TERRAIN_ROCK);
            }
        }
    }
}

static void add_meadows(void)
{
    int width = map_grid_width();
    int height = map_grid_height();

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            if (map_terrain_get(grid_offset) != TERRAIN_CLEAR) {
                continue;
            }
            if (map_elevation_at(grid_offset) > 0) {
                continue;
            }
            if (map_terrain_is(grid_offset, TERRAIN_TREE | TERRAIN_ROCK | TERRAIN_WATER)) {
                continue;
            }
            float n = fbm((float) x + 2500.0f, (float) y + 2500.0f, meadow_seed);
            if (n > 0.52f && n < 0.65f) {
                map_terrain_set_with_tile_update(grid_offset, TERRAIN_MEADOW);
            }
        }
    }
}


static void add_lakes(void)
{
    int width = map_grid_width();
    int height = map_grid_height();
    int area = width * height;

    int lake_count = 1 + area / 8000;
    if (lake_count < 1) {
        lake_count = 1;
    } else if (lake_count > 6) {
        lake_count = 6;
    }

    for (int i = 0; i < lake_count; i++) {
        int radius = terrain_generator_random_between(2, 6);
        int center_x = terrain_generator_random_between(radius + 2, width - radius - 3);
        int center_y = terrain_generator_random_between(radius + 2, height - radius - 3);
        unsigned int shape_seed = perlin_seed + 3001u + (unsigned int) i * 97u;

        for (int dy = -radius - 2; dy <= radius + 2; dy++) {
            for (int dx = -radius - 2; dx <= radius + 2; dx++) {
                int x = center_x + dx;
                int y = center_y + dy;
                if (!map_grid_is_inside(x, y, 1)) {
                    continue;
                }

                float dist = sqrtf((float) (dx * dx + dy * dy));
                float n = fbm((float) x * 0.35f, (float) y * 0.35f, shape_seed);
                float threshold = (float) radius * (0.75f + 0.55f * n);
                if (dist <= threshold) {
                    int grid_offset = map_grid_offset(x, y);
                    map_terrain_set_with_tile_update(grid_offset, TERRAIN_WATER);
                    map_elevation_set(grid_offset, 0);
                }
            }
        }
    }
}

static void add_sea_edge(void)
{
    int width = map_grid_width();
    int height = map_grid_height();
    int side = terrain_generator_random_between(0, 4);
    int thickness = terrain_generator_random_between(3, 6);

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int is_sea = 0;
            if (side == 0 && y < thickness) {
                is_sea = 1;
            } else if (side == 1 && y >= height - thickness) {
                is_sea = 1;
            } else if (side == 2 && x < thickness) {
                is_sea = 1;
            } else if (side == 3 && x >= width - thickness) {
                is_sea = 1;
            }
            if (is_sea) {
                int grid_offset = map_grid_offset(x, y);
                map_terrain_set_with_tile_update(grid_offset, TERRAIN_WATER);
                map_elevation_set(grid_offset, 0);
            }
        }
    }
}

void terrain_generator_generate_perlin(unsigned int seed)
{


    perlin_seed = seed;
    mountain_seed = seed;
    meadow_seed = seed;

    generate_grassland();

    terrain_generator_generate_river();
    // terrain_generator_straight_river();
    add_forests();
    add_mountains();
    add_meadows();
    // add_lakes();
    // add_sea_edge();
}
