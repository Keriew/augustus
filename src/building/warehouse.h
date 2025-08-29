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

/*---------------------------------------*
 * Stock & capacity
 *---------------------------------------*/

 /**
  * @brief Get warehouse space state. Only used for the UI - rightclick drawing of the warehouse info
  * @param warehouse
  * @return One of WAREHOUSE_ROOM, WAREHOUSE_SOME_ROOM, WAREHOUSE_FULL.
  */
int building_warehouse_get_space_info(building *warehouse);

/**
 * @brief Get the amount of a specific resource in a warehouse.
 * TODO: create building_storage helper for this and granary equivalent
 * @param warehouse
 * @param resource
 * @return Amount of the resource in the warehouse.
 */
int building_warehouse_get_amount(building *warehouse, int resource);

/**
 * @brief Get the amount of free space in the warehouse overall.
 * @param b
 * @param resource
 * @return Amount of the free space in the warehouse
 */
int building_warehouse_get_free_space_amount(building *b);

/**
 * @brief Count available (deliverable) amount in a warehouse.
 * TODO: create building_storage helper for this and granary equivalent
 * @param warehouse
 * @param resource
 * @return Amount of the resource in the warehouse that can be delivered.
 */
int building_warehouse_get_available_amount(building *warehouse, int resource);

/**
 * @brief Calculate the maximum amount of a resource that can be added to a warehouse.
 * TODO: create building_storage helper for this and granary equivalent
 * @param b
 * @param resource
 * @return amount that can be added, 0 if none
 */
int building_warehouse_maximum_receptible_amount(building *b, int resource);

/**
 * @brief Sum available amounts across all warehouses.
 * TODO: create building_storage helper for this and granary equivalent
 * @param resource
 * @param respect_maintaining
 * @return available amount across all warehouses
 */
int building_warehouses_count_available_resource(int resource, int respect_maintaining);

/*---------------------------------------*
 * Acceptance / storage checks
 *---------------------------------------*/

 /**
  * @brief Check if a warehouse accepts storing a resource.
  * TODO: create building_storage helper for this and granary equivalent
  * @param b
  * @param resource
  * @param understaffed
  * @return 1 or 0
  */
int building_warehouse_accepts_storage(building *b, int resource, int *understaffed);

/*-----------------------*
 * Adding / importing
 *-----------------------*/

 /**
  * @brief Add imported resources to a warehouse.
  * TODO: create building_storage helper for this and granary equivalent
  * @param warehouse
  * @param resource
  * @param amount
  * @param land_trader
  * @return amount added, 0 on failure
  */
int building_warehouse_add_import(building *warehouse, int resource, int amount, int land_trader);

/**
 * @brief Try to add a resource amount to a single warehouse.
 * TODO: create building_storage helper for this and granary equivalent
 * @param b
 * @param resource
 * @param quantity
 * @param respect_settings
 * @return amount added
 */
int building_warehouse_try_add_resource(building *b, int resource, int quantity, int respect_settings);

/**
 * @brief Try to add a resource amount across available warehouses.
 * TODO: create building_storage helper for this and granary equivalent
 * @param resource
 * @param amount
 * @param respect_settings
 */
int building_warehouses_add_resource(int resource, int amount, int respect_settings);

/*------------------------*
 * Removing / exporting
 *------------------------*/

 /**
  * @brief Remove exported resources from a warehouse.
  * TODO: create building_storage helper for this and granary equivalent
  * @param warehouse
  * @param resource
  * @param amount
  * @param land_trader
  * @return amount removed, 0 on failure
  */
int building_warehouse_remove_export(building *warehouse, int resource, int amount, int land_trader);

/**
 * @brief Try to remove a resource amount from a single warehouse.
 * TODO: create building_storage helper for this and granary equivalent
 * @param warehouse
 * @param resource
 * @param desired_amount
 */
int building_warehouse_try_remove_resource(building *warehouse, int resource, int desired_amount);

/**
 * @brief Apply a curse effect that removes resources from a warehouse.
 * TODO: create building_storage helper for this and granary equivalent
 * @param warehouse
 * @param amount
 */
void building_warehouse_remove_resource_curse(building *warehouse, int amount);

/**
 * @brief Try to remove a resource amount across available warehouses.
 * TODO: create building_storage helper for this and granary equivalent
 * @param resource
 * @param amount
 * @return remaining amount that could not be removed, or 0 all requested amount removed
 */
int building_warehouses_remove_resource(int resource, int amount);

/*---------------------------------------*
 * Routing and Building finding
 *---------------------------------------*/

 /**
  * @brief Compute how much can be obtained for a destination warehouse.
  * TODO: create building_storage helper for this and granary equivalent
  * @param destination
  * @param resource
  * @return Amount obtainable per current settings/state.
  */
int building_warehouse_amount_can_get_from(building *destination, int resource);

/**
 * @brief For a GETTING warehouse, find a source warehouse to get from.
 * TODO: create building_storage helper for this and granary equivalent
 * @param src
 * @param resource
 * @param dst
 * @return ID of the warehouse, or 0 if none found
 */
int building_warehouse_for_getting(building *src, int resource, map_point *dst);

/**
 * @brief Return ID of a warehouse that is accepting the resource.
 * TODO: create building_storage helper for this and granary equivalent
 * @param src_building_id
 * @param x
 * @param y
 * @param resource
 * @param road_network_id
 * @param understaffed
 * @param dst
 * @return ID of the warehouse, or 0 if none found
 */
int building_warehouse_for_storing(int src_building_id, int x, int y, int resource, int road_network_id,
                                   int *understaffed, map_point *dst);

/**
 * @brief Find a warehouse with the resource, honoring permissions.
 * @param x
 * @param y
 * @param resource
 * @param road_network_id
 * @param understaffed
 * @param dst
 * @param p
 * @return ID of the warehouse, or 0 if none found
 */
int building_warehouse_with_resource(int x, int y, int resource, int road_network_id, int *understaffed, map_point *dst, building_storage_permission_states p);

/*----------------------*
 * Worker / task logic
 *----------------------*/

 /**
  * @brief Determine the current task for a warehouse worker.
  * TODO: create building_storage helper for this and granary equivalent
  * @param warehouse
  * @param resource
  * @return warehouse task enum value
  */
int building_warehouse_determine_worker_task(building *warehouse, int *resource);

/*------------------------------*
 * Global updates & operations
 *------------------------------*/

 /**
  * @brief Recount resources for a warehouse group.
  * TODO: create building_storage helper for this and granary equivalent
  * @param main
  */
void building_warehouse_recount_resources(building *main);

/*----------------------*
 * Requests to Rome
 *----------------------*/

 /**
  * @brief Create a cart pusher to Rome with the requested resources.
  * TODO: create building_storage helper for this and granary equivalent
  * @param resource
  * @param amount
  * @return amount that couldnt be sent, or 0 if all sent
  */
int building_warehouses_send_resources_to_rome(int resource, int amount);


#endif // BUILDING_WAREHOUSE_H
