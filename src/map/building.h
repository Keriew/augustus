#ifndef MAP_BUILDING_H
#define MAP_BUILDING_H

#include "building/type.h"
#include "core/buffer.h"
#include "game/save_version.h"

/**
 * Returns the building at the given offset
 * @param grid_offset Map offset
 * @return Building ID of building at offset, 0 means no building
 */
int map_building_at(int grid_offset);

int map_building_from_buffer(buffer *buildings, int grid_offset);

void map_building_set(int grid_offset, int building_id);

/**
 * Increases building damage by 1
 * @param grid_offset Map offset
 * @return New damage amount
 */
int map_building_damage_increase(int grid_offset);

void map_building_damage_clear(int grid_offset);

int map_building_rubble_building_id(int grid_offset);

void map_building_set_rubble_grid_building_id(int grid_offset, unsigned int building_id, int size);

/**
 * Clears the maps related to buildings
 */
void map_building_clear(void);

void map_building_save_state(buffer *buildings, buffer *damage, buffer *rubble);

void map_building_load_state(buffer *buildings, buffer *damage, buffer *rubble, savegame_version_t version);

int map_building_is_reservoir(int x, int y);

#endif // MAP_BUILDING_H
