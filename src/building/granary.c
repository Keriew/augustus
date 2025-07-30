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
#include "map/road_access.h"
#include "map/routing_terrain.h"
#include "scenario/property.h"
#include "sound/effect.h"

#define MAX_GRANARIES 100
#define CURSE_LOADS BUILDING_STORAGE_QUANTITY_MAX / 2
#define INFINITE 10000
#define STORAGE_ADDED_PER_CARTLOAD 1 //used to be 100; 
//RESOURCE_ONE_LOAD in resource.h still points to 100, since it's used in distribution

static struct {
    int building_ids[MAX_GRANARIES];
    int num_items;
    int total_storage[RESOURCE_MAX_FOOD];
} non_getting_granaries;

static int get_amount(building *granary, int resource)
{
    if (!resource_is_food(resource)) {
        return 0;
    }
    if (granary->type != BUILDING_GRANARY) {
        return 0;
    }
    return granary->resources[resource];
}

static int granary_is_accepting(int resource, building *b)
{
    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];
    int amount = get_amount(b, resource);

    if (b->has_plague) {
        return 0;
    }
    if (entry->state != BUILDING_STORAGE_STATE_ACCEPTING) {
        return 0;
    }
    if (amount < entry->quantity) {
        return 1;
    }

    return 0;
}

//currently the only non-static granary_is, due to dependency with warehouse
int building_granary_is_getting(int resource, building *b)
{
    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];
    int amount = get_amount(b, resource);

    if (b->has_plague) {
        return 0;
    }
    if (entry->state != BUILDING_STORAGE_STATE_GETTING) {
        return 0;
    }
    if (amount < entry->quantity) {
        return 1;
    }

    return 0;
}

static int building_granary_is_maintaining(int resource, building *b)
{
    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];

    if (entry->state == BUILDING_STORAGE_STATE_MAINTAINING) {
        return 1;
    }
    return 0;
}

static int granary_allows_getting(int resource, building *b)
{
    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];

    if (b->has_plague || (entry->state == BUILDING_STORAGE_STATE_GETTING ||
        entry->state == BUILDING_STORAGE_STATE_MAINTAINING)) {
        return 0;
        //if the building has plague or gets or maintains resource - it doesnt allow getting
    }

    return 1;
}

int building_granary_is_not_accepting(int resource, building *b)
{
    if (building_granary_maximum_receptible_amount(resource, b) <= 0) {
        return 1;
    } else {
        return 0;
    }
}

int building_granary_is_full(building *b)
{
    return b->resources[RESOURCE_NONE] <= 0;
}

int building_granary_resource_amount(int resource, building *b)
{
    return b->resources[resource];
}

int building_granary_add_import(building *granary, int resource, int land_trader)
{
    if (!building_granary_add_resource(granary, resource, 0)) {
        return 0;
    }
    int price = trade_price_buy(resource, land_trader);
    city_finance_process_import(price);
    return 1;
}

int building_granary_remove_export(building *granary, int resource, int land_trader)
{
    if (building_granary_remove_resource(granary, resource, STORAGE_ADDED_PER_CARTLOAD) == STORAGE_ADDED_PER_CARTLOAD) {
        return 0;
    }
    int price = trade_price_sell(resource, land_trader);
    city_finance_process_export(price);
    return 1;
}

