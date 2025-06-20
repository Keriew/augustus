#include "bridge.h"

#include "building/building.h"
#include "building/type.h"
#include "city/view.h"
#include "core/direction.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/data.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/routing_terrain.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "map/tiles.h"

#define MAX_DISTANCE_BETWEEN_PILLARS 12
#define MINIMUM_DISTANCE_FOR_PILLARS 9

static struct {
    int end_grid_offset;
    int length;
    int direction;
    int direction_grid_delta;
} bridge;

int map_bridge_building_length(void)
{
    return bridge.length;
}

int building_type_is_bridge(building_type type)
//technically should be elsewhere, but this is the best place for now, to centralise bridge logic since it's an exemption. 
{
    return type == BUILDING_LOW_BRIDGE || type == BUILDING_SHIP_BRIDGE;
}

void map_bridge_reset_building_length(void)
{
    bridge.length = 0;
}

int map_bridge_calculate_length_direction(int x, int y, int *length, int *direction)
{
    int grid_offset = map_grid_offset(x, y);
    bridge.end_grid_offset = 0;
    bridge.direction_grid_delta = 0;
    bridge.length = *length = 0;
    bridge.direction = *direction = 0;

    if (!map_terrain_is(grid_offset, TERRAIN_WATER)) {
        return 0;
    }
    if (map_terrain_is(grid_offset, TERRAIN_ROAD | TERRAIN_BUILDING)) {
        return 0;
    }
    if (map_terrain_count_directly_adjacent_with_type(grid_offset, TERRAIN_WATER) != 3) {
        return 0;
    }
    if (!map_terrain_is(grid_offset + map_grid_delta(0, -1), TERRAIN_WATER)) {
        bridge.direction_grid_delta = map_grid_delta(0, 1);
        bridge.direction = DIR_4_BOTTOM;
    } else if (!map_terrain_is(grid_offset + map_grid_delta(1, 0), TERRAIN_WATER)) {
        bridge.direction_grid_delta = map_grid_delta(-1, 0);
        bridge.direction = DIR_6_LEFT;
    } else if (!map_terrain_is(grid_offset + map_grid_delta(0, 1), TERRAIN_WATER)) {
        bridge.direction_grid_delta = map_grid_delta(0, -1);
        bridge.direction = DIR_0_TOP;
    } else if (!map_terrain_is(grid_offset + map_grid_delta(-1, 0), TERRAIN_WATER)) {
        bridge.direction_grid_delta = map_grid_delta(1, 0);
        bridge.direction = DIR_2_RIGHT;
    } else {
        return 0;
    }
    *direction = bridge.direction;
    bridge.length = 1;
    for (int i = 0; i < 64; i++) { //longer bridges
        grid_offset += bridge.direction_grid_delta;
        bridge.length++;
        int next_offset = grid_offset + bridge.direction_grid_delta;
        if (map_terrain_is(next_offset, TERRAIN_TREE)) {
            break;
        }
        if (!map_terrain_is(next_offset, TERRAIN_WATER)) {
            bridge.end_grid_offset = grid_offset;
            if (map_terrain_count_directly_adjacent_with_type(grid_offset, TERRAIN_WATER) != 3) {
                bridge.end_grid_offset = 0;
            }
            *length = bridge.length;
            return bridge.end_grid_offset;
        }
        if (map_terrain_is(next_offset, TERRAIN_ROAD | TERRAIN_BUILDING)) {
            break;
        }
        if (map_terrain_count_diagonally_adjacent_with_type(grid_offset, TERRAIN_WATER) != 4) {
            break;
        }
    }
    // invalid bridge
    *length = bridge.length;
    return 0;
}

static int get_number_of_bridge_sections(void)
{
    if (bridge.length < MINIMUM_DISTANCE_FOR_PILLARS) {
        return 1;
    }
    // Number of pillars is equal to length/max_distance, rounded up
    int number_of_pillars = (bridge.length + MAX_DISTANCE_BETWEEN_PILLARS - 1) / MAX_DISTANCE_BETWEEN_PILLARS;
    return number_of_pillars + 1;
}

static int get_pillar_distance(void)
{
    if (bridge.length < MINIMUM_DISTANCE_FOR_PILLARS) {
        return bridge.length;
    }

    int number_of_sections = get_number_of_bridge_sections();
    int pillar_distance = bridge.length / (number_of_sections);

    return pillar_distance;
}

static int get_pillar_shift(void)
{
    if (bridge.length < MINIMUM_DISTANCE_FOR_PILLARS) {
        return 0;
    }

    int pillar_distance = get_pillar_distance();
    int pillar_shift = (bridge.length % pillar_distance) / 2;

    return pillar_shift;
}


