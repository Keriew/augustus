#include "terrain_generator.h"

#include "terrain_generator_algorithms.h"

#include "core/random.h"
#include "map/elevation.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "scenario/editor_map.h"

#include <stdlib.h>

static int use_fixed_seed = 0;
static unsigned int fixed_seed = 0;

int terrain_generator_clamp_int(int value, int min_value, int max_value)
{
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

int terrain_generator_random_between(int min_value, int max_value)
{
    return random_between_from_stdlib(min_value, max_value);
}

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

static void choose_edge_point(int side, int width, int height, int *x, int *y)
{
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
        *x = terrain_generator_clamp_int(*x, 0, width - 1);
    }
    if (height > 2) {
        *y = terrain_generator_clamp_int(*y, 0, height - 1);
    }
}

static void adjust_point_to_land(int *x, int *y, int width, int height)
{
    int grid_offset = map_grid_offset(*x, *y);
    if (!map_terrain_is(grid_offset, TERRAIN_WATER)) {
        return;
    }

    for (int radius = 1; radius <= 10; radius++) {
        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                int nx = *x + dx;
                int ny = *y + dy;
                if (!map_grid_is_inside(nx, ny, 1)) {
                    continue;
                }
                int offset = map_grid_offset(nx, ny);
                if (!map_terrain_is(offset, TERRAIN_WATER)) {
                    *x = nx;
                    *y = ny;
                    return;
                }
            }
        }
    }
}

static void set_road_tile(int x, int y)
{
    int grid_offset = map_grid_offset(x, y);
    map_terrain_remove(grid_offset,
        TERRAIN_WATER | TERRAIN_TREE | TERRAIN_SHRUB | TERRAIN_ROCK | TERRAIN_MEADOW | TERRAIN_ELEVATION | TERRAIN_ACCESS_RAMP);
    map_terrain_add(grid_offset, TERRAIN_ROAD);
    map_property_clear_constructing(grid_offset);
    map_property_set_multi_tile_size(grid_offset, 1);
    map_property_mark_draw_tile(grid_offset);
    map_tiles_update_area_roads(x, y, 3);
    map_tiles_update_region_empty_land(x - 1, y - 1, x + 1, y + 1);
}

static void add_road_between_points(int start_x, int start_y, int end_x, int end_y)
{
    int x = start_x;
    int y = start_y;
    int guard = 0;
    int max_steps = map_grid_width() * map_grid_height() * 2;

    set_road_tile(x, y);
    while ((x != end_x || y != end_y) && guard++ < max_steps) {
        int dx = end_x - x;
        int dy = end_y - y;
        int step_x = (dx > 0) - (dx < 0);
        int step_y = (dy > 0) - (dy < 0);
        int abs_dx = abs(dx);
        int abs_dy = abs(dy);
        int roll = terrain_generator_random_between(0, 100);

        if (abs_dx >= abs_dy) {
            if (step_x && roll < 70) {
                x += step_x;
            } else if (step_y) {
                y += step_y;
            }
        } else {
            if (step_y && roll < 70) {
                y += step_y;
            } else if (step_x) {
                x += step_x;
            }
        }

        x = terrain_generator_clamp_int(x, 0, map_grid_width() - 1);
        y = terrain_generator_clamp_int(y, 0, map_grid_height() - 1);
        set_road_tile(x, y);
    }
}

static void set_entry_exit_points(void)
{
    int width = map_grid_width();
    int height = map_grid_height();
    int entry_side = terrain_generator_random_between(0, 4);
    int exit_side = terrain_generator_random_between(0, 4);
    while (exit_side == entry_side) {
        exit_side = terrain_generator_random_between(0, 4);
    }

    int entry_x = 0;
    int entry_y = 0;
    int exit_x = 0;
    int exit_y = 0;

    choose_edge_point(entry_side, width, height, &entry_x, &entry_y);
    choose_edge_point(exit_side, width, height, &exit_x, &exit_y);

    adjust_point_to_land(&entry_x, &entry_y, width, height);
    adjust_point_to_land(&exit_x, &exit_y, width, height);

    scenario_editor_set_entry_point(entry_x, entry_y);
    scenario_editor_set_exit_point(exit_x, exit_y);

    add_road_between_points(entry_x, entry_y, exit_x, exit_y);
}

void terrain_generator_generate(terrain_generator_algorithm algorithm)
{
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
            terrain_generator_river_map(fixed_seed);
            break;
        case TERRAIN_GENERATOR_RANDOM:
        default:
            terrain_generator_random_terrain();
            break;
    }

    set_entry_exit_points();

    random_clear_stdlib_seed();
}

void terrain_generator_set_seed(int enabled, unsigned int seed)
{
    use_fixed_seed = enabled != 0;
    fixed_seed = seed;
}
