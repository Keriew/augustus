#include "warehouse.h"

#include "building/barracks.h"
#include "building/count.h"
#include "building/granary.h"
#include "building/industry.h"
#include "building/monument.h"
#include "building/model.h"
#include "building/storage.h"
#include "city/finance.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/image.h"
#include "empire/trade_prices.h"
#include "figure/figure.h"
#include "game/tutorial.h"
#include "map/image.h"
#include "scenario/property.h"

#define INFINITE 10000
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MAX_CARTLOADS_PER_SPACE 4

static void building_warehouse_space_set_image(building *space, int resource);

int building_warehouse_get_space_info(building *warehouse)
{
    int total_loads = 0;
    int empty_spaces = 0;
    building *space = warehouse;
    for (int i = 0; i < 8; i++) {
        space = building_next(space);
        if (space->id <= 0) {
            return 0;
        }
        if (space->subtype.warehouse_resource_id) {
            total_loads += space->resources[space->subtype.warehouse_resource_id];
        } else {
            empty_spaces++;
        }
    }
    if (empty_spaces > 0) {
        return WAREHOUSE_ROOM;
    } else if (total_loads < FULL_WAREHOUSE) {
        return WAREHOUSE_SOME_ROOM;
    } else {
        return WAREHOUSE_FULL;
    }
}

int building_warehouse_get_amount(building *warehouse, int resource)
{
    int loads = 0;
    building *space = warehouse;
    for (int i = 0; i < 8; i++) {
        space = building_next(space);
        if (space->id <= 0) {
            return 0;
        }
        if (space->subtype.warehouse_resource_id && space->subtype.warehouse_resource_id == resource) {
            loads += space->resources[resource];
        }
    }
    return loads;
}

int building_warehouse_get_available_amount(building *warehouse, int resource)
{
    if (warehouse->state != BUILDING_STATE_IN_USE || warehouse->has_plague) {
        return 0;
    }

    if (building_storage_get_state(warehouse, resource, 1) == BUILDING_STORAGE_STATE_MAINTAINING) {
        return 0;
    }

    return building_warehouse_get_amount(warehouse, resource);
}

static building *building_warehouse_find_space(building *warehouse, int resource, int adding)
{
    if (!warehouse || warehouse->id <= 0) {
        return 0;
    }
    building *space = building_main(warehouse);

    if (adding) {
        // Step 1: Try partially filled bay
        for (int i = 0; i < 8; i++) {
            space = building_next(space);
            if (!space->id) {
                return 0;
            }
            if (space->subtype.warehouse_resource_id == resource &&
                space->resources[resource] < MAX_CARTLOADS_PER_SPACE) {
                return space;
            }
        }

        // Step 2: Try empty or assignable bay
        space = building_main(warehouse);
        for (int i = 0; i < 8; i++) {
            space = building_next(space);
            if (!space->id) {
                return 0;
            }
            if ((space->subtype.warehouse_resource_id == RESOURCE_NONE ||
                space->subtype.warehouse_resource_id == resource) &&
                space->resources[resource] < MAX_CARTLOADS_PER_SPACE) {
                return space;
            }
        }
    } else {
        // Removing: find first bay with at least one unit of the resource
        for (int i = 0; i < 8; i++) {
            space = building_next(space);
            if (!space->id) {
                return 0;
            }
            if (space->subtype.warehouse_resource_id == resource &&
                space->resources[resource] > 0) {
                return space;
            }
        }
    }
    return 0;
}

void building_warehouse_recount_resources(building *main)
{
    //helper to reflect the resources in the main warehouse, like granary does
    if (!main || main->type != BUILDING_WAREHOUSE) {
        return;
    }

    // Reset all resource counters in the main warehouse
    for (int r = 0; r < RESOURCE_MAX; r++) {
        main->resources[r] = 0;
    }

    building *space = main;
    for (int i = 0; i < 8; i++) {
        space = building_next(space);
        if (space->id <= 0) {
            continue;
        }

        int resource = space->subtype.warehouse_resource_id;
        if (resource > RESOURCE_NONE && resource < RESOURCE_MAX) {
            main->resources[resource] += space->resources[resource];
        }
        building_warehouse_space_set_image(space, resource);
    }
    // Total sum of all loads (regardless of type)
    int total_loads = 0;
    for (int r = 1; r < RESOURCE_MAX; r++) {
        total_loads += main->resources[r];
    }
    main->resources[RESOURCE_NONE] = BUILDING_STORAGE_QUANTITY_MAX - total_loads;
}

