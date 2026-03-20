#include "terrain_generator_algorithms.h"

#include "map/elevation.h"
#include "map/grid.h"
#include "map/terrain.h"

#include <stdlib.h>

void terrain_generator_generate_river_valley(void)
{
    int width = map_grid_width();
    int height = map_grid_height();
    int river_half_width = 1;
    int guard = 0;
    int max_steps = width * height * 2;
    int start_side = terrain_generator_random_between(0, 4);
    int end_side = terrain_generator_random_between(0, 4);
    while (end_side == start_side) {
        end_side = terrain_generator_random_between(0, 4);
    }

    int start_x = 0;
    int start_y = 0;
    int end_x = 0;
    int end_y = 0;

    switch (start_side) {
        case 0: // north
            start_y = 0;
            start_x = terrain_generator_random_between(1, width - 1);
            break;
        case 1: // south
            start_y = height - 1;
            start_x = terrain_generator_random_between(1, width - 1);
            break;
        case 2: // west
            start_x = 0;
            start_y = terrain_generator_random_between(1, height - 1);
            break;
        default: // east
            start_x = width - 1;
            start_y = terrain_generator_random_between(1, height - 1);
            break;
    }

    switch (end_side) {
        case 0: // north
            end_y = 0;
            end_x = terrain_generator_random_between(1, width - 1);
            break;
        case 1: // south
            end_y = height - 1;
            end_x = terrain_generator_random_between(1, width - 1);
            break;
        case 2: // west
            end_x = 0;
            end_y = terrain_generator_random_between(1, height - 1);
            break;
        default: // east
            end_x = width - 1;
            end_y = terrain_generator_random_between(1, height - 1);
            break;
    }

    if (width > 2) {
        start_x = terrain_generator_clamp_int(start_x, 0, width - 1);
        end_x = terrain_generator_clamp_int(end_x, 0, width - 1);
    }
    if (height > 2) {
        start_y = terrain_generator_clamp_int(start_y, 0, height - 1);
        end_y = terrain_generator_clamp_int(end_y, 0, height - 1);
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
        int meander_roll = terrain_generator_random_between(0, 100);
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
                step_y = terrain_generator_random_between(0, 2) ? 1 : -1;
            } else {
                step_x = dx;
            }
        } else if (dy != 0) {
            if (meander_roll < 20) {
                step_x = terrain_generator_random_between(0, 2) ? 1 : -1;
            } else {
                step_y = dy;
            }
        }

        int next_x = terrain_generator_clamp_int(x + step_x, 0, width - 1);
        int next_y = terrain_generator_clamp_int(y + step_y, 0, height - 1);
        if (next_x == x && next_y == y) {
            if (dx != 0) {
                next_x = terrain_generator_clamp_int(x + dx, 0, width - 1);
            }
            if (dy != 0) {
                next_y = terrain_generator_clamp_int(y + dy, 0, height - 1);
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
            if (elevation > 0 && terrain_generator_random_between(0, 4) == 0) {
                elevation++;
            }
            elevation = terrain_generator_clamp_int(elevation, 0, 4);
            map_elevation_set(grid_offset, elevation);

            int roll = terrain_generator_random_between(0, 100);
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