int building_granary_add_resource(building *granary, int resource, int is_produced)
{
    if (granary->id <= 0) {
        return 1;
    }
    if (!resource_is_food(resource)) {
        return 0;
    }
    if (granary->type != BUILDING_GRANARY) {
        return 0;
    }
    if (building_granary_is_full(granary)) {
        return 0; // no space
    }
    if (building_granary_is_not_accepting(resource, granary)) {
        return 0;
    }
    if (is_produced) {
        city_resource_add_produced_to_granary(STORAGE_ADDED_PER_CARTLOAD);
    }
    city_resource_add_to_granary(resource, STORAGE_ADDED_PER_CARTLOAD);
    if (granary->resources[RESOURCE_NONE] <= STORAGE_ADDED_PER_CARTLOAD) {
        granary->resources[resource] += granary->resources[RESOURCE_NONE];
        granary->resources[RESOURCE_NONE] = 0;
    } else {
        granary->resources[resource] += STORAGE_ADDED_PER_CARTLOAD;
        granary->resources[RESOURCE_NONE] -= STORAGE_ADDED_PER_CARTLOAD;
    }
    return 1;
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
        int space_available = b->resources[RESOURCE_NONE];
        if (respect_settings) {
            int allowed_space = building_granary_maximum_receptible_amount(resource, b);
            if (allowed_space < space_available) {
                space_available = allowed_space;
            }
        }
        if (space_available > amount) {
            space_available = amount;
        }
        if (space_available <= 0) {
            continue;
        }
        amount -= space_available;
        b->resources[resource] += space_available;
        b->resources[RESOURCE_NONE] -= space_available;
        city_resource_add_to_granary(resource, space_available);
        if (amount <= 0) {
            break;
        }
    }

    return amount;
}

int building_granary_remove_resource(building *granary, int resource, int amount)
{
    return amount - building_granary_try_remove_resource(granary, resource, amount);
}

int building_granary_try_remove_resource(building *granary, int resource, int desired_amount)
{
    if (desired_amount <= 0) {
        return 0;
    }
    if (granary->has_plague) {
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

int building_granary_try_fullload_remove_resource(building *granary, int resource, int desired_loads)
{
    if (desired_loads <= 0) {
        return 0;
    }
    if (granary->has_plague) {
        return 0;
    }
    int desired_amount = desired_loads;
    if (granary->resources[resource] < desired_amount) {
        desired_amount = granary->resources[resource];
    }
    return building_granary_try_remove_resource(granary, resource, desired_amount);
}

int building_granaries_remove_resource(int resource, int amount)
{
    // first go for non-getting, non-maintaining granaries
    for (building *b = building_first_of_type(BUILDING_GRANARY); b && amount; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE) {
            if (!(building_granary_is_getting(resource, b) && building_granary_is_maintaining(resource, b))) {
                amount = building_granary_remove_resource(b, resource, amount);
            }
        }
    }
    // if that doesn't work, take it anyway
    for (building *b = building_first_of_type(BUILDING_GRANARY); b && amount; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE) {
            amount = building_granary_remove_resource(b, resource, amount);
        }
    }
    return amount;
}

int building_granary_count_available_resource(building *b, int resource, int respect_maintaining)
{
    if (b->type != BUILDING_GRANARY || b->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    if (!respect_maintaining || !building_granary_is_maintaining(resource, b)) {
        return building_granary_resource_amount(resource, b);
    } else {
        return 0;
    }
}

int building_granaries_count_available_resource(int resource, int respect_maintaining)
{
    int total = 0;

    for (building *b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {

        total += building_granary_count_available_resource(b, resource, respect_maintaining);
    }

    return total;
}

int building_granaries_send_resources_to_rome(int resource, int amount)
{
    // first go for non-getting, non-maintaining granaries
    for (building *b = building_first_of_type(BUILDING_GRANARY); b && amount; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE) {
            if (!(building_granary_is_getting(resource, b) && building_granary_is_maintaining(resource, b))) {
                int remaining = building_granary_remove_resource(b, resource, amount);
                if (remaining < amount) {
                    int loads = amount - remaining;
                    amount = remaining;
                    map_point road;
                    if (map_has_road_access(b->x, b->y, b->size, &road)) {
                        figure *f = figure_create(FIGURE_CART_PUSHER, road.x, road.y, DIR_4_BOTTOM);
                        f->action_state = FIGURE_ACTION_234_CARTPUSHER_GOING_TO_ROME_CREATED;
                        f->resource_id = resource;
                        f->loads_sold_or_carrying = loads;
                        f->building_id = b->id;
                    }
                }
            }
        }
    }
    // if that doesn't work, take it anyway, but not from maintaining granaries
    for (building *b = building_first_of_type(BUILDING_GRANARY); b && amount; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE && !building_granary_is_maintaining(resource, b)) {
            int remaining = building_granary_remove_resource(b, resource, amount);
            if (remaining < amount) {
                int loads = amount - remaining;
                amount = remaining;
                map_point road;
                if (map_has_road_access(b->x, b->y, b->size, &road)) {
                    figure *f = figure_create(FIGURE_CART_PUSHER, road.x, road.y, DIR_4_BOTTOM);
                    f->action_state = FIGURE_ACTION_234_CARTPUSHER_GOING_TO_ROME_CREATED;
                    f->resource_id = resource;
                    f->loads_sold_or_carrying = loads;
                    f->building_id = b->id;
                }
            }
        }
    }
    return amount;
}