int building_warehouse_try_add_resource(building *b, int resource, int quantity)
{
    if (!b || b->id <= 0 || quantity <= 0 || !resource) {
        return 0;
    }
    signed short max_acceptable = building_warehouse_maximum_receptible_amount(b, resource);
    if (!max_acceptable) {
        return 0;
    }
    if (quantity > max_acceptable) { //if trying to add more than acceptable, limit it
        quantity = max_acceptable;
    }
    signed short added = 0;
    while (added < quantity) {
        building *space = building_warehouse_find_space(b, resource, 1);
        if (!space) {
            break;
        }
        signed short space_remaining = MAX_CARTLOADS_PER_SPACE - space->resources[resource];
        signed short to_add = (quantity - added < space_remaining) ? (quantity - added) : space_remaining;

        space->resources[resource] += to_add;
        space->subtype.warehouse_resource_id = resource;
        added += to_add;

        city_resource_add_to_warehouse(resource, to_add);
        building_warehouse_space_set_image(space, resource);
    }

    if (added) {
        tutorial_on_add_to_warehouse();
    }
    return added;
}

int building_warehouses_add_resource(int resource, int amount)
{
    if (amount <= 0) {
        return 0;
    }

    for (building *b = building_first_of_type(BUILDING_WAREHOUSE); b && amount > 0; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }

        int was_added = building_warehouse_try_add_resource(b, resource, amount);
        amount -= was_added;
    }

    return amount;
}

int building_warehouse_try_remove_resource(building *warehouse, int resource, int desired_amount)
{
    if (desired_amount <= 0 || !resource) {
        return 0;
    }
    if (warehouse->has_plague) {
        return 0;
    }
    int remaining_desired = desired_amount;
    int removed_amount = 0;
    building *space = warehouse;
    for (int i = 0; i < 8; i++) {
        if (remaining_desired <= 0) {
            return removed_amount;
        }
        space = building_next(space);
        if (space->id <= 0) {
            continue;
        }
        if (space->subtype.warehouse_resource_id != resource || space->resources[resource] <= 0) {
            continue;
        }
        if (space->resources[resource] > remaining_desired) {
            removed_amount += remaining_desired;
            city_resource_remove_from_warehouse(resource, remaining_desired);
            space->resources[resource] -= remaining_desired;
            remaining_desired = 0;
        } else {
            removed_amount += space->resources[resource];
            city_resource_remove_from_warehouse(resource, space->resources[resource]);
            remaining_desired -= space->resources[resource];
            space->resources[resource] = 0;
            space->subtype.warehouse_resource_id = RESOURCE_NONE;
        }
        building_warehouse_space_set_image(space, resource);
    }
    return removed_amount;
}

void building_warehouse_remove_resource_curse(building *warehouse, int amount)
{
    if (warehouse->type != BUILDING_WAREHOUSE) {
        return;
    }

    building *space = warehouse;
    for (int i = 0; i < 8 && amount > 0; i++) {
        space = building_next(space);
        int resource = space->subtype.warehouse_resource_id;
        if (space->id <= 0 || space->resources[resource] <= 0) {
            continue;
        }
        if (space->resources[resource] > amount) {
            city_resource_remove_from_warehouse(resource, amount);
            space->resources[resource] -= amount;
            amount = 0;
        } else {
            city_resource_remove_from_warehouse(resource, space->resources[resource]);
            amount -= space->resources[resource];
            space->resources[resource] = 0;
            space->subtype.warehouse_resource_id = RESOURCE_NONE;
        }
        building_warehouse_space_set_image(space, resource);
    }
}

static void building_warehouse_space_set_image(building *space, int resource)
{   //keep this function static - external files should not set warehouse images directly
    int image_id;
    if (building_loads_stored(space) <= 0) {
        image_id = image_group(GROUP_BUILDING_WAREHOUSE_STORAGE_EMPTY);
    } else {
        image_id = resource_get_data(resource)->image.storage + space->resources[resource] - 1;
    }
    map_image_set(space->grid_offset, image_id);
}

