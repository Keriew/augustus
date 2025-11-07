#include "city_water_ghost.h"

#include "building/building.h"
#include "building/construction.h"
#include "city/view.h"
#include "map/grid.h"
#include "map/water_supply.h"
#include "widget/city_building_ghost.h"

#include <string.h>

enum {
    WATER_ACCESS_NONE = 0x0,
    WATER_ACCESS_WELL = 0x1,
    WATER_ACCESS_FOUNTAIN = 0x2
};

static uint8_t has_water_access[GRID_SIZE * GRID_SIZE];
static uint8_t has_reservoir_access[GRID_SIZE * GRID_SIZE];
static building_type last_building_type = BUILDING_NONE;
static building_type last_reservoir_building_type = BUILDING_NONE;
static int last_well_count = 0;
static int last_fountain_count = 0;
static int last_reservoir_count = 0;

static void set_well_access(int x, int y, int grid_offset)
{
    has_water_access[grid_offset] |= WATER_ACCESS_WELL;
}

static void set_fountain_access(int x, int y, int grid_offset)
{
    has_water_access[grid_offset] |= WATER_ACCESS_FOUNTAIN;
}

static void set_reservoir_access(int x, int y, int grid_offset)
{
    has_reservoir_access[grid_offset] = 1;
}

static void update_water_access(void)
{
    memset(has_water_access, 0, sizeof(uint8_t) * GRID_SIZE * GRID_SIZE);
    for (building *b = building_first_of_type(BUILDING_WELL); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_RUBBLE) {
            city_view_foreach_tile_in_range(b->grid_offset, 1, map_water_supply_well_radius(), set_well_access);
        }
    }
    for (building *b = building_first_of_type(BUILDING_FOUNTAIN); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_RUBBLE) {
            city_view_foreach_tile_in_range(b->grid_offset, 1, map_water_supply_fountain_radius(), set_fountain_access);
        }
    }
}

static void update_reservoir_access(void)
{
    memset(has_reservoir_access, 0, sizeof(uint8_t) * GRID_SIZE * GRID_SIZE);
    for (building *b = building_first_of_type(BUILDING_RESERVOIR); b; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE && b->has_water_access) {
            city_view_foreach_tile_in_range(b->grid_offset, 3, map_water_supply_reservoir_radius(), set_reservoir_access);
            city_view_foreach_tile_in_range(b->grid_offset, 0, 3, set_reservoir_access);// include the reservoir tiles
            set_reservoir_access(b->x, b->y, b->grid_offset); // include the reservoir main tile
        }
    }
}

static void draw_water_access(int x, int y, int grid_offset)
{
    uint8_t water_access = has_water_access[grid_offset];
    if (water_access & WATER_ACCESS_FOUNTAIN) {
        city_building_ghost_draw_fountain_range(x, y, grid_offset);
    } else if (water_access & WATER_ACCESS_WELL) {
        city_building_ghost_draw_well_range(x, y, grid_offset);
    }
}

static void draw_reservoir_access(int x, int y, int grid_offset)
{
    if (has_reservoir_access[grid_offset]) {
        city_building_ghost_draw_reservoir_range(x, y, grid_offset);
    }
}

void city_water_ghost_draw_water_structure_ranges(void)
{
    building_type type = building_construction_type();
    // we're counting the number of buildings using the building linked list rather than the counts in building/counts.c
    // because the linked list counts update immediately so the outlines still update even when the game is paused
    int num_wells = 0;
    for (building *b = building_first_of_type(BUILDING_WELL); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_RUBBLE) {
            num_wells++;
        }
    }
    int num_fountains = 0;
    for (building *b = building_first_of_type(BUILDING_FOUNTAIN); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_RUBBLE) {
            num_fountains++;
        }
    }
    if (type != last_building_type || num_wells != last_well_count || num_fountains != last_fountain_count) {
        update_water_access();
    }
    last_building_type = type;
    last_well_count = num_wells;
    last_fountain_count = num_fountains;
    city_view_foreach_valid_map_tile(draw_water_access);
}

void city_water_ghost_draw_reservoir_ranges(void)
{
    building_type type = building_construction_type();
    int num_reservoirs = 0;
    for (building *b = building_first_of_type(BUILDING_RESERVOIR); b; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE && b->has_water_access) {
            num_reservoirs++;
        }
    }
    if (type != last_reservoir_building_type || num_reservoirs != last_reservoir_count) {
        update_reservoir_access();
    }
    last_reservoir_building_type = type;
    last_reservoir_count = num_reservoirs;
    city_view_foreach_valid_map_tile(draw_reservoir_access);
}
