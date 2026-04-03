#include "terrain_generator_algorithms.h"

#include "map/grid.h"
#include "map/terrain.h"

void terrain_generator_generate_flat_plains(void)
{
    int width = map_grid_width();
    int height = map_grid_height();

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int grid_offset = map_grid_offset(x, y);
            int roll = terrain_generator_random_between(0, 100);
            if (roll < 6) {
                map_terrain_set_with_tile_update(grid_offset, TERRAIN_TREE);
            } else if (roll < 8) {
                map_terrain_set_with_tile_update(grid_offset, TERRAIN_ROCK);
            } else if (roll < 18) {
                map_terrain_set_with_tile_update(grid_offset, TERRAIN_SHRUB);
            } else if (roll < 35) {
                map_terrain_set_with_tile_update(grid_offset, TERRAIN_MEADOW);
            } else if (roll < 60) {
                map_terrain_set_with_tile_update(grid_offset, TERRAIN_WATER);
            }
        }
    }
}