int building_warehouse_add_import(building *warehouse, int resource, int amount, int land_trader)
{
    if (warehouse->type != BUILDING_WAREHOUSE) {
        building *main_warehouse = building_main(warehouse);
        if (main_warehouse->type == BUILDING_WAREHOUSE) {
            warehouse = main_warehouse;
            // account for fetched warehouse space instead of main warehouse
        }
    }
    building_storage_permission_states permission = land_trader ?
        BUILDING_STORAGE_PERMISSION_TRADERS : BUILDING_STORAGE_PERMISSION_DOCK;
    if (!building_storage_get_permission(permission, warehouse)) {
        return 0; // cannot import to this warehouse
    }
    if (building_storage_get_state(warehouse, resource, 1) == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return 0; // cannot accept this resource
    }
    int added_amount = building_warehouse_try_add_resource(warehouse, resource, 1);
    if (added_amount <= 0) {
        return 0; // no space to add
    }
    return 1;
}

int building_warehouse_remove_export(building *warehouse, int resource, int amount, int land_trader)
{
    if (warehouse->type != BUILDING_WAREHOUSE) {
        building *main_warehouse = building_main(warehouse);
        if (main_warehouse->type == BUILDING_WAREHOUSE) {
            warehouse = main_warehouse;
            // account for fetched warehouse space instead of main warehouse
        }
    }
    if (resource == RESOURCE_NONE || amount <= 0) {
        return 0; // invalid resource or amount
    }
    building_storage_permission_states permission;
    switch (land_trader) {
        case 0: // sea trader
            permission = BUILDING_STORAGE_PERMISSION_DOCK;
            break;
        case 1: // land trader
            permission = BUILDING_STORAGE_PERMISSION_TRADERS;
            break;
        case -1: //native trader
            permission = BUILDING_STORAGE_PERMISSION_NATIVES;
            land_trader = 1; // native trader is always land trader
            break;
    }

    if (!building_storage_get_permission(permission, warehouse)) {
        return 0; // cannot export from this warehouse
    }
    int removed_amount = building_warehouse_try_remove_resource(warehouse, resource, amount);
    int price = trade_price_sell(resource, land_trader);
    city_finance_process_export(price * removed_amount);
    return removed_amount;
}

static building *get_next_warehouse(void)
{
    unsigned int building_id = city_resource_last_used_warehouse();
    int wrapped_around = 0;
    building *b = building_first_of_type(BUILDING_WAREHOUSE);
    while (b) {
        if (b->state == BUILDING_STATE_IN_USE && (b->id > building_id || wrapped_around)) {
            return b;
        }
        if (!b->next_of_type) {
            if (wrapped_around) {
                return 0;
            }
            wrapped_around = 1;
            b = building_first_of_type(BUILDING_WAREHOUSE);
        } else {
            b = b->next_of_type;
        }
    }
    return 0;
}

static int warehouse_allows_getting(building *b, int resource)
{
    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];

    if (b->has_plague || (entry->state >= BUILDING_STORAGE_STATE_GETTING)) {
        return 0;
        //if the building has plague or gets or maintains resource - it doesnt allow getting
    }

    return 1;
}

static int get_acceptable_quantity(building *b, int resource)
{
    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];

    const building_storage_state state = building_storage_get_state(b, resource, 1);
    if (state == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return 0; // not accepting this resource
    } else {
        return entry->quantity;
    }
}

static int building_warehouse_max_space_for_resource(building *b, int resource)
{
    // internal function to check space with respect to tiled storage - keep static
    int max_storable = 0;
    building *space = b;
    for (int i = 0; i < 8; i++) {
        space = building_next(space);
        if (space->id <= 0) {
            return 0;
        }
        if (space->subtype.warehouse_resource_id == resource) {
            max_storable += MAX_CARTLOADS_PER_SPACE - space->resources[resource];
        }
        if (space->subtype.warehouse_resource_id == RESOURCE_NONE) {
            max_storable += MAX_CARTLOADS_PER_SPACE;
        }
    }
    return max_storable;
}

