#include "granary.h"

#include "building/destruction.h"
#include "building/model.h"
#include "building/storage.h"
#include "building/warehouse.h"
#include "city/finance.h"
#include "city/map.h"
#include "city/message.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/config.h"
#include "empire/trade_prices.h"
#include "figure/figure.h"
#include "map/grid.h"
#include "map/road_access.h"
#include "map/routing_terrain.h"
#include "scenario/property.h"
#include "sound/effect.h"

#define MAX_GRANARIES 100
#define CURSE_LOADS BUILDING_STORAGE_QUANTITY_MAX / 2
#define INFINITE 10000
#define ONE_CARTLOAD 1 //used to be 100 to equal units; 
//RESOURCE_ONE_LOAD in resource.h still points to 100, since it's used in distribution
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

static struct {
    int building_ids[MAX_GRANARIES];
    int num_items;
    int total_storage[RESOURCE_MAX_FOOD];
} non_getting_granaries;

static int granary_allows_getting(building *b, int resource)
{
    const building_storage_state s = building_storage_get_state(b, resource, 1);

    if (b->has_plague || s >= BUILDING_STORAGE_STATE_GETTING) {
        return 0;
        //if the building has plague or gets or maintains resource - it doesnt allow getting
    }

    return 1;
}

int building_granary_get_amount(building *b, int resource)
{
    return b->resources[resource];
}
int building_granary_get_free_space_amount(building *b)
{
    return b->resources[RESOURCE_NONE];
}


int building_granary_add_import(building *granary, int resource, int amount, int land_trader)
{
    if (!resource) {
        return 0; // invalid resource
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
    if (!building_storage_get_permission(permission, granary)) {
        return 0; // cannot export from this granary
    }
    int amount_added = building_granary_try_add_resource(granary, resource, amount, 0, 1);
    if (!amount_added) {
        return 0;
    }

    int price = trade_price_buy(resource, land_trader);
    city_finance_process_import(price * amount_added);
    return amount_added;
}

int building_granary_remove_export(building *granary, int resource, int amount, int land_trader)
{
    if (!resource) {
        return 0; // invalid resource
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
    if (!building_storage_get_permission(permission, granary)) {
        return 0; // cannot export from this granary
    }
    int removed = building_granary_try_remove_resource(granary, resource, amount);
    if (!removed) {
        return 0;
    }
    int price = trade_price_sell(resource, land_trader) * removed;
    city_finance_process_export(price);
    return removed;
}

int building_granary_try_add_resource(building *granary, int resource, int amount, int is_produced, int respect_settings)
{
    if (granary->id <= 0 || !resource_is_food(resource) || granary->type != BUILDING_GRANARY
    || ((building_storage_get_state(granary, resource, 1) == BUILDING_STORAGE_STATE_NOT_ACCEPTING) && respect_settings)) {
        return 0;
    }
    int amount_added = 0;
    int max_current_capacity = respect_settings ?
        building_granary_maximum_receptible_amount(granary, resource) : building_granary_get_free_space_amount(granary);
    if (amount > max_current_capacity) {
        amount_added = max_current_capacity;
    } else {
        amount_added = amount;
    }
    if (is_produced) {
        city_resource_add_produced_to_granary(amount_added); // add to city production
    }
    city_resource_add_to_granary(resource, amount_added); // add to city stored food data
    if (granary->resources[RESOURCE_NONE] <= amount_added) {
        granary->resources[resource] += granary->resources[RESOURCE_NONE];
        granary->resources[RESOURCE_NONE] = 0;
    } else {
        granary->resources[resource] += amount_added;
        granary->resources[RESOURCE_NONE] -= amount_added;
    }
    return amount_added;
}

int building_granaries_add_resource(int resource, int amount, int respect_settings)
{
    if (!resource_is_food(resource)) {
        return amount;
    }

    for (building *b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE || b->resources[RESOURCE_NONE] <= 0) {
            continue;
        }
        amount -= building_granary_try_add_resource(b, resource, amount, 0, respect_settings);
        if (amount <= 0) {
            break;
        }
    }
    return amount;
}

int building_granary_try_remove_resource(building *granary, int resource, int desired_amount)
{
    if (desired_amount <= 0 || granary->has_plague) {
        return 0;
    }
    int removed;
    if (granary->resources[resource] >= desired_amount) {
        removed = desired_amount;
    } else {
        removed = granary->resources[resource];
    }
    city_resource_remove_from_granary(resource, removed);
    granary->resources[resource] -= removed;
    granary->resources[RESOURCE_NONE] += removed;
    return removed;
}

int building_granaries_remove_resource(int resource, int amount)
{
    // first go for non-getting, non-maintaining granaries that allow caesar to take resources
    for (building *b = building_first_of_type(BUILDING_GRANARY); b && amount; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE) {
            if (building_storage_get_state(b, resource, 1) < BUILDING_STORAGE_STATE_GETTING) {
                amount -= building_granary_try_remove_resource(b, resource, amount);
            }
        }
    }
    // if that doesn't work, take it anyway
    for (building *b = building_first_of_type(BUILDING_GRANARY); b && amount; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE && building_storage_get_permission(BUILDING_STORAGE_PERMISSION_CAESAR, b)) {
            amount -= building_granary_try_remove_resource(b, resource, amount);
        }
    }
    return amount;
}