int building_granary_maximum_receptible_amount(int resource, building *b)
{
    if (b->has_plague) {
        return 0;
    }

    const building_storage *s = building_storage_get(b->storage_id);
    building_storage_state state = s->resource_state[resource].state;
    building_storage_quantity quantity = s->resource_state[resource].quantity;

    if (state == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return 0;
    }

    int stored_amount = b->resources[resource];
    int max_amount = quantity;  // Since quantity is a direct value like 32, 28, etc.

    return (max_amount > stored_amount) ? (max_amount - stored_amount) : 0;
}



int building_granary_remove_for_getting_deliveryman(building *src, building *dst, int *resource)
{
    int max_amount = 0;
    int max_resource = 0;

    for (resource_type food = RESOURCE_MIN_FOOD; food < RESOURCE_MAX_FOOD; food++) {
        if (building_granary_is_getting(food, dst) && granary_allows_getting(food, src)) {
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
    if (max_amount > dst->resources[RESOURCE_NONE]) {
        max_amount = dst->resources[RESOURCE_NONE];
    }
    const int receptible_amount = building_granary_maximum_receptible_amount(max_resource, dst);
    if (max_amount > receptible_amount) {
        max_amount = receptible_amount;
    }
    building_granary_remove_resource(src, max_resource, max_amount);
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
        if (building_granary_is_getting(food, granary) &&
            non_getting_granaries.total_storage[food] > STORAGE_ADDED_PER_CARTLOAD) {
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
            if (granary_allows_getting(food, b)) { //if it allows getting, it means its not getting or maintaining
                total_non_getting += b->resources[food];
                non_getting_granaries.total_storage[food] += b->resources[food];
            }
        }
        if (total_non_getting > STORAGE_ADDED_PER_CARTLOAD) {
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
    const building_storage *s = building_storage_get(b->storage_id);
    if (building_granary_is_not_accepting(resource, b) || s->empty_all) {
        return 0;
    }

    if (config_get(CONFIG_GP_CH_DELIVER_ONLY_TO_ACCEPTING_GRANARIES)) {
        if (building_granary_is_getting(resource, b)) {
            return 0;
        }
    }
    return b->resources[RESOURCE_NONE] >= STORAGE_ADDED_PER_CARTLOAD;
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
        if (!building_granary_is_getting(resource, b) || s->empty_all) {
            continue;
        }
        if (b->resources[RESOURCE_NONE] > STORAGE_ADDED_PER_CARTLOAD) {
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

int building_granary_amount_can_get_from(building *destination, building *origin)
{
    int amount_gettable = 0;
    for (resource_type food = RESOURCE_MIN_FOOD; food < RESOURCE_MAX_FOOD; food++) {
        if (building_granary_is_getting(food, origin) && //if origin is getting
            granary_allows_getting(food, destination)) { // and destination allows getting
            amount_gettable += destination->resources[food];
        }
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
    int getting_something = 0;
    for (resource_type food = RESOURCE_MIN_FOOD; food < RESOURCE_MAX_FOOD && !getting_something; food++) {
        getting_something |= building_granary_is_getting(food, src);
    }
    if (!getting_something) {
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
        if (building_granary_amount_can_get_from(b, src) >= min_amount) {
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
            total_stored += get_amount(b, r);
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
                building_granary_add_resource(min_building, list->items[i], 0);
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
            total_stored += get_amount(b, r);
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
                amount = building_granary_remove_resource(max_building, food, amount);
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