int building_warehouse_maximum_receptible_amount(building *b, int resource)
{
    building_warehouse_recount_resources(b);
    if (b->has_plague || building_storage_get_empty_all(b->id) ||
         b->state != BUILDING_STATE_IN_USE || b->resources[RESOURCE_NONE] <= 0) {
        return 0;
    }
    if (building_storage_get_state(b, resource, 1) == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return 0; // early check for relative state
    }
    unsigned char max_allowed = get_acceptable_quantity(b, resource);
    unsigned char current_amount = building_warehouse_get_amount(b, resource);
    unsigned char remaining_allowed = (max_allowed > current_amount) ? (max_allowed - current_amount) : 0;

    unsigned char resource_space_limit = building_warehouse_max_space_for_resource(b, resource); // max by tile layout
    unsigned char free_space_overall = b->resources[RESOURCE_NONE]; // total free space

    unsigned char available_space = MIN(free_space_overall, resource_space_limit); // tile storage and free space
    unsigned char max_receptible = MIN(remaining_allowed, available_space);
    // Max the building is allowed to receive, considering all limits
    // allowed remaining is the amount that can be added to the warehouse considering set limit and current storage
    max_receptible = max_receptible < 0 ? 0 : max_receptible; // in case current storage exceeds limits, 0

    return  max_receptible;
}

int building_warehouses_count_available_resource(int resource, int respect_maintaining)
{
    int total = 0;
    building *b = get_next_warehouse();
    if (!b) {
        return 0;
    }
    building *initial_warehouse = b;

    do {
        if (b->state == BUILDING_STATE_IN_USE) {
            if (!respect_maintaining ||
                building_storage_get_state(b, resource, 1) != BUILDING_STORAGE_STATE_MAINTAINING ||
                building_storage_get_empty_all(b->id)) {
                total += building_warehouse_get_amount(b, resource);
            }
        }
        b = b->next_of_type ? b->next_of_type : building_first_of_type(BUILDING_WAREHOUSE);
    } while (b != initial_warehouse);

    return total;
}

static void try_create_cart_to_rome(building *b, int resource, int loads)
{
    map_point road;
    if (map_has_road_access_rotation(b->subtype.orientation, b->x, b->y, 3, &road)) {
        figure *f = figure_create(FIGURE_CART_PUSHER, road.x, road.y, DIR_4_BOTTOM);
        f->action_state = FIGURE_ACTION_234_CARTPUSHER_GOING_TO_ROME_CREATED;
        f->resource_id = resource;
        f->loads_sold_or_carrying = loads;
        f->building_id = b->id;
    }
}

int building_warehouses_send_resources_to_rome(int resource, int amount)
{
    building *b = get_next_warehouse();
    if (!b) {
        return amount;
    }
    building *initial_warehouse = b;

    // First go for non-getting warehouses
    do {
        if (b->state == BUILDING_STATE_IN_USE) {
            if (warehouse_allows_getting(b, resource)) {
                city_resource_set_last_used_warehouse(b->id);
                int taken_loads = building_warehouse_try_remove_resource(b, resource, amount);
                amount -= taken_loads;
                if (taken_loads) {
                    try_create_cart_to_rome(b, resource, taken_loads);
                }
            }
        }
        b = b->next_of_type ? b->next_of_type : building_first_of_type(BUILDING_WAREHOUSE);
    } while (b != initial_warehouse && amount > 0);

    if (amount <= 0) {
        return 0;
    }

    // If that doesn't work, take it anyway
    do {
        if ((b->state == BUILDING_STATE_IN_USE) &&
         (building_storage_get_state(b, resource, 1) < BUILDING_STORAGE_STATE_MAINTAINING)) {
            city_resource_set_last_used_warehouse(b->id);
            int taken_loads = building_warehouse_try_remove_resource(b, resource, amount);
            amount -= taken_loads;
            if (taken_loads) {
                try_create_cart_to_rome(b, resource, taken_loads);
            }
        }
        b = b->next_of_type ? b->next_of_type : building_first_of_type(BUILDING_WAREHOUSE);
    } while (b != initial_warehouse && amount > 0);

    return amount;
}