int building_granary_count_available_resource(building *b, int resource, int respect_maintaining)
{
    if (b->type != BUILDING_GRANARY || b->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    if (!respect_maintaining || (building_storage_get_state(b, resource, 1) < BUILDING_STORAGE_STATE_MAINTAINING)) {
        return building_granary_get_amount(b, resource);
    } else {
        return 0;
    }
}

int building_granaries_count_available_resource(int resource, int respect_maintaining, int caesars_request)
{
    int total = 0;

    for (building *b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {
        if (caesars_request && !building_storage_get_permission(BUILDING_STORAGE_PERMISSION_CAESAR, b)) {
            continue;
        }
        total += building_granary_count_available_resource(b, resource, respect_maintaining);
    }

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

int building_granaries_send_resources_to_rome(int resource, int amount)
{
    // first go for non-getting, non-maintaining granaries with caesar permission
    for (building *b = building_first_of_type(BUILDING_GRANARY); b && amount; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE) {
            if (building_storage_get_state(b, resource, 1) < BUILDING_STORAGE_STATE_GETTING &&
                building_storage_get_permission(BUILDING_STORAGE_PERMISSION_CAESAR, b)) {
                int taken_loads = building_granary_try_remove_resource(b, resource, amount);
                amount -= taken_loads;
                if (taken_loads) {
                    try_create_cart_to_rome(b, resource, taken_loads);
                }
            }
        }
    }
    if (amount <= 0) {
        return 0;
    }
    // if that doesn't work, take it anyway, but not from maintaining and no caesar permission granaries
    for (building *b = building_first_of_type(BUILDING_GRANARY); b && amount; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE) {
            if ((building_storage_get_state(b, resource, 1) < BUILDING_STORAGE_STATE_MAINTAINING) &&
                building_storage_get_permission(BUILDING_STORAGE_PERMISSION_CAESAR, b)) {
                int taken_loads = building_granary_try_remove_resource(b, resource, amount);
                amount -= taken_loads;
                if (taken_loads) {
                    try_create_cart_to_rome(b, resource, taken_loads);
                }
            }
        }
    }
    return amount;
}

int building_granary_maximum_receptible_amount(building *b, int resource)
{
    if (b->has_plague || building_storage_get_empty_all(b->id) ||
         b->state != BUILDING_STATE_IN_USE || b->resources[RESOURCE_NONE] <= 0) {
        return 0;
    }

    const building_storage_state state = building_storage_get_state(b, resource, 1);
    const building_storage *s = building_storage_get(b->storage_id);
    if (state == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return 0;
    }

    int max_accepted_amount = s->resource_state[resource].quantity; // max player-set limit
    int current_stored = b->resources[resource]; // already stored
    int remaining_for_resource = max_accepted_amount - current_stored;

    if (remaining_for_resource <= 0) {
        return 0;
    }

    int free_space_overall = b->resources[RESOURCE_NONE]; // general free slots
    int final_capacity = MIN(remaining_for_resource, free_space_overall);
    return final_capacity > 0 ? final_capacity : 0;
}


int building_granary_remove_for_getting_deliveryman(building *src, building *dst, int *resource)
{
    int max_amount = 0;
    int max_resource = 0;

    for (resource_type food = RESOURCE_MIN_FOOD; food < RESOURCE_MAX_FOOD; food++) {
        if (building_storage_get_state(dst, food, 1) == BUILDING_STORAGE_STATE_GETTING
            && granary_allows_getting(src, food)) {
            if (src->resources[food] > max_amount) {
                max_amount = src->resources[food];
                max_resource = food;
                continue; //only one resource per deliveryman, once found, exit loop
            }
        }
    }

    if (config_get(CONFIG_GP_CH_GRANARIES_GET_DOUBLE)) {
        if (max_amount > 16) {
            max_amount = 16;
        }
    } else {
        if (max_amount > 8) {
            max_amount = 8;
        }
    }
    if (max_amount > dst->resources[RESOURCE_NONE]) { // not necessary anymore since we check max receptible amount below
        max_amount = dst->resources[RESOURCE_NONE];
    }
    const int receptible_amount = building_granary_maximum_receptible_amount(dst, max_resource);
    if (max_amount > receptible_amount) {
        max_amount = receptible_amount;
    }
    building_granary_try_remove_resource(src, max_resource, max_amount);
    *resource = max_resource;
    return max_amount;
}

int building_granary_determine_worker_task(building *granary)
{
    int pct_workers = calc_percentage(granary->num_workers, model_get_building(granary->type)->laborers);
    if (pct_workers < 50) {
        return GRANARY_TASK_NONE;
    }
    const building_storage *s = building_storage_get(granary->storage_id);
    if (s->empty_all) {
        // bring food to another granary
        for (int i = RESOURCE_MIN_FOOD; i < RESOURCE_MAX_FOOD; i++) {
            if (granary->resources[i]) {
                return i;
            }
        }
        return GRANARY_TASK_NONE;
    }
    if (granary->resources[RESOURCE_NONE] <= 0) {
        return GRANARY_TASK_NONE; // granary full, nothing to get
    }

    for (resource_type food = RESOURCE_MIN_FOOD; food < RESOURCE_MAX_FOOD; food++) {
        if (!config_get(CONFIG_GP_CH_ENABLE_GETTING_WHILE_STOCKPILED) && city_resource_is_stockpiled(food)) {
            continue; // skip if stockpiled
        }
        if (building_storage_get_state(granary, food, 1) == BUILDING_STORAGE_STATE_GETTING &&
            non_getting_granaries.total_storage[food] >= ONE_CARTLOAD &&
            building_storage_get_amount(granary, food) < building_storage_get_storage_state_quantity(granary, food)) {
            return GRANARY_TASK_GETTING;
        }
    }
    return GRANARY_TASK_NONE;
}

void building_granaries_calculate_stocks(void)
{
    non_getting_granaries.num_items = 0;
    for (int i = 0; i < MAX_GRANARIES; i++) {
        non_getting_granaries.building_ids[i] = 0;
    }
    for (int i = 0; i < RESOURCE_MAX_FOOD; i++) {
        non_getting_granaries.total_storage[i] = 0;
    }

    for (building *b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE || !b->has_road_access ||
            b->distance_from_entry <= 0 || b->has_plague) {
            continue;
        }
        int total_non_getting = 0;

        for (resource_type food = RESOURCE_MIN_FOOD; food < RESOURCE_MAX_FOOD; food++) {
            if (granary_allows_getting(b, food)) { //if it allows getting, it means its not getting or maintaining
                total_non_getting += b->resources[food];
                non_getting_granaries.total_storage[food] += b->resources[food];
            }
        }
        if (total_non_getting > ONE_CARTLOAD) {
            non_getting_granaries.building_ids[non_getting_granaries.num_items] = b->id;
            if (non_getting_granaries.num_items < MAX_GRANARIES - 2) {
                non_getting_granaries.num_items++;
            }
        }
    }
}

