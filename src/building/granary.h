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

int building_granary_add_import(building *granary, int resource, int amount, int land_trader);

int building_granary_remove_export(building *granary, int resource, int amount, int land_trader);

int building_granary_try_add_resource(building *granary, int resource, int amount, int is_produced);

int building_granaries_add_resource(int resource, int amount, int respect_settings);

int building_granary_get_amount(building *b, int resource);

int building_granary_try_remove_resource(building *granary, int resource, int desired_amount);

int building_granaries_remove_resource(int resource, int amount);

int building_granary_count_available_resource(building *b, int resource, int respect_maintaining);

int building_granaries_count_available_resource(int resource, int respect_maintaining);

int building_granaries_send_resources_to_rome(int resource, int amount);

int building_granary_remove_for_getting_deliveryman(building *src, building *dst, int *resource);

int building_granary_determine_worker_task(building *granary);

void building_granaries_calculate_stocks(void);

int building_granary_accepts_storage(building *b, int resource, int *understaffed);

building *building_granary_get_granary_needing_food(building *source, int resource, int getting);

int building_granary_for_storing(int x, int y, int resource, int road_network_id,
                                 int force_on_stockpile, int *understaffed, map_point *dst);

int building_getting_granary_for_storing(int x, int y, int resource, int road_network_id, map_point *dst);

int building_granary_amount_can_get_from(building *destination, building *origin, int resource);

int building_granary_for_getting(building *src, map_point *dst, int min_amount);

void building_granary_bless(void);

void building_granary_warehouse_curse(int big);

int building_granary_maximum_receptible_amount(building *b, int);

void building_granary_update_built_granaries_capacity(void);

#endif // BUILDING_GRANARY_H
