#ifndef BUILDING_GRANARY_H
#define BUILDING_GRANARY_H

#include "building/building.h"
#include "map/point.h"

// make sure to update src/window/building/distribution.c so the number renders correctly
#define FULL_GRANARY 32
#define THREEQUARTERS_GRANARY 24
#define HALF_GRANARY 16
#define QUARTER_GRANARY 8

enum {
  GRANARY_TASK_NONE = -1,
  GRANARY_TASK_GETTING = 0
};

/*---------------------------------------*
 * Stock & capacity
 *---------------------------------------*/

 /**
  * @brief Get the amount of a specific resource in a granary.
  * @param b
  * @param resource
  * @return Amount of the resource in the granary.
  */
int building_granary_get_amount(building *b, int resource);

/**
 * @brief Get the amount of free space in the granary overall.
 * @param b
 * @param resource
 * @return Amount of the free space in the granary.
 */
int building_granary_get_free_space_amount(building *b);

/**
 * @brief Count available (deliverable) amount in a granary.
 * @param b
 * @param resource
 * @param respect_maintaining
 * @return Amount of the resource in the granary that can be delivered.
 */
int building_granary_count_available_resource(building *b, int resource, int respect_maintaining);

/**
 * @brief Sum available amounts across all granaries.
 * @param resource
 * @param respect_maintaining
 * @param caesars_request If true, count only granaries that allow Caesar to take resources
 * @return available amount across all granaries
 */
int building_granaries_count_available_resource(int resource, int respect_maintaining, int caesars_request);

/**
 * @brief Calculate the maximum amount of a resource that can be added to a granary.
 * @param b
 * @param resource
 * @return amount that can be added, 0 if none
 */
int building_granary_maximum_receptible_amount(building *b, int resource);

/*---------------------------------------*
 * Acceptance / storage checks
 *---------------------------------------*/

 /**
  * @brief Check if a granary accepts storing a resource.
  * @param b
  * @param resource
  * @param understaffed
  * @return free space >= ONE_CARTLOAD (1 or 0)
  */
int building_granary_accepts_storage(building *b, int resource, int *understaffed);

/*-----------------------*
 * Adding / importing
 *-----------------------*/

 /**
  * @brief Add imported resources to a granary.
  * @param granary
  * @param resource
  * @param amount
  * @param land_trader
  * @return amount added, 0 on failure
  */
int building_granary_add_import(building *granary, int resource, int amount, int land_trader);

/**
 * @brief Try to add a resource amount to a single granary.
 * @param granary
 * @param resource
 * @param amount
 * @param is_produced
 * @param respect_settings
 * @return amount added
 */
int building_granary_try_add_resource(building *granary, int resource, int amount, int is_produced, int respect_settings);

/**
 * @brief Try to add a resource amount across available granaries.
 * @param resource
 * @param amount
 * @param respect_settings
 */
int building_granaries_add_resource(int resource, int amount, int respect_settings);

/*------------------------*
 * Removing / exporting
 *------------------------*/

 /**
  * @brief Remove exported resources from a granary.
  * @param granary
  * @param resource
  * @param amount
  * @param land_trader
  * @return amount removed, 0 on failure
  */
int building_granary_remove_export(building *granary, int resource, int amount, int land_trader);

/**
 * @brief Try to remove a resource amount from a single granary.
 * @param granary
 * @param resource
 * @param desired_amount
 */
int building_granary_try_remove_resource(building *granary, int resource, int desired_amount);

/**
 * @brief Try to remove a resource amount across available granaries.
 * @param resource
 * @param amount
 * @return remaining amount that could not be removed, or 0 all requested amount removed
 */
int building_granaries_remove_resource(int resource, int amount);

/*---------------------------------------*
 * Routing and Building finding
 *---------------------------------------*/

 /**
  * @brief Find a granary matching need.
  * @param source
  * @param resource
  * @param getting
  */
building *building_granary_get_granary_needing_food(building *source, int resource, int getting);

/**
 * @brief Return ID of a granary that is accepting the resource.
 * @param x
 * @param y
 * @param resource
 * @param road_network_id
 * @param force_on_stockpile
 * @param understaffed
 * @param dst
 * @return ID of the granary, or 0 if none found
 */
int building_granary_for_storing(int x, int y, int resource, int road_network_id,
                                 int force_on_stockpile, int *understaffed, map_point *dst);

/**
 * @brief Return ID of a granary that is getting the resource.
 * @param x
 * @param y
 * @param resource
 * @param road_network_id
 * @param dst
 */
int building_getting_granary_for_storing(int x, int y, int resource, int road_network_id, map_point *dst);

/**
 * @brief Compute how much can be obtained from origin for destination.
 * @param destination
 * @param origin
 * @param resource
 * @return amount of resource that can be gotten from origin granary to destination granary
 */
int building_granary_amount_can_get_from(building *destination, building *origin, int resource);

/**
 * @brief For a GETTING granary, find a source granary to get from.
 * @param src
 * @param dst
 * @param min_amount
 * @return ID of the granary, or 0 if none found
 */
int building_granary_for_getting(building *src, map_point *dst, int min_amount);

/**
 * @brief Remove resource from src for a GETTING deliveryman.
 * @param src
 * @param dst
 * @param resource
 * @return amount taken by the deliveryman
 */
int building_granary_remove_for_getting_deliveryman(building *src, building *dst, int *resource);

/*----------------------*
 * Worker / task logic
 *----------------------*/

 /**
  * @brief Determine the current task for a granary worker.
  * @param granary
  * @return granary task enum value
  */
int building_granary_determine_worker_task(building *granary);

/*------------------------------*
 * Global updates & operations
 *------------------------------*/

 /**
  * @brief Rebuild internal lists and recompute stocks.
  */
void building_granaries_calculate_stocks(void);


/**
 * @brief Save-game helper: update the capacity of all built granaries.
 */
void building_granary_update_built_granaries_capacity(void);

/*----------------------*
 * Requests to Rome
 *----------------------*/

 /**
  * @brief Create a cart pusher to Rome with the requested resources.
  * Should be brought together into one with building_warehouses_send_resources_to_rome(), atm they are separate.
  * @param resource
  * @param amount
  * @return amount that couldnt be sent, or 0 if all sent
  */
int building_granaries_send_resources_to_rome(int resource, int amount);

/*----------------------*
 * Blessing / Cursing
 *----------------------*/

 /**
  * @brief Legacy (no longer used since Mercury blessing changed).
  */
void building_granary_bless(void);

/**
 * @brief
 * @param big
 */
void building_granary_warehouse_curse(int big);


#endif // BUILDING_GRANARY_H