int building_granary_accepts_storage(building *b, int resource, int *understaffed)
{
    if (b->state != BUILDING_STATE_IN_USE || b->type != BUILDING_GRANARY ||
        !b->has_road_access || b->distance_from_entry <= 0 || b->has_plague) {
        return 0;
    }
    int pct_workers = calc_percentage(b->num_workers, model_get_building(b->type)->laborers);
    if (pct_workers < 100) {
        if (understaffed) {
            *understaffed += 1;
        }
        return 0;
    }
    if (!building_granary_maximum_receptible_amount(b, resource)) {
        return 0;
    }

    if (config_get(CONFIG_GP_CH_DELIVER_ONLY_TO_ACCEPTING_GRANARIES)) {
        if (building_storage_get_state(b, resource, 1) == BUILDING_STORAGE_STATE_GETTING) {
            return 0;
        }
    }
    return b->resources[RESOURCE_NONE] >= ONE_CARTLOAD;
}

building *building_granary_get_granary_needing_food(building *source, int resource, int getting)
{

    int max_distance = config_get(CONFIG_GP_CH_FARMS_DELIVER_CLOSE) ? 64 : INFINITE;
    building *b;
    for (b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {
        if (b->road_network_id != source->road_network_id || !building_granary_accepts_storage(b, resource, 0)) {
            continue;
        }
        if ((building_storage_get_state(b, resource, 1) == BUILDING_STORAGE_STATE_GETTING) && getting ||
            (building_storage_get_state(b, resource, 1) != BUILDING_STORAGE_STATE_GETTING && !getting)) {
            //'not accepting' is already filtered out by building_granary_accepts_storage
            //if getting is 1, then we are looking only for granaries that are getting the resource, otherwise only accepting
            int dist = map_grid_chess_distance(source->grid_offset, b->grid_offset);
            if (dist < max_distance) {
                return b; // found a granary with right storage state and within distance
            }

        }
    }
    return b; // null
}

int building_granary_for_storing(int x, int y, int resource, int road_network_id,
    int force_on_stockpile, int *understaffed, map_point *dst)
{
    if (scenario_property_rome_supplies_wheat()) {
        return 0;
    }
    if (!resource_is_food(resource)) {
        return 0;
    }
    if (city_resource_is_stockpiled(resource) && !force_on_stockpile) {
        return 0;
    }
    int min_dist = INFINITE;
    int min_building_id = 0;
    for (building *b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {
        if (b->road_network_id != road_network_id ||
            !building_granary_accepts_storage(b, resource, understaffed)) {
            continue;
        }
        // there is room
        int dist = calc_maximum_distance(b->x + 1, b->y + 1, x, y);
        if (dist < min_dist) {
            min_dist = dist;
            min_building_id = b->id;
        }
    }
    // deliver to center of granary
    building *min = building_get(min_building_id);
    map_point_store_result(min->x + 1, min->y + 1, dst);
    return min_building_id;
}

int building_getting_granary_for_storing(int x, int y, int resource, int road_network_id, map_point *dst)
{
    if (scenario_property_rome_supplies_wheat()) {
        return 0;
    }
    if (!resource_is_food(resource)) {
        return 0;
    }

    if (city_resource_is_stockpiled(resource)) {
        return 0;
    }
    int min_dist = INFINITE;
    int min_building_id = 0;
    for (building *b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE || b->has_plague) {
            continue;
        }
        if (!b->has_road_access || b->distance_from_entry <= 0 || b->road_network_id != road_network_id) {
            continue;
        }
        int pct_workers = calc_percentage(b->num_workers, model_get_building(b->type)->laborers);
        if (pct_workers < 100) {
            continue;
        }
        const building_storage *s = building_storage_get(b->storage_id);
        if (!building_granary_maximum_receptible_amount(b, resource) || s->empty_all) {
            continue;
        } else {
            // there is room
            int dist = calc_maximum_distance(b->x + 1, b->y + 1, x, y);
            if (dist < min_dist) {
                min_dist = dist;
                min_building_id = b->id;
            }
        }
    }
    building *min = building_get(min_building_id);
    map_point_store_result(min->x + 1, min->y + 1, dst);
    return min_building_id;
}

int building_granary_amount_can_get_from(building *destination, building *origin, int resource)
{
    int amount_gettable = 0;
    if (!resource_is_food(resource)) {
        return 0; // only food resources can be gotten from granaries
    }
    if (building_storage_get_state(origin, resource, 1) == BUILDING_STORAGE_STATE_GETTING && //if origin is getting
            granary_allows_getting(destination, resource)) { // and destination allows getting
        amount_gettable = destination->resources[resource];
    }
    return amount_gettable;
}

int building_granary_for_getting(building *src, map_point *dst, int min_amount)
{
    const building_storage *s_src = building_storage_get(src->storage_id);
    if (s_src->empty_all) {
        return 0;
    }
    if (scenario_property_rome_supplies_wheat()) {
        return 0;
    }
    int food_to_get = RESOURCE_NONE;
    for (resource_type food = RESOURCE_MIN_FOOD; food < RESOURCE_MAX_FOOD; food++) {
        if (building_storage_get_state(src, food, 1) == BUILDING_STORAGE_STATE_GETTING) {
            food_to_get = food;
            break; // only one resource per deliveryman, once found, exit loop
        }
    }
    if (!food_to_get) {
        return 0;
    }
    int min_dist = INFINITE;
    int min_building_id = 0;
    for (int i = 0; i < non_getting_granaries.num_items; i++) {
        building *b = building_get(non_getting_granaries.building_ids[i]);
        if (!config_get(CONFIG_GP_CH_GETTING_GRANARIES_GO_OFFROAD)) {
            if (b->road_network_id != src->road_network_id) {
                continue;
            }
        }
        if (b->id == src->id) {
            continue; // don't get from the same granary
        }
        if (building_granary_amount_can_get_from(b, src, food_to_get) >= min_amount) {
            int dist = calc_maximum_distance(b->x + 1, b->y + 1, src->x + 1, src->y + 1);
            if (dist < min_dist) {
                min_dist = dist;
                min_building_id = b->id;
            }
        }
    }
    building *min = building_get(min_building_id);
    map_point_store_result(min->x + 1, min->y + 1, dst);
    return min_building_id;
}

void building_granary_bless(void)
{
    int min_stored = INFINITE;
    building *min_building = 0;
    for (building *b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE || b->has_plague) {
            continue;
        }
        int total_stored = 0;
        for (int r = RESOURCE_MIN_FOOD; r < RESOURCE_MAX_FOOD; r++) {
            total_stored += building_granary_get_amount(b, r);
        }
        if (total_stored < min_stored) {
            min_stored = total_stored;
            min_building = b;
        }
    }
    if (min_building) {
        city_resource_determine_available(1);

        const resource_list *list = city_resource_get_available_foods();

        for (unsigned int i = 0; i < list->size; i++) {
            for (int n = 0; n < 6; n++) {
                building_granary_try_add_resource(min_building, list->items[i], 1, 0, 1);
            }
        }
    }
}