int map_bridge_get_sprite_id(int index, int length, int direction, int is_ship_bridge)
{
    if (is_ship_bridge) {
        int pillar_distance = get_pillar_distance();
        int pillar_shift = get_pillar_shift();
        if (index == 1 || index == length - 2) {
            // platform after ramp
            return 13;
        } else if (index == 0) {
            // ramp at start
            switch (direction) {
                case DIR_0_TOP:
                    return 7;
                case DIR_2_RIGHT:
                    return 8;
                case DIR_4_BOTTOM:
                    return 9;
                case DIR_6_LEFT:
                    return 10;
            }
        } else if (index == length - 1) {
            // ramp at end
            switch (direction) {
                case DIR_0_TOP:
                    return 9;
                case DIR_2_RIGHT:
                    return 10;
                case DIR_4_BOTTOM:
                    return 7;
                case DIR_6_LEFT:
                    return 8;
            }
        } else if ((index - pillar_shift) % pillar_distance == 0) {
            if (direction == DIR_0_TOP || direction == DIR_4_BOTTOM) {
                return 14;
            } else {
                return 15;
            }
        } else {
            // middle of the bridge
            if (direction == DIR_0_TOP || direction == DIR_4_BOTTOM) {
                return 11;
            } else {
                return 12;
            }
        }
    } else {
        if (index == 0) {
            // ramp at start
            switch (direction) {
                case DIR_0_TOP:
                    return 1;
                case DIR_2_RIGHT:
                    return 2;
                case DIR_4_BOTTOM:
                    return 3;
                case DIR_6_LEFT:
                    return 4;
            }
        } else if (index == length - 1) {
            // ramp at end
            switch (direction) {
                case DIR_0_TOP:
                    return 3;
                case DIR_2_RIGHT:
                    return 4;
                case DIR_4_BOTTOM:
                    return 1;
                case DIR_6_LEFT:
                    return 2;
            }
        } else {
            // middle part
            if (direction == DIR_0_TOP || direction == DIR_4_BOTTOM) {
                return 5;
            } else {
                return 6;
            }
        }
    }
    return 0;
}

int map_bridge_add(int x, int y, int is_ship_bridge)
{
    int min_length = is_ship_bridge ? 5 : 2;
    if (bridge.end_grid_offset <= 0 || bridge.length < min_length) {
        bridge.length = 0;
        return bridge.length;
    }

    bridge.direction -= city_view_orientation();
    if (bridge.direction < 0) {
        bridge.direction += 8;
    }

    int grid_offset = map_grid_offset(x, y);
    int bridge_type = !is_ship_bridge ? BUILDING_LOW_BRIDGE : BUILDING_SHIP_BRIDGE;

    building *b = building_create(bridge_type, x, y);
    for (int i = 0; i < bridge.length; i++) {
        map_terrain_add(grid_offset, TERRAIN_ROAD);
        map_terrain_add(grid_offset, TERRAIN_BUILDING);
        map_building_set(grid_offset, b->id);
        int value = map_bridge_get_sprite_id(i, bridge.length, bridge.direction, is_ship_bridge);
        map_sprite_bridge_set(grid_offset, value);

        grid_offset += bridge.direction_grid_delta;
    }


    map_routing_update_land();
    map_routing_update_water();
    map_tiles_update_region_water(x, y, map_grid_offset_to_x(grid_offset), map_grid_offset_to_y(grid_offset));
    return bridge.length;
}

int map_is_bridge(int grid_offset)
{
    return map_terrain_is(grid_offset, TERRAIN_WATER) && map_terrain_is(grid_offset, TERRAIN_ROAD) && map_terrain_is(grid_offset, TERRAIN_BUILDING);
}

int map_bridge_is_ramp_sprite(int sprite)
{
    return (sprite >= 1 && sprite <= 4) || (sprite >= 7 && sprite <= 10);
}

static int legacy_map_is_bridge(int grid_offset)
{
    //old way for checking for bridges - check if it's sprite, and check if it's on water
    //checking just for sprites is misleading, as on land buildings also have sprites - it's their animation frame
    //this function is not currently used in this module, but leaving it here as a precaution
    return (map_sprite_bridge_at(grid_offset)) && map_terrain_is(grid_offset, TERRAIN_WATER);
}