int building_warehouses_remove_resource(int resource, int amount)
{
    building *b = get_next_warehouse();
    if (!b) {
        return amount;
    }
    building *initial_warehouse = b;

    // First go for non-getting, non-maintaining warehouses
    do {
        if (b->state == BUILDING_STATE_IN_USE) {
            if (building_storage_get_state(b, resource, 1) < BUILDING_STORAGE_STATE_GETTING) {
                city_resource_set_last_used_warehouse(b->id);
                amount = building_warehouse_try_remove_resource(b, resource, amount);
            }
        }
        b = b->next_of_type ? b->next_of_type : building_first_of_type(BUILDING_WAREHOUSE);
    } while (b != initial_warehouse && amount > 0);

    if (amount <= 0) {
        return 0;
    }

    // If that doesn't work, take it anyway
    do {
        if (b->state == BUILDING_STATE_IN_USE) {
            city_resource_set_last_used_warehouse(b->id);
            amount = building_warehouse_try_remove_resource(b, resource, amount);
        }
        b = b->next_of_type ? b->next_of_type : building_first_of_type(BUILDING_WAREHOUSE);
    } while (b != initial_warehouse && amount > 0);

    return amount;
}

int building_warehouse_accepts_storage(building *b, int resource, int *understaffed)
{
    if (b->state != BUILDING_STATE_IN_USE || b->type != BUILDING_WAREHOUSE ||
        !b->has_road_access || b->distance_from_entry <= 0 || b->has_plague) {
        return 0;
    }
    if (building_storage_get_state(b, resource, 1) == BUILDING_STORAGE_STATE_NOT_ACCEPTING ||
        building_storage_get_empty_all(b->id)) {
        return 0;
    }
    int pct_workers = calc_percentage(b->num_workers, model_get_building(b->type)->laborers);
    if (pct_workers < 100) {
        if (understaffed) {
            *understaffed += 1;
        }
        return 0;
    }
    if (building_warehouse_max_space_for_resource(b, resource)) {
        return 1;
    }
    return 0;
}

int building_warehouse_for_storing(int src_building_id, int x, int y, int resource, int road_network_id,
    int *understaffed, map_point *dst)
{
    int min_dist = INFINITE;
    int min_building_id = 0;
    for (building *b = building_first_of_type(BUILDING_WAREHOUSE); b; b = b->next_of_type) {
        if (b->id == src_building_id || (road_network_id != -1 && b->road_network_id != road_network_id) ||
            !building_warehouse_accepts_storage(b, resource, understaffed) ||
            (building_warehouse_maximum_receptible_amount(b, resource) <= 0)) {
            continue;
        }
        int dist = calc_maximum_distance(b->x, b->y, x, y);
        if (dist < min_dist) {
            min_dist = dist;
            min_building_id = b->id;
        }
    }
    building *b = building_get(min_building_id);
    if (b->has_road_access == 1) {
        map_point_store_result(b->x, b->y, dst);
    } else if (!map_has_road_access_rotation(b->subtype.orientation, b->x, b->y, 3, dst)) {
        return 0;
    }
    return min_building_id;
}

int building_warehouse_amount_can_get_from(building *destination, int resource)
{
    int loads_stored = 0;
    building *space = destination;
    for (int t = 0; t < 8; t++) {
        space = building_next(space);
        if (space->id > 0 && space->subtype.warehouse_resource_id == resource) {
            loads_stored += space->resources[resource];
        }
    }
    return loads_stored;
}

int building_warehouse_for_getting(building *src, int resource, map_point *dst)
{
    int min_dist = INFINITE;
    building *min_building = 0;
    for (building *b = building_first_of_type(BUILDING_WAREHOUSE); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE || b->has_plague) {
            continue;
        }
        if (b->id == src->id) {
            continue;
        }
        int loads_stored = building_warehouse_amount_can_get_from(b, resource);
        if (loads_stored > 0 && warehouse_allows_getting(b, resource)) {
            int dist = calc_maximum_distance(b->x, b->y, src->x, src->y);
            dist -= 4 * loads_stored;
            if (dist < min_dist) {
                min_dist = dist;
                min_building = b;
            }
        }
    }
    if (min_building) {
        if (dst) {
            map_point_store_result(min_building->road_access_x, min_building->road_access_y, dst);
        }
        return min_building->id;
    } else {
        return 0;
    }
}

