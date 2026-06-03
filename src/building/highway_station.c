#include "highway_station.h"

#include "building/building.h"
#include "building/distribution.h"
#include "building/image.h"
#include "city/buildings.h"
#include "game/resource.h"
#include "map/building_tiles.h"
#include "map/data.h"
#include "map/terrain.h"

#define MAX_DISTANCE 40

static int count_highway_tiles(void)
{
    int tiles = 0;
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
                tiles++;
            }
        }
    }
    return tiles;
}

int building_highway_station_highway_blocks(void)
{
    // count_highway_tiles() returns sub-tiles. A placed highway is a 2x2 block,
    // so each block contributes 4 sub-tiles. Divide by 4 to get block count.
    return count_highway_tiles() / 4;
}

int building_highway_station_monthly_need(void)
{
    int blocks = building_highway_station_highway_blocks();
    if (blocks <= 0) {
        return 0;
    }
    return (blocks + HIGHWAY_STATION_HIGHWAY_BLOCKS_PER_UNIT - 1) / HIGHWAY_STATION_HIGHWAY_BLOCKS_PER_UNIT;
}

int building_highway_station_max_stock(void)
{
    return building_highway_station_monthly_need() * HIGHWAY_STATION_STOCK_MONTHS * HIGHWAY_STATION_RESOURCE_PER_LOAD;
}

int building_highway_station_is_functional(building *b)
{
    if (!b || b->state != BUILDING_STATE_IN_USE || b->num_workers <= 0) {
        return 0;
    }
    int need = building_highway_station_monthly_need();
    if (need == 0) {
        return 1;
    }
    int need_internal = need * HIGHWAY_STATION_RESOURCE_PER_LOAD;
    if (b->resources[RESOURCE_STONE] < need_internal) {
        return 0;
    }
    if (b->resources[RESOURCE_SAND] < need_internal) {
        return 0;
    }
    return 1;
}

void building_highway_station_refresh_graphic(building *b)
{
    if (!b || b->state != BUILDING_STATE_IN_USE) {
        return;
    }
    map_building_tiles_add(b->id, b->x, b->y, b->size,
        building_image_get(b), TERRAIN_BUILDING);
}

void building_highway_station_consume_monthly(void)
{
    int id = city_buildings_get_highway_station();
    if (!id) {
        return;
    }
    building *b = building_get(id);
    if (b->state != BUILDING_STATE_IN_USE) {
        return;
    }
    int need = building_highway_station_monthly_need();
    if (need <= 0) {
        return;
    }
    int need_internal = need * HIGHWAY_STATION_RESOURCE_PER_LOAD;
    if (b->resources[RESOURCE_STONE] >= need_internal && b->resources[RESOURCE_SAND] >= need_internal) {
        b->resources[RESOURCE_STONE] -= need_internal;
        b->resources[RESOURCE_SAND] -= need_internal;
    }
    building_highway_station_refresh_graphic(b);
}


int building_highway_station_get_storage_destination(building *highway_station)
{
    int max_stock = building_highway_station_max_stock();
    if (max_stock <= 0) {
        return 0;
    }
    if (highway_station->resources[RESOURCE_STONE] >= max_stock &&
        highway_station->resources[RESOURCE_SAND] >= max_stock) {
        return 0;
    }
    resource_storage_info info[RESOURCE_MAX] = { 0 };
    info[RESOURCE_STONE].needed = 1;
    info[RESOURCE_SAND].needed = 1;
    if (!building_distribution_get_resource_storages_for_building(info, highway_station, MAX_DISTANCE)) {
        return 0;
    }
    int fetch_inventory = building_distribution_fetch(highway_station, info, 0, 1);
    if (fetch_inventory != RESOURCE_NONE) {
        highway_station->data.market.fetch_inventory_id = fetch_inventory;
        return info[fetch_inventory].building_id;
    }
    fetch_inventory = building_distribution_fetch(highway_station, info, max_stock, 0);
    if (fetch_inventory != RESOURCE_NONE) {
        highway_station->data.market.fetch_inventory_id = fetch_inventory;
        return info[fetch_inventory].building_id;
    }
    return 0;
}
