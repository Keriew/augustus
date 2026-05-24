#ifndef BUILDING_HIGHWAY_STATION_H
#define BUILDING_HIGHWAY_STATION_H

#include "building/building.h"

#define HIGHWAY_STATION_HIGHWAY_BLOCKS_PER_UNIT 50
#define HIGHWAY_STATION_RESOURCE_PER_LOAD 100
#define HIGHWAY_STATION_STOCK_MONTHS 6

int building_highway_station_is_functional(building *b);

int building_highway_station_highway_blocks(void);

int building_highway_station_monthly_need(void);

int building_highway_station_max_stock(void);

void building_highway_station_consume_monthly(void);

int building_highway_station_get_storage_destination(building *highway_station);

void building_highway_station_refresh_graphic(building *b);

#endif // BUILDING_HIGHWAY_STATION_H