int building_warehouse_with_resource(int x, int y, int resource, int road_network_id,
     int *understaffed, map_point *dst, building_storage_permission_states p)
{
    int min_dist = INFINITE;
    building *min_building = 0;
    for (building *b = building_first_of_type(BUILDING_WAREHOUSE); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE || b->has_plague) {
            continue;
        }
        if (!b->has_road_access || b->distance_from_entry <= 0 || b->road_network_id != road_network_id) {
            continue;
        }
        if (!building_storage_get_permission(p, b)) {
            continue;
        }

        int pct_workers = calc_percentage(b->num_workers, model_get_building(b->type)->laborers);
        if (pct_workers < 100) {
            if (understaffed) {
                *understaffed += 1;
            }
            continue;
        }
        int loads_stored = 0;
        building *space = b;
        for (int t = 0; t < 8; t++) {
            space = building_next(space);
            if (space->id > 0 && space->subtype.warehouse_resource_id == resource) {
                loads_stored += space->resources[resource];
            }
        }
        if (loads_stored > 0) {
            int dist = calc_maximum_distance(b->x, b->y, x, y);
            dist -= 2 * loads_stored;
            if (dist < min_dist) {
                min_dist = dist;
                min_building = b;
            }
        }
    }
    if (min_building) {
        if (dst) {
            map_point_store_result(min_building->road_access_x, min_building->road_access_y, dst);
        }
        return min_building->id;
    } else {
        return 0;
    }
}

int building_warehouse_determine_worker_task(building *warehouse, int *resource)
{
    int pct_workers = calc_percentage(warehouse->num_workers, model_get_building(warehouse->type)->laborers);
    if (pct_workers < 50) {
        return WAREHOUSE_TASK_NONE;
    }

    building *space;
    //TASK 1: emptying takes priority
    if (building_storage_get_empty_all(warehouse->id)) {
        resource_type resource_to_empty = building_storage_get_highest_quantity_resource(warehouse);
        if (resource_to_empty) {
            space = building_warehouse_find_space(warehouse, resource_to_empty, 0);

            if (space->resources[resource_to_empty]) {
                *resource = resource_to_empty;
                return WAREHOUSE_TASK_DELIVERING;
            }

        }
    }
    //TASK 2: getting resources
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        //determine if any of the resources need to be fetched becasuse of 'getting'
        if (building_storage_get_state(warehouse, r, 1) != BUILDING_STORAGE_STATE_GETTING ||
            city_resource_is_stockpiled(r) || !resource_is_storable(r)) {
            continue;
        }
        unsigned char needed = building_warehouse_maximum_receptible_amount(warehouse, r);

        int fetch_amount = MAX_CARTLOADS_PER_SPACE;
        if (needed >= fetch_amount && fetch_amount > 0) {
            if (!building_warehouse_for_getting(warehouse, r, 0)) {
                continue;
            }
            *resource = r;
            return WAREHOUSE_TASK_GETTING;
        }

    }
    // TASK 3: delivering resources
    if (!building_storage_get_permission(BUILDING_STORAGE_PERMISSION_WORKER, warehouse)) {
        return WAREHOUSE_TASK_NONE; //halt resource delivery to workshops and granaries
    }
    // deliver raw materials to workshops
    for (int r = RESOURCE_MIN_NON_FOOD; r < RESOURCE_MAX_NON_FOOD; r++) {
        if (warehouse->resources[r] <= 0 || !resource_is_raw_material(r) || city_resource_is_stockpiled(r) ||
             building_storage_get_state(warehouse, r, 1) == BUILDING_STORAGE_STATE_MAINTAINING ||
             !building_has_workshop_for_raw_material_with_room(r, warehouse->road_network_id)) {
            continue; // skip if no resource, not raw material,maintaining, stockpiled or no workshop
        }
        *resource = r;
        return WAREHOUSE_TASK_DELIVERING;
    }
    // deliver food to granaries
    unsigned char delivering_food = 0;
    for (int r = RESOURCE_MIN_FOOD; r < RESOURCE_MAX_FOOD; r++) {
        if (warehouse->resources[r] <= 0 || !resource_is_food(r) || city_resource_is_stockpiled(r) ||
            building_storage_get_state(warehouse, r, 1) == BUILDING_STORAGE_STATE_MAINTAINING) {
            continue; // skip if no resource, not food,maintaining, stockpiled 
        }
        if (building_granary_get_granary_needing_food(warehouse, r, 1)) {
            *resource = r;
            delivering_food = 1;
            break; //found a getting granary in need of food
        } else if (building_granary_get_granary_needing_food(warehouse, r, 0)) {
            *resource = r; // keep checking in case there is a getting granary
            delivering_food = 1;
        }

    }
    if (delivering_food) {
        return WAREHOUSE_TASK_DELIVERING;
    }
    // Idle
    return WAREHOUSE_TASK_NONE;
}