int map_bridge_find_start_and_direction(int grid_offset, int *axis, int *axis_direction)
{
    if (!map_is_bridge(grid_offset)) {
        return -1;
    }

    building *b = building_get(map_building_at(grid_offset));
    if (!b) return -1;

    int start = map_grid_offset(b->x, b->y);
    int building_id = b->id;

    static const int dirs[4][2] = {
        {  0, -1 }, // north
        { +1,  0 }, // east
        {  0, +1 }, // south
        { -1,  0 }  // west
    };

    for (int i = 0; i < 4; i++) {
        int dx = dirs[i][0];
        int dy = dirs[i][1];
        int neighbor = start + map_grid_delta(dx, dy);
        if (map_building_at(neighbor) == building_id) {
            *axis = (dx != 0) ? 0 : 1;
            *axis_direction = (dx + dy > 0) ? +1 : -1;
            return start;
        }
    }

    // If no direction found, default to +1 vertical (shouldn't happen)
    *axis = 1;
    *axis_direction = 1;
    return start;
}



void map_bridge_remove(int grid_offset, int mark_deleted)
{
    if (!map_is_bridge(grid_offset)) {
        return;
    }

    int axis, dir;
    int start = map_bridge_find_start_and_direction(grid_offset, &axis, &dir);
    if (start < 0) {
        return;
    }

    int delta = (axis == 0)
        ? map_grid_delta(dir, 0)  // horizontal
        : map_grid_delta(0, dir); // vertical

    int building_id = map_building_at(start);
    int current = start;

    int bridge_x_start = map_grid_offset_to_x(start);
    int bridge_y_start = map_grid_offset_to_y(start);

    while (map_is_bridge(current) && map_building_at(current) == building_id) {
        if (mark_deleted) {
            map_property_mark_deleted(current);
        } else {
            map_sprite_clear_tile(current);
            map_terrain_remove(current, TERRAIN_ROAD);
            map_terrain_remove(current, TERRAIN_BUILDING);
            map_building_set(current, 0);
        }
        current += delta;
    }

    int bridge_x_end = map_grid_offset_to_x(current - delta);
    int bridge_y_end = map_grid_offset_to_y(current - delta);

    game_undo_disable();
    map_tiles_update_region_water(bridge_x_start, bridge_y_start, bridge_x_end, bridge_y_end);
    map_tiles_update_region_empty_land(bridge_x_start, bridge_y_start, bridge_x_end, bridge_y_end);
}


int map_bridge_count_figures(int grid_offset)
{
    if (!map_is_bridge(grid_offset)) {
        return 0;
    }

    int axis, dir;
    int start = map_bridge_find_start_and_direction(grid_offset, &axis, &dir);
    if (start < 0) {
        return 0;
    }

    int delta = (axis == 0)
        ? map_grid_delta(dir, 0)  // horizontal
        : map_grid_delta(0, dir); // vertical

    int building_id = map_building_at(start);
    int current = start;
    int figures = 0;
    // find lower end of the bridge
    while (map_is_bridge(current) && map_building_at(current) == building_id) {
        if (map_has_figure_at(grid_offset)) {
            figures++;
        }
        current += delta;
    }
    return figures;
}

void map_bridge_update_after_rotate(int counter_clockwise)
{
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (map_is_bridge(grid_offset)) {
                int new_value;
                switch (map_sprite_bridge_at(grid_offset)) {
                    case 1: new_value = counter_clockwise ? 2 : 4; break;
                    case 2: new_value = counter_clockwise ? 3 : 1; break;
                    case 3: new_value = counter_clockwise ? 4 : 2; break;
                    case 4: new_value = counter_clockwise ? 1 : 3; break;
                    case 5: new_value = 6; break;
                    case 6: new_value = 5; break;
                    case 7: new_value = counter_clockwise ? 8 : 10; break;
                    case 8: new_value = counter_clockwise ? 9 : 7; break;
                    case 9: new_value = counter_clockwise ? 10 : 8; break;
                    case 10: new_value = counter_clockwise ? 7 : 9; break;
                    case 11: new_value = 12; break;
                    case 12: new_value = 11; break;
                    case 13: new_value = 13; break;
                    case 14: new_value = 15; break;
                    case 15: new_value = 14; break;
                    default: new_value = map_sprite_bridge_at(grid_offset);
                }
                map_sprite_bridge_set(grid_offset, new_value);
            }
        }
    }
}

int map_bridge_height(int grid_offset)
{
    int sprite = map_sprite_bridge_at(grid_offset);
    if (sprite <= 6) {
        // low bridge
        switch (sprite) {
            case 1:
            case 4:
                return 10;
            case 2:
            case 3:
                return 16;
            default:
                return 20;
        }
    } else {
        // ship bridge
        switch (sprite) {
            case 7:
            case 8:
            case 9:
            case 10:
                return 14;
            case 13:
                return 30;
            default:
                return 36;
        }
    }
}
