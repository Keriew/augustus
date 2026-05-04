#include "building.h"


#include "figure/movement.h"


#include "building/building.h"
#include "core/config.h"
#include "game/save_version.h"
#include "map/grid.h"
#include "map/terrain.h"

static grid_u32 buildings_grid;
static grid_u8 damage_grid;
static grid_u32 rubble_info_grid;

static grid_u32 buildings_grid_backup;
static grid_u8 damage_grid_backup;
static grid_u32 rubble_info_grid_backup;


unsigned int map_building_at(int grid_offset)
{
    return map_grid_is_valid_offset(grid_offset) ? buildings_grid.items[grid_offset] : 0;
}

building_type map_building_type_at(int grid_offset)
{
    unsigned int building_id = map_building_at(grid_offset);
    return building_id ? building_get(building_id)->type : BUILDING_NONE;
}

unsigned int map_building_from_buffer_16(buffer *buildings, int grid_offset)
{
    buffer_set(buildings, grid_offset * sizeof(uint16_t));
    return buffer_read_u16(buildings);
}

unsigned int map_building_from_buffer_32(buffer *buildings, int grid_offset)
{
    buffer_set(buildings, grid_offset * sizeof(uint32_t));
    return buffer_read_u32(buildings);
}

void map_building_set(int grid_offset, unsigned int building_id)
{
    buildings_grid.items[grid_offset] = building_id;
}

void map_building_damage_clear(int grid_offset)
{
    damage_grid.items[grid_offset] = 0;
}

int map_building_damage_increase(int grid_offset)
{
    return ++damage_grid.items[grid_offset];
}

unsigned int map_building_rubble_building_id(int grid_offset)
{
    return rubble_info_grid.items[grid_offset];
}

void map_building_set_rubble_grid_building_id(int grid_offset, unsigned int building_id, int size)
{
    int x = map_grid_offset_to_x(grid_offset);
    int y = map_grid_offset_to_y(grid_offset);
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int offset = map_grid_offset(x + i, y + j);
            if (!building_id || !map_terrain_is(offset, TERRAIN_WATER)) {
                rubble_info_grid.items[offset] = building_id;
            }
        }
    }
}

int map_building_ruins_left(unsigned int building_id)
{
    // doesnt work for hippodromes and forts - forts shouldnt turn to rubble, hippodromes are not repairable
    building *b = building_get(building_id);
    int ruins_count = 0;
    if (b->type == BUILDING_HIPPODROME || building_is_fort(b->type)) {
        return 0;
    }
    int size = b->data.rubble.og_size ? b->data.rubble.og_size : b->size;
    int slice_offset = b->data.rubble.og_grid_offset ? b->data.rubble.og_grid_offset : b->grid_offset;
    grid_slice *slice = map_grid_get_grid_slice_square(slice_offset, size);
    for (int i = 0; i < slice->size; i++) {
        int grid_offset = slice->grid_offsets[i];
        if (map_building_rubble_building_id(grid_offset) == building_id) {
            ruins_count++;
        } else if (map_building_at(grid_offset) == building_id &&
                building_get(building_id)->type == BUILDING_BURNING_RUIN) {
            ruins_count++;
        }
    }
    return ruins_count;
}

void map_building_backup(void)
{
    map_grid_copy_u32(buildings_grid.items, buildings_grid_backup.items);
    map_grid_copy_u8(damage_grid.items, damage_grid_backup.items);
    map_grid_copy_u32(rubble_info_grid.items, rubble_info_grid_backup.items);
}

void map_building_restore(void)
{
    map_grid_copy_u32(buildings_grid_backup.items, buildings_grid.items);
    map_grid_copy_u8(damage_grid_backup.items, damage_grid.items);
    map_grid_copy_u32(rubble_info_grid_backup.items, rubble_info_grid.items);
}

void map_building_clear_backup(void)
{
    map_grid_clear_u32(buildings_grid_backup.items);
    map_grid_clear_u8(damage_grid_backup.items);
    map_grid_clear_u32(rubble_info_grid_backup.items);
}

void map_building_clear(void)
{
    map_grid_clear_u32(buildings_grid.items);
    map_grid_clear_u8(damage_grid.items);
    map_grid_clear_u32(rubble_info_grid.items);
}

void map_building_save_state(buffer *buildings, buffer *damage, buffer *rubble)
{
    map_grid_save_state_u32(buildings_grid.items, buildings);
    map_grid_save_state_u8(damage_grid.items, damage);
    map_grid_save_state_u32(rubble_info_grid.items, rubble);
}

void map_building_load_state(buffer *buildings, buffer *damage, buffer *rubble, savegame_version_t version)
{
    if (version <= SAVE_GAME_LAST_U16_GRIDS) {
        map_grid_load_state_u16_to_u32(buildings_grid.items, buildings);
        map_grid_load_state_u8(damage_grid.items, damage);
    } else {
        map_grid_load_state_u32(buildings_grid.items, buildings);
        map_grid_load_state_u8(damage_grid.items, damage);
        map_grid_load_state_u32(rubble_info_grid.items, rubble);
    }
}

int map_building_is_reservoir(int x, int y)
{
    if (!map_grid_is_inside(x, y, 3)) {
        return 0;
    }
    int grid_offset = map_grid_offset(x, y);
    unsigned int building_id = map_building_at(grid_offset);
    if (!building_id || building_get(building_id)->type != BUILDING_RESERVOIR) {
        return 0;
    }
    for (int dy = 0; dy < 3; dy++) {
        for (int dx = 0; dx < 3; dx++) {
            if (building_id != map_building_at(grid_offset + map_grid_delta(dx, dy))) {
                return 0;
            }
        }
    }
    return 1;
}

/*
int map_building_damage_get(int grid_offset)
{
    return map_grid_is_valid_offset(grid_offset) ? damage_grid.items[grid_offset] : 0;
}
*/


int map_building_damage_get(int grid_offset)
{
    return damage_grid.items[grid_offset];
}

int map_building_average_damage(const building *b)
{
    int total_damage = 0;
    int tiles = 0;

    int size = b->size;
    int base_x = map_grid_offset_to_x(b->grid_offset);
    int base_y = map_grid_offset_to_y(b->grid_offset);

    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            int offset = map_grid_offset(base_x + dx, base_y + dy);

            if (map_building_at(offset) == b->id) {
                total_damage += map_building_damage_get(offset);
                tiles++;
            }
        }
    }

    return tiles ? total_damage / tiles : 0;
}

void map_building_get_health(const building *b, int *current, int *max)
{
    int max_hp = BUILDING_HP;

    switch (b->type) {
        case BUILDING_WALL: max_hp = WALL_HP; break;
        case BUILDING_GATEHOUSE: max_hp = GATEHOUSE_HP; break;
        case BUILDING_PALISADE:
        case BUILDING_PALISADE_GATE: max_hp = PALISADE_HP; break;
        case BUILDING_TOWER: max_hp = TOWER_HP; break;

        default: max_hp = BUILDING_HP; break;
    }

    int damage = map_building_average_damage(b);

    int current_hp = max_hp - damage;
    if (current_hp < 0) current_hp = 0;

    *current = current_hp;
    *max = max_hp;
}
