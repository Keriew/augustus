#include "terrain_generator_algorithms.h"

#include "map/elevation.h"
#include "map/grid.h"
#include "map/terrain.h"
#include <math.h>
#include <stdint.h>

#include "simplex_noise.h"
#include "terrain_generator.h"
#include "core/log.h"
#include "core/random.h"

static unsigned int perlin_seed = 1;
static unsigned int mountain_seed = 1;
static unsigned int meadow_seed = 1;

// Resets the map to flat grassland as the base layer for procedural passes.
static void generate_grassland(void) {
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

static void paint_according_to_segments(void) {
    /*  This is a debug function.
     *  Loop over the grid and for every cell lookup what the segment_id is.
     *  If it is 0, do nothing
     *  If the segment_id is bigger than 0, use the following mapping:
     *  1 = meadows
     *  2 = forests
     *  3 = mountains
     *  4 = shrub
     *  5 = rubble
     *  6 and bigger, do nothing
     */
    const uint16_t *segments = terrain_generator_segments();
    int width = map_grid_width();
    int height = map_grid_height();

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            uint16_t segment_id = segments[grid_offset];

            switch (segment_id) {
                case 1:
                    map_terrain_set_with_tile_update(grid_offset, TERRAIN_MEADOW);
                    break;
                case 2:
                    map_terrain_set_with_tile_update(grid_offset, TERRAIN_TREE);
                    break;
                case 3:
                    map_terrain_set_with_tile_update(grid_offset, TERRAIN_ROCK);
                    break;
                case 4:
                    map_terrain_set_with_tile_update(grid_offset, TERRAIN_SHRUB);
                    break;
                case 5:
                    map_terrain_set_with_tile_update(grid_offset, TERRAIN_RUBBLE);
                    break;
                default:
                    break;
            }
        }
    }
}

void setTerrainControlled(double e,
                          double d,
                          int grid_offset,
                          double fertility, // 0.0 -> barren, 1.0 -> lush
                          double roughness, // 0.0 -> flat,   1.0 -> mountainous
                          double openness, int allowed_terrain) // 0.0 -> dense vegetation, 1.0 -> open grassland
{
    double rock_threshold = 0.85 - (roughness * 0.35); // More roughness -> more mountains
    double meadow_threshold = 0.75 - (fertility * 0.25); // More fertility -> more meadows
    double tree_threshold = 0.60 + (openness * 0.25); // More openness -> fewer trees
    double shrub_threshold = 0.45 + (openness * 0.15); // Shrubs become rarer with openness
    double water_threshold = 0.40 - (roughness * 0.20); // More roughness -> fewer lakes

    if (e + (d * 0.30) < water_threshold) {
        map_terrain_set_with_tile_update(grid_offset, TERRAIN_WATER);
        return;
    }

    // Mountains
    if (allowed_terrain & TERRAIN_ROCK && e > rock_threshold) {
        map_terrain_set_with_tile_update(grid_offset, TERRAIN_ROCK);
        return;
    }

    // Vegetation zone
    if (e > 0.40) {
        if (allowed_terrain & TERRAIN_TREE && d > tree_threshold && openness < 0.9) {
            // Dense vegetation
            map_terrain_set_with_tile_update(grid_offset, TERRAIN_TREE);
            return;
        }
        if (allowed_terrain & TERRAIN_MEADOW && d > meadow_threshold) {
            // Meadows
            map_terrain_set_with_tile_update(grid_offset, TERRAIN_MEADOW);
            return;
        }
        if (allowed_terrain & TERRAIN_SHRUB && d > shrub_threshold && openness < 0.95) {
            // Sparse vegetation
            map_terrain_set_with_tile_update(grid_offset, TERRAIN_SHRUB);
            return;
        }
        map_terrain_set_with_tile_update(grid_offset, TERRAIN_CLEAR);
        return;
    }

    // Low elevation
    if (allowed_terrain & TERRAIN_MEADOW && d > meadow_threshold + 0.1 && openness < 0.8) {
        // Fertile plains -> meadows
        map_terrain_set_with_tile_update(grid_offset, TERRAIN_MEADOW);
        return;
    }
    if (allowed_terrain & TERRAIN_SHRUB && d > shrub_threshold + 0.05 && openness < 0.7) {
        // Some shrubs in non-open areas
        map_terrain_set_with_tile_update(grid_offset, TERRAIN_SHRUB);
        return;
    }
    map_terrain_set_with_tile_update(grid_offset, TERRAIN_CLEAR);
}

void paint_by_noise(SimplexNoise *noise, double fertility, double roughness, double openness, int allowed_terrain) {
    double elevation_scale = 0.05; // controls smoothness
    double detail_scale = 0.08;
    int width = map_grid_width();
    int height = map_grid_height();
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            double elevation = simplex2D_octaves(noise, x * elevation_scale, y * elevation_scale, 5, 0.5, 2.0);
            double detail = simplex2D_octaves(noise, x * detail_scale + 100, y * detail_scale + 100, 4, 0.5, 2.0);

            double e = (elevation + 1.0) * 0.5;
            double d = (detail + 1.0) * 0.5;
            int grid_offset = map_grid_offset(x, y);
            if (map_terrain_is(grid_offset, TERRAIN_WATER | TERRAIN_ROAD)) {
                continue; // Preserve river tiles
            }
            setTerrainControlled(e, d, grid_offset,
                                 fertility, roughness, openness, allowed_terrain);
        }
    }
}

// Builds a complete terrain pass from base layer through river and biome overlays.
void terrain_generator_river_map(unsigned int seed) {
    perlin_seed = seed;
    mountain_seed = seed;
    meadow_seed = seed;

    generate_grassland();
    terrain_generator_generate_river();

    double rand_val = random_fractional_from_stdlib();
    // Generate extra rivers with 50% chance
    if (rand_val < 0.5) {
        terrain_generator_generate_river();
    }

    SimplexNoise noise;
    initSimplex(&noise, seed);

    // int width = map_grid_width();
    // int height = map_grid_height();
    double fertility = 0.6;
    double roughness = 0.5;
    double openness = 0.4;
    log_info("Generate first pass", 0, 1);
    paint_by_noise(&noise, fertility, roughness, openness, TERRAIN_ROCK | TERRAIN_WATER);
    segment_map();

    log_info("Generate path", 0, 1);
    set_entry_exit_points();
    log_info("Generate second pass", 0, 1);
    paint_by_noise(&noise, fertility, roughness, openness,
                   TERRAIN_ROCK | TERRAIN_WATER | TERRAIN_MEADOW | TERRAIN_SHRUB | TERRAIN_TREE);

}
