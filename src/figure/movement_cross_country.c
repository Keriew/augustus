#include "movement.h"
#include "direction.h"
#include "building/properties.h"

#include "core/log.h"
#include "SDL.h"

#include "building/building.h"
#include "building/destruction.h"
#include "building/roadblock.h"
#include "core/calc.h"
#include "core/config.h"
#include "figure/combat.h"
#include "figure/route.h"
#include "figure/service.h"
#include "game/time.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/random.h"
#include "map/road_access.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"

// Not sure what this does?
void figure_movement_set_cross_country_direction(figure *f, int x_src, int y_src, int x_dst, int y_dst, int is_missile) {
    // All x/y are in 1/15th of a tile
    f->cc_destination_x = x_dst;
    f->cc_destination_y = y_dst;

    // Calculate absolute deltas
    f->cc_delta_x = (x_src > x_dst) ? (x_src - x_dst) : (x_dst - x_src);
    f->cc_delta_y = (y_src > y_dst) ? (y_src - y_dst) : (y_dst - y_src);

    // Calculate initial Bresenham error term (2 * minor_delta - major_delta)
    if (f->cc_delta_x < f->cc_delta_y) {
        // Y is major axis, X is minor
        f->cc_delta_xy = 2 * f->cc_delta_x - f->cc_delta_y;
    } else if (f->cc_delta_y < f->cc_delta_x) {
        // X is major axis, Y is minor
        f->cc_delta_xy = 2 * f->cc_delta_y - f->cc_delta_x;
    } else { // Deltas are equal (perfect diagonal)
        f->cc_delta_xy = 0;
    }

    if (is_missile) {
        // Missiles use a continuous direction calculation
        f->direction = calc_missile_direction(x_src, y_src, x_dst, y_dst);
    } else {
        // Figures use snapped 9-way direction
        f->direction = calc_general_direction(x_src, y_src, x_dst, y_dst);
        
        // Direction Snapping Logic: If the path is highly vertical (Y-dominant)
        if (f->cc_delta_y > 2 * f->cc_delta_x) {
            switch (f->direction) {
                // Diagonal directions snap to vertical direction (Top or Bottom)
                case DIR_TOP_RIGHT: case DIR_TOP_LEFT: 
                    f->direction = DIR_TOP; 
                    break;
                case DIR_BOTTOM_RIGHT: case DIR_BOTTOM_LEFT: 
                    f->direction = DIR_BOTTOM; 
                    break;
            }
        }

        // Direction Snapping Logic: If the path is highly horizontal (X-dominant)
        if (f->cc_delta_x > 2 * f->cc_delta_y) {
            switch (f->direction) {
                // Diagonal directions snap to horizontal direction (Right or Left)
                case DIR_TOP_RIGHT: case DIR_BOTTOM_RIGHT: 
                    f->direction = DIR_RIGHT; 
                    break;
                case DIR_BOTTOM_LEFT: case DIR_TOP_LEFT: 
                    f->direction = DIR_LEFT; 
                    break;
            }
        }

    // Set the major axis for Bresenham's algorithm
    if (f->cc_delta_x >= f->cc_delta_y) {
        f->cc_direction = CC_MAJOR_X;
    } else {
        f->cc_direction = CC_MAJOR_Y;
    }
}
// ???
void figure_movement_set_cross_country_destination(figure *f, int x_dst, int y_dst)
{
    f->destination_x = x_dst;
    f->destination_y = y_dst;
    figure_movement_set_cross_country_direction(
        f, f->cross_country_x, f->cross_country_y,
        TICKS_PER_TILE * x_dst, TICKS_PER_TILE * y_dst, 0);
}
// Calculates the direction when travelling cross country (no roads)
static void cross_country_update_delta(figure *f)
{
    // The X-axis is the major axis
    if (f->cc_direction == CC_MAJOR_X) { 
        // Case 1: Error term is positive (or zero, meaning step diagonally)
        if (f->cc_delta_xy >= 0) {
            // Update error term: E_new = E_old + 2 * (dy - dx)
            f->cc_delta_xy += 2 * (f->cc_delta_y - f->cc_delta_x);
        } else {
            // Case 2: Error term is negative (meaning step only on major axis)
            // Update error term: E_new = E_old + 2 * dy
            f->cc_delta_xy += 2 * f->cc_delta_y;
        }
        // Decrement the total distance remaining on the major axis (X)
        f->cc_delta_x--;
        
    // The Y-axis is the major axis
    } else { // Implicitly f->cc_direction == CC_MAJOR_Y
        // Case 1: Error term is positive (or zero, meaning step diagonally)
        if (f->cc_delta_xy >= 0) {
            // Update error term: E_new = E_old + 2 * (dx - dy)
            f->cc_delta_xy += 2 * (f->cc_delta_x - f->cc_delta_y);
        } else {
            // Case 2: Error term is negative (meaning step only on major axis)
            // Update error term: E_new = E_old + 2 * dx
            f->cc_delta_xy += 2 * f->cc_delta_x;
        }
        // Decrement the total distance remaining on the major axis (Y)
        f->cc_delta_y--;
    }
}
// Moves the figure one sub-tile step in the x direction
static void cross_country_advance_x(figure *f)
{
    if (f->cross_country_x < f->cc_destination_x) {
        f->cross_country_x++;
    } else if (f->cross_country_x > f->cc_destination_x) {
        f->cross_country_x--;
    }
}
// Moves the figure one sub-tile step in the y direction
static void cross_country_advance_y(figure *f)
{
    if (f->cross_country_y < f->cc_destination_y) {
        f->cross_country_y++;
    } else if (f->cross_country_y > f->cc_destination_y) {
        f->cross_country_y--;
    }
}
// Calculates in which direction should the figure move towards
static void cross_country_advance(figure *f)
{
    cross_country_update_delta(f);
    
    // Check if the Y-axis is the major axis
    if (f->cc_direction == CC_MAJOR_Y) { 
        // Always advance the major axis (Y)
        cross_country_advance_y(f);
        
        // If the error term >= 0, also advance the minor axis (X)
        if (f->cc_delta_xy >= 0) {
            // Decrement the remaining distance on the minor axis
            f->cc_delta_x--; 
            cross_country_advance_x(f);
        }
    } else { // The X-axis is the major axis (f->cc_direction == CC_MAJOR_X)
        // Always advance the major axis (X)
        cross_country_advance_x(f);
        
        // If the error term >= 0, also advance the minor axis (Y)
        if (f->cc_delta_xy >= 0) {
            // Decrement the remaining distance on the minor axis
            f->cc_delta_y--; 
            cross_country_advance_y(f);
        }
    }
}
// What does this do?
int figure_movement_move_ticks_cross_country(figure *f, int num_ticks)
{
    map_figure_delete(f);
    int is_at_destination = 0;
    while (num_ticks > 0) {
        num_ticks--;
        if (f->missile_height > 0) {
            f->missile_height--;
        } else {
            f->missile_height = 0;
        }
        if (f->cc_delta_x + f->cc_delta_y <= 0) {
            is_at_destination = 1;
            break;
        }
        cross_country_advance(f);
    }
    f->x = f->cross_country_x / TICKS_PER_TILE;
    f->y = f->cross_country_y / TICKS_PER_TILE;
    f->grid_offset = map_grid_offset(f->x, f->y);
    if (map_terrain_is(f->grid_offset, TERRAIN_BUILDING)) {
        f->in_building_wait_ticks = 8;
    } else if (f->in_building_wait_ticks) {
        f->in_building_wait_ticks--;
    }
    map_figure_add(f);
    return is_at_destination;
}

// Checks if the figure can launch a missile (cross country)
int figure_movement_can_launch_cross_country_missile(int x_src, int y_src, int x_dst, int y_dst)
{
    int height = 0;
    figure *f = figure_get(0); // abuse unused figure 0 as scratch
    f->cross_country_x = TICKS_PER_TILE * x_src;
    f->cross_country_y = TICKS_PER_TILE * y_src;
    if (map_terrain_is(map_grid_offset(x_src, y_src), TERRAIN_WALL_OR_GATEHOUSE) ||
        building_get(map_building_at(map_grid_offset(x_src, y_src)))->type == BUILDING_WATCHTOWER) {
        height = 6;
    }
    figure_movement_set_cross_country_direction(f, TICKS_PER_TILE * x_src, TICKS_PER_TILE * y_src, TICKS_PER_TILE * x_dst, TICKS_PER_TILE * y_dst, 0);

    for (int guard = 0; guard < 1000; guard++) {
        for (int i = 0; i < 8; i++) {
            if (f->cc_delta_x + f->cc_delta_y <= 0) {
                return 1;
            }
            cross_country_advance(f);
        }
        f->x = f->cross_country_x / TICKS_PER_TILE;
        f->y = f->cross_country_y / TICKS_PER_TILE;
        if (height) {
            height--;
        } else {
            int grid_offset = map_grid_offset(f->x, f->y);
            if (map_terrain_is(grid_offset, TERRAIN_WALL | TERRAIN_GATEHOUSE | TERRAIN_TREE)) {
                break;
            }
            if (map_terrain_is(grid_offset, TERRAIN_BUILDING) && map_property_multi_tile_size(grid_offset) > 1) {
                building_type type = building_get(map_building_at(grid_offset))->type;
                if (type != BUILDING_FORT_GROUND) {
                    break;
                }
            }
        }
    }
    return 0;
}