void building_granary_warehouse_curse(int big)
{
    int max_stored = 0;
    building *max_building = 0;
    for (building *b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE || b->has_plague) {
            continue;
        }
        int total_stored = 0;
        for (int r = RESOURCE_MIN_FOOD; r < RESOURCE_MAX_FOOD; r++) {
            total_stored += building_granary_get_amount(b, r);
        }
        if (total_stored > max_stored) {
            max_stored = total_stored;
            max_building = b;
        }
    }
    for (building *b = building_first_of_type(BUILDING_WAREHOUSE); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE || b->has_plague) {
            continue;
        }
        int total_stored = 0;
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            total_stored += building_warehouse_get_amount(b, r);
        }
        if (total_stored > max_stored) {
            max_stored = total_stored;
            max_building = b;
        }
    }
    if (!max_building) {
        return;
    }
    if (big) {
        city_message_disable_sound_for_next_message();
        city_message_post(0, MESSAGE_FIRE, max_building->type, max_building->grid_offset);
        building_destroy_by_fire(max_building);
        sound_effect_play(SOUND_EFFECT_EXPLOSION);
        map_routing_update_land();
    } else {
        if (max_building->type == BUILDING_WAREHOUSE) {
            building_warehouse_remove_resource_curse(max_building, CURSE_LOADS);
        } else if (max_building->type == BUILDING_GRANARY) {
            int amount = CURSE_LOADS;
            for (resource_type food = RESOURCE_MIN_FOOD; food < RESOURCE_MAX_FOOD; food++) {
                amount -= building_granary_try_remove_resource(max_building, food, amount);
            }
        }
    }
}

void building_granary_update_built_granaries_capacity(void)
{
    for (building *b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {
        int total_units = 0;
        for (int resource = RESOURCE_MIN_FOOD; resource < RESOURCE_MAX_FOOD; resource++) {
            total_units += b->resources[resource];
        }
        total_units += b->resources[RESOURCE_NONE];

        if (total_units < BUILDING_STORAGE_QUANTITY_MAX) {
            b->resources[RESOURCE_NONE] += BUILDING_STORAGE_QUANTITY_MAX - total_units;
        }
        // for now, we don't handle the case where we decrease granary capacity
    }
}
