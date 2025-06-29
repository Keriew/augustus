#ifndef MAP_BRIDGE_H
#define MAP_BRIDGE_H
#include "building/type.h"

int map_bridge_building_length(void);

int building_type_is_bridge(building_type type);
//technically should be elsewhere, but this is the best place for now, to centralise bridge logic since it's an exemption. 
//similarly to roadblocks, which also have a building_type check in roadblock.c
void map_bridge_reset_building_length(void);

int map_bridge_calculate_length_direction(int x, int y, int *length, int *direction);

int map_bridge_get_sprite_id(int index, int length, int direction, int is_ship_bridge);
/**
 * Adds a bridge to the terrain
 * @param x Map X
 * @param y Map Y
 * @param is_ship_bridge Whether this is a ship bridge or low bridge
 * @return Length of the bridge
 */
int map_bridge_add(int x, int y, int is_ship_bridge);

void map_bridge_remove(int grid_offset, int mark_deleted);

int map_bridge_is_ramp_sprite(int sprite);

int map_bridge_find_start_and_direction(int grid_offset, int *axis, int *axis_direction);

void map_bridge_update_after_rotate(int counter_clockwise);

int map_bridge_count_figures(int grid_offset);

int map_is_bridge(int grid_offset);

int map_bridge_height(int grid_offset);

#endif // MAP_BRIDGE_H
