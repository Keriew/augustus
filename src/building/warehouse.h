#ifndef BUILDING_WAREHOUSE_H
#define BUILDING_WAREHOUSE_H

#include "building/building.h"
#include "building/storage.h"
#include "map/point.h"

#define FULL_WAREHOUSE 32
#define THREEQ_WAREHOUSE 24
#define HALF_WAREHOUSE 16
#define QUARTER_WAREHOUSE 8

enum {
    WAREHOUSE_REMOVING_RESOURCE = 0,
    WAREHOUSE_ADDING_RESOURCE = 1
};

enum {
    WAREHOUSE_ROOM = 0,
    WAREHOUSE_FULL = 1,
    WAREHOUSE_SOME_ROOM = 2
};

enum {
    WAREHOUSE_TASK_NONE = -1,
    WAREHOUSE_TASK_GETTING = 0,
    WAREHOUSE_TASK_DELIVERING = 1
};

/*
Single warehouse functions
*/
int building_warehouse_get_space_info(building *warehouse);
int building_warehouse_get_amount(building *warehouse, int resource);
int building_warehouse_get_available_amount(building *warehouse, int resource);
int building_warehouse_try_add_resource(building *b, int resource, int quantity);
int building_warehouse_maximum_receptible_amount(building *b, int resource);
int building_warehouse_try_remove_resource(building *warehouse, int resource, int desired_amount);
void building_warehouse_remove_resource_curse(building *warehouse, int amount);
int building_warehouse_add_import(building *warehouse, int resource, int amount, int land_trader);
int building_warehouse_remove_export(building *warehouse, int resource, int amount, int land_trader);
void building_warehouse_recount_resources(building *main);
int building_warehouse_accepts_storage(building *b, int resource, int *understaffed);
int building_warehouse_amount_can_get_from(building *destination, int resource);
int building_warehouse_for_getting(building *src, int resource, map_point *dst);
int building_warehouse_determine_worker_task(building *warehouse, int *resource);

/*
Find warehouse functions
*/
int building_warehouse_for_storing(int src_building_id, int x, int y, int resource, int road_network_id,
                                   int *understaffed, map_point *dst);
int building_warehouse_with_resource(int x, int y, int resource, int road_network_id, int *understaffed, map_point *dst, building_storage_permission_states p);

/*
Multiple/global warehouse functions
*/
int building_warehouses_remove_resource(int resource, int amount);
int building_warehouses_count_available_resource(int resource, int respect_maintaining);
int building_warehouses_send_resources_to_rome(int resource, int amount);
int building_warehouses_add_resource(int resource, int amount);

#endif // BUILDING_WAREHOUSE_H
