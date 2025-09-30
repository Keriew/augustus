#include "trader.h"

#include "building/building.h"
#include "building/caravanserai.h"
#include "building/dock.h"
#include "building/granary.h"
#include "building/lighthouse.h"
#include "building/model.h"
#include "building/monument.h"
#include "building/warehouse.h"
#include "building/storage.h"
#include "city/buildings.h"
#include "city/finance.h"
#include "city/health.h"
#include "city/map.h"
#include "city/message.h"
#include "city/resource.h"
#include "city/trade.h"
#include "city/trade_policy.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "core/log.h"
#include "core/random.h"
#include "empire/city.h"
#include "empire/empire.h"
#include "empire/object.h"
#include "empire/trade_prices.h"
#include "empire/trade_route.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "figure/trader.h"
#include "figure/visited_buildings.h"
#include "game/time.h"
#include "map/routing.h"
#include "map/routing_path.h"
#include "scenario/map.h"
#include "scenario/property.h"

#include <math.h> 
#include <stdio.h>

#define INFINITE 10000
#define TRADER_INITIAL_WAIT GAME_TIME_TICKS_PER_DAY
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define SCORE_BASE 100 // base for scoring system, resource multipliers are in relation to this value
#define DISTANCE_BASELINE 80 // baseline for chess distance - 2* (2/3 of small province, 1/4 of enormous province)
#define PRICE_BASELINE 80
#define MULTIPLIER_PRICE_MIN 50 // minimum multiplier for resource basing on price
#define MULTIPLIER_PRICE_MAX 300 // maximum multiplier for resource basing on price
#define MULTIPLIER_DISTANCE_MIN 50
#define MULTIPLIER_DISTANCE_MAX 300

#define LOGARITHIMIC_SCALER_DISTANCE 200
#define LOGARITHMIC_SCALER_SELL 0  // from perspectvie of the trader - trader sells, player buys
#define LOGARITHMIC_SCALER_BUY 80
// adjustable scaling factors. Higher value = more influence.
// 0 =  no influence. Generally, values between 20-100 are most useful.

/* e.g. with buy scaler at 50 and sell scaler at 0, traders will not consider the price when selling goods to the player,
but will consider price a factor when buying goods from the player -> the more expensive the resource, more likely
the trader will go to the location to buy it. */

typedef struct {
    int value_multiplier[RESOURCE_MAX];
} sell_multipliers;

typedef struct {
    int value_multiplier[RESOURCE_MAX];
} buy_multipliers;

static struct {
    sell_multipliers sell_multiplier;
    buy_multipliers buy_multiplier;
} data;

static int get_least_filled_quota_resource(building *b, int city_id, signed char trader_buying);

static int calculate_log_score(int baseline, int multiplier_min, int multiplier_max,
     int logarithmic_scaler, int input_value)
{
    if (input_value <= 0) input_value = 1; // Avoid log(0) errors
    double ratio = (double) input_value / baseline;
    int score = (int) (SCORE_BASE + logarithmic_scaler * log10(ratio));
    score = MIN(MAX(score, multiplier_min), multiplier_max);
    return score;
}

static void resource_multiplier_init(void)
{
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        // player buys, traders sell
        int price_sell_multiplier = calculate_log_score(PRICE_BASELINE, MULTIPLIER_PRICE_MIN, MULTIPLIER_PRICE_MAX,
        LOGARITHMIC_SCALER_SELL, trade_price_buy(r, 1)); //trader sells, player buys 
        data.sell_multiplier.value_multiplier[r] = price_sell_multiplier;
        int price_buy_multiplier = calculate_log_score(PRICE_BASELINE, MULTIPLIER_PRICE_MIN, MULTIPLIER_PRICE_MAX,
        LOGARITHMIC_SCALER_BUY, trade_price_sell(r, 1)); //trader buys, player sells 
        data.buy_multiplier.value_multiplier[r] = price_buy_multiplier;
        // add any other rules that increase priority of a resource here, e.g.: resource_is_food(r) ? 150 : 100;
    }
}

static void resource_multiplier_reset(void)
{
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        data.sell_multiplier.value_multiplier[r] = 0;
        data.buy_multiplier.value_multiplier[r] = 0;
    }
}

// Mercury Grand Temple base bonus to trader speed
static int trader_bonus_speed(void)
{
    if (building_monument_working(BUILDING_GRAND_TEMPLE_MERCURY)) {
        return 25;
    } else {
        return 0;
    }
}

// Neptune Grand Temple base bonus to trader speed
static int sea_trader_bonus_speed(void)
{
    if (building_monument_working(BUILDING_GRAND_TEMPLE_NEPTUNE)) {
        return 25;
    } else {
        return 0;
    }
}

int figure_create_trade_caravan(int x, int y, int city_id)
{
    figure *caravan = figure_create(FIGURE_TRADE_CARAVAN, x, y, DIR_0_TOP);
    caravan->empire_city_id = city_id;
    caravan->action_state = FIGURE_ACTION_100_TRADE_CARAVAN_CREATED;
    random_generate_next();
    caravan->wait_ticks = random_byte() & TRADER_INITIAL_WAIT;
    // donkey 1
    figure *donkey1 = figure_create(FIGURE_TRADE_CARAVAN_DONKEY, x, y, DIR_0_TOP);
    donkey1->action_state = FIGURE_ACTION_100_TRADE_CARAVAN_CREATED;
    donkey1->leading_figure_id = caravan->id;
    // donkey 2
    figure *donkey2 = figure_create(FIGURE_TRADE_CARAVAN_DONKEY, x, y, DIR_0_TOP);
    donkey2->action_state = FIGURE_ACTION_100_TRADE_CARAVAN_CREATED;
    donkey2->leading_figure_id = donkey1->id;
    return caravan->id;
}

int figure_create_trade_ship(int x, int y, int city_id)
{
    figure *ship = figure_create(FIGURE_TRADE_SHIP, x, y, DIR_0_TOP);
    ship->empire_city_id = city_id;
    ship->action_state = FIGURE_ACTION_110_TRADE_SHIP_CREATED;
    random_generate_next();
    ship->wait_ticks = random_byte() & TRADER_INITIAL_WAIT;
    return ship->id;
}

int figure_trade_caravan_can_buy(figure *trader, int building_id, int city_id)
{
    building *b = building_get(building_id);
    if (b->type != BUILDING_WAREHOUSE &&
        !(config_get(CONFIG_GP_CH_ALLOW_EXPORTING_FROM_GRANARIES) && b->type == BUILDING_GRANARY)) {
        return 0;
    }
    if (b->has_plague) {
        return 0;
    }
    if (trader->trader_amount_bought >= figure_trade_land_trade_units()) {
        return 0;
    }
    if (!building_storage_get_permission(BUILDING_STORAGE_PERMISSION_TRADERS, b)) {
        return 0;
    }
    return 1;
}

int figure_trade_caravan_can_sell(figure *trader, int building_id, int city_id)
{
    building *b = building_get(building_id);
    if (b->type != BUILDING_WAREHOUSE && b->type != BUILDING_GRANARY) {
        return 0;
    }
    if (b->has_plague) {
        return 0;
    }
    if (trader->loads_sold_or_carrying >= figure_trade_land_trade_units()) {
        return 0;
    }
    if (!building_storage_get_permission(BUILDING_STORAGE_PERMISSION_TRADERS, b)) {
        return 0;
    }
    if (building_storage_get(b->storage_id)->empty_all) {
        return 0;
    }
    return 1;
}

resource_type get_native_trader_buy_resource(building *b)
{
    resource_type highest_resource = RESOURCE_NONE;
    if (b->type == BUILDING_WAREHOUSE) {
        building_warehouse_recount_resources(b);
    }
    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (b->resources[r] > highest_resource &&
            (city_resource_trade_status(r) & TRADE_STATUS_EXPORT)) {
            highest_resource = r;
        }
    }
    return highest_resource;
}

static int trader_get_buy_resource(int building_id, int city_id)
{
    //TODO: get all the logic of trade happening into this function, rather than decision making in the action function
    unsigned char land_trader = 1; // 1 = land trader, 0 = sea trader
    building *b = building_get(building_id);
    if (!b || (b->type != BUILDING_WAREHOUSE && b->type != BUILDING_GRANARY)) {
        return RESOURCE_NONE;
    }

    if (b->type == BUILDING_GRANARY && !config_get(CONFIG_GP_CH_ALLOW_EXPORTING_FROM_GRANARIES)) {
        return RESOURCE_NONE;
    }
    int resource = get_least_filled_quota_resource(b, city_id, 1); // 1 = trader buying
    if (resource == RESOURCE_NONE && city_id == 0) { //native trader
        resource = building_storage_get_highest_quantity_resource(b);
    }
    if (resource == RESOURCE_NONE) {
        return RESOURCE_NONE;
    }
    unsigned char success = 0;
    if (b->type == BUILDING_GRANARY) {
        success = building_granary_remove_export(b, resource, 1, land_trader);
    } else {
        success = building_warehouse_remove_export(b, resource, 1, land_trader);
    }

    return success ? resource : RESOURCE_NONE;
}

static int trader_get_sell_resource(int building_id, int city_id)
{
    unsigned char land_trader = 1; // 1 = land trader, 0 = sea trader
    building *b = building_get(building_id);
    if (!b || (b->type != BUILDING_WAREHOUSE && b->type != BUILDING_GRANARY)) {
        return RESOURCE_NONE;
    }

    int resource = get_least_filled_quota_resource(b, city_id, 0); // 0 = trader selling
    if (resource == RESOURCE_NONE) {
        return RESOURCE_NONE;
    }

    unsigned char success = 0;
    if (b->type == BUILDING_GRANARY) {
        success = building_granary_add_import(b, resource, 1, land_trader);
    } else {
        success = building_warehouse_add_import(b, resource, 1, land_trader);
    }

    return success ? resource : RESOURCE_NONE;
}

static int get_least_filled_quota_resource(building *b, int city_id, signed char trader_buying)
{
    int r_start = (b->type == BUILDING_GRANARY) ? RESOURCE_MIN_FOOD : RESOURCE_MIN;
    int r_end = (b->type == BUILDING_GRANARY) ? RESOURCE_MAX_FOOD : RESOURCE_MAX;

    int best_resource = RESOURCE_NONE;
    int lowest_percent = 101; // Higher than max possible fill (100%)

    int route_id = empire_city_get_route_id(city_id);
    int available = 0;
    for (int r = r_start; r < r_end; r++) {
        // Check if resource is available
        if (trader_buying) {
            if (b->type == BUILDING_GRANARY) {
                available = building_granary_count_available_resource(b, r, 1);
            } else {
                available = building_warehouse_get_available_amount(b, r);
            }

        } else {
            if (b->type == BUILDING_GRANARY) {
                available = building_granary_maximum_receptible_amount(b, r);
            } else {
                available = building_warehouse_maximum_receptible_amount(b, r);
            }
        }
        if (available <= 0) {
            continue;
        }
        if (trader_buying) {
            if (!empire_can_export_resource_to_city(city_id, r)) {
                continue;
            }
        } else {
            if (!empire_can_import_resource_from_city(city_id, r)) {
                continue;
            }
        }

        int limit = trade_route_limit(route_id, r);
        if (limit <= 0) continue;

        int traded = trade_route_traded(route_id, r);
        int fill_percent = (traded * 100) / limit;

        if (fill_percent < lowest_percent) {
            lowest_percent = fill_percent;
            best_resource = r;
        }
    }

    return best_resource;
}

static int get_closest_storage(const figure *f, int x, int y, int city_id, map_point *dst)
{
    const int max_trade_units = (f->type != FIGURE_NATIVE_TRADER) ?
        figure_trade_land_trade_units() : figure_trade_land_trade_units() / 3 + 1;

    resource_multiplier_init();
    int sellable[RESOURCE_MAX] = { 0 };
    int buyable[RESOURCE_MAX] = { 0 };
    int route_id = empire_city_get_route_id(f->empire_city_id);
    // 1. Determine what resources and how many can this caravan sell and buy
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        signed char resource_sell = empire_can_import_resource_from_city(city_id, r) ? 1 : 0;
        signed char resource_buy = empire_can_export_resource_to_city(city_id, r) ? 1 : 0;
        if (resource_sell || resource_buy) {
            int remaining = trade_route_limit(route_id, r) - trade_route_traded(route_id, r);
            if (route_id == 0 && f->type == FIGURE_NATIVE_TRADER) { // no limits for native traders
                remaining = figure_trade_land_trade_units();
            }
            if (remaining > 0) {
                if (resource_sell) {
                    sellable[r] = remaining;
                }
                if (resource_buy) {
                    buyable[r] = remaining;
                }
            }
        }
    }
    int permissions = f->type == FIGURE_NATIVE_TRADER ? BUILDING_STORAGE_PERMISSION_NATIVES : BUILDING_STORAGE_PERMISSION_TRADERS;
    int sell_capacity = max_trade_units - f->loads_sold_or_carrying;
    int buy_capacity = max_trade_units - f->trader_amount_bought;
    int best_score = -1;
    int building_types[] = { BUILDING_GRANARY, BUILDING_WAREHOUSE };
    int best_building_id = 0;
    for (int t = 0; t < 2; t++) {
        // Loop over all buildings of the current type (granary or warehouse)
        for (building *b = building_first_of_type(building_types[t]); b; b = b->next_of_type) {
            // Skip buildings
            if (b->state != BUILDING_STATE_IN_USE || b->has_plague || !b->has_road_access
            || (figure_visited_building_in_list(f->last_visited_index, b->id)) || b->id == f->destination_building_id ||
            !building_storage_get_permission(permissions, b)) {
                continue; // Not active, infected, unreachable by road, recently visited, currenty at, not accepted
            }
            int sell_score = 0; // Score for how many units the trader can sell to this building
            int buy_score = 0;  // Score for how many units the trader can buy from this building
            // Loop through all resource types
            for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                if (building_types[t] == BUILDING_GRANARY && !resource_is_food(r)) {
                    continue;
                }
                // === SELL SCORING: Trader -> Building ===
                if (sellable[r] > 0 && sell_capacity > 0) {
                    // Get how much of this resource the building can accept
                    int receptable = (building_types[t] == BUILDING_GRANARY)
                        ? building_granary_maximum_receptible_amount(b, r) :
                        building_warehouse_maximum_receptible_amount(b, r);
                    // Limit to how much can actually be sold here
                    int can_add = MIN(MIN(sellable[r], receptable), sell_capacity);
                    can_add = can_add * data.sell_multiplier.value_multiplier[r]; // Apply sell multiplier
                    sell_score += can_add; // Add this to the total sell score
                }

                // === BUY SCORING: Building -> Trader ===
                if (buyable[r] > 0 && buy_capacity > 0) {
                    // Get how much of this resource the building currently holds
                    int available = (building_types[t] == BUILDING_GRANARY)
                        ? building_granary_get_amount(b, r) : building_warehouse_get_available_amount(b, r);
                    // Limit to how much the trader can actually buy from here
                    int can_take = MIN(MIN(buyable[r], available), buy_capacity);
                    can_take = can_take * data.buy_multiplier.value_multiplier[r]; // Apply buy multiplier
                    buy_score += can_take; // Add this to the total buy score
                }
            }
            const map_tile *exit = city_map_exit_point();
            int raw_distance = map_grid_chess_distance(f->grid_offset, b->grid_offset);
            if (route_id == 0 && f->type == FIGURE_NATIVE_TRADER) {
                raw_distance += raw_distance; //native traders always return home after 1 trade, so double the distance
            } else {
                raw_distance += map_grid_chess_distance(b->grid_offset, exit->grid_offset);
            }
            int distance_score = calculate_log_score(raw_distance, MULTIPLIER_DISTANCE_MIN, MULTIPLIER_DISTANCE_MAX,
                LOGARITHIMIC_SCALER_DISTANCE, DISTANCE_BASELINE);
            //swapping the input and baseline gives inverted score: higher score for shorter distances
            int total_score = (sell_score + buy_score) * distance_score / 100; // Normalize by 100 
            // If this building is the best candidate so far, store it
            if (total_score > best_score && total_score > 0) {
                best_score = total_score;
                best_building_id = b->id;
            }
        }
    }
    // 5. Return result 
    if (best_building_id) {
        const building *best_building = building_get(best_building_id);
        if (best_building->type == BUILDING_GRANARY && best_building->has_road_access >= 1) {
            // go to center of granary
            map_point_store_result(best_building->x + 1, best_building->y + 1, dst);
        } else if (best_building->type == BUILDING_WAREHOUSE && best_building->has_road_access >= 1) {
            map_point_store_result(best_building->x, best_building->y, dst);
        } else if (!map_has_road_access_warehouse(best_building->x, best_building->y, dst) &&
             !map_has_road_access_granary(best_building->x, best_building->y, dst)) {
            resource_multiplier_reset();
            return 0; // No road access found
        } else {
            map_point_store_result(best_building->x, best_building->y, dst); //fallback
        }
        resource_multiplier_reset();
        return best_building->id;
    }
    resource_multiplier_reset();
    return 0;
}

static void go_to_next_storage(figure *f)
{
    map_point dst;
    int destination_id = get_closest_storage(f, f->x, f->y, f->empire_city_id, &dst);
    if (destination_id) {
        f->destination_building_id = destination_id;
        f->action_state = FIGURE_ACTION_101_TRADE_CARAVAN_ARRIVING;
        f->destination_x = dst.x;
        f->destination_y = dst.y;
    } else {
        const map_tile *exit = city_map_exit_point();
        f->action_state = FIGURE_ACTION_103_TRADE_CARAVAN_LEAVING;
        f->destination_x = exit->x;
        f->destination_y = exit->y;
    }
}

static int trader_image_id(void)
{
    if (scenario_property_climate() == CLIMATE_DESERT) {
        return IMAGE_CAMEL;
    } else {
        return image_group(GROUP_FIGURE_TRADE_CARAVAN);
    }
}

void figure_trade_caravan_action(figure *f)
{
    int move_speed = trader_bonus_speed();

    f->is_ghost = 0;

    if (config_get(CONFIG_GP_CARAVANS_MOVE_OFF_ROAD)) {
        f->terrain_usage = TERRAIN_USAGE_ANY;
    } else {
        f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    }

    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_100_TRADE_CARAVAN_CREATED:
            f->is_ghost = 1;
            f->wait_ticks++;
            if (f->wait_ticks > TRADER_INITIAL_WAIT) {
                f->wait_ticks = 0;
                go_to_next_storage(f);
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_101_TRADE_CARAVAN_ARRIVING:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            switch (f->direction) {
                case DIR_FIGURE_AT_DESTINATION:
                    f->action_state = FIGURE_ACTION_102_TRADE_CARAVAN_TRADING;
                    break;
                case DIR_FIGURE_REROUTE:
                    figure_route_remove(f);
                    break;
                case DIR_FIGURE_LOST:
                    f->state = FIGURE_STATE_DEAD;
                    f->is_ghost = 1;
                    break;
            }
            break;
        case FIGURE_ACTION_102_TRADE_CARAVAN_TRADING:
            f->wait_ticks++;
            if (f->wait_ticks > 10) {
                f->wait_ticks = 0;
                int move_on = 0;
                if (figure_trade_caravan_can_buy(f, f->destination_building_id, f->empire_city_id)) {
                    int resource = trader_get_buy_resource(f->destination_building_id, f->empire_city_id);
                    if (resource) {
                        trade_route_increase_traded(empire_city_get_route_id(f->empire_city_id), resource);
                        trader_record_bought_resource(f->trader_id, resource);
                        city_health_update_sickness_level_in_building(f->destination_building_id);
                        f->trader_amount_bought++;
                    } else {
                        move_on++;
                    }
                } else {
                    move_on++;
                }
                if (figure_trade_caravan_can_sell(f, f->destination_building_id, f->empire_city_id)) {
                    int resource = trader_get_sell_resource(f->destination_building_id, f->empire_city_id);
                    if (resource) {
                        trade_route_increase_traded(empire_city_get_route_id(f->empire_city_id), resource);
                        trader_record_sold_resource(f->trader_id, resource);
                        city_health_update_sickness_level_in_building(f->destination_building_id);
                        f->loads_sold_or_carrying++;
                    } else {
                        move_on++;
                    }
                } else {
                    move_on++;
                }
                if (move_on == 2) {
                    f->last_visited_index = figure_visited_buildings_add(f->last_visited_index,
                         f->destination_building_id);
                    go_to_next_storage(f);
                }
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_103_TRADE_CARAVAN_LEAVING:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            switch (f->direction) {
                case DIR_FIGURE_AT_DESTINATION:
                    f->action_state = FIGURE_ACTION_100_TRADE_CARAVAN_CREATED;
                    f->state = FIGURE_STATE_DEAD;
                    break;
                case DIR_FIGURE_REROUTE:
                    figure_route_remove(f);
                    break;
                case DIR_FIGURE_LOST:
                    f->state = FIGURE_STATE_DEAD;
                    break;
            }
            break;
    }
    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);


    f->image_id = trader_image_id() + dir + 8 * f->image_offset;
}

void figure_trade_caravan_donkey_action(figure *f)
{
    int move_speed = trader_bonus_speed();

    f->is_ghost = 0;

    if (config_get(CONFIG_GP_CARAVANS_MOVE_OFF_ROAD)) {
        f->terrain_usage = TERRAIN_USAGE_ANY;
    } else {
        f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    }

    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;

    figure *leader = figure_get(f->leading_figure_id);
    if (f->leading_figure_id <= 0) {
        f->state = FIGURE_STATE_DEAD;
    } else {
        if (leader->action_state == FIGURE_ACTION_149_CORPSE) {
            f->state = FIGURE_STATE_DEAD;
        } else if (leader->state != FIGURE_STATE_ALIVE) {
            f->state = FIGURE_STATE_DEAD;
        } else if (leader->type != FIGURE_TRADE_CARAVAN && leader->type != FIGURE_TRADE_CARAVAN_DONKEY) {
            f->state = FIGURE_STATE_DEAD;
        } else {
            figure_movement_follow_ticks_with_percentage(f, 1, move_speed);
        }
    }

    if (leader->is_ghost && !leader->height_adjusted_ticks) {
        f->is_ghost = 1;
    }
    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);

    f->image_id = trader_image_id() + dir + 8 * f->image_offset;
}

void figure_native_trader_action(figure *f)
{
    int move_speed = trader_bonus_speed();

    f->is_ghost = 0;
    f->terrain_usage = TERRAIN_USAGE_ANY;
    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_160_NATIVE_TRADER_GOING_TO_STORAGE:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_163_NATIVE_TRADER_AT_STORAGE;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
                f->is_ghost = 1;
            }
            if (building_get(f->destination_building_id)->state != BUILDING_STATE_IN_USE) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_161_NATIVE_TRADER_RETURNING:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            }
            break;
        case FIGURE_ACTION_162_NATIVE_TRADER_CREATED:
            f->is_ghost = 1;
            f->wait_ticks++;
            if (f->wait_ticks > 10) {
                f->wait_ticks = 0;
                map_point tile;
                int building_id = get_closest_storage(f, f->x, f->y, 0, &tile);
                if (building_id) {
                    f->action_state = FIGURE_ACTION_160_NATIVE_TRADER_GOING_TO_STORAGE;
                    f->destination_building_id = building_id;
                    f->destination_x = tile.x;
                    f->destination_y = tile.y;
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_163_NATIVE_TRADER_AT_STORAGE:
            f->wait_ticks++;
            if (f->wait_ticks > 10) {
                f->wait_ticks = 0;
                building *b = building_get(f->destination_building_id);
                int resource = get_native_trader_buy_resource(b); // preemptive check of resource to avoid standing idle
                if (building_storage_get_permission(BUILDING_STORAGE_PERMISSION_NATIVES, b) &&
                    f->trader_amount_bought < figure_trade_land_trade_units() && resource != RESOURCE_NONE) {
                    int removed = 0;
                    if (b->type == BUILDING_GRANARY) {
                        removed = building_granary_try_remove_resource(b, resource, 1);
                    } else if (b->type == BUILDING_WAREHOUSE) {
                        removed = building_warehouse_try_remove_resource(b, resource, 1);
                    }
                    if (removed) {
                        trader_record_bought_resource(f->trader_id, resource);
                        int price = trade_price_sell(resource, 1);
                        city_finance_process_export(price * removed);
                        city_health_update_sickness_level_in_building(f->destination_building_id);
                        f->trader_amount_bought += 3; //native traders 3 times less efficient
                    }

                } else {
                    map_point tile;
                    int building_id = get_closest_storage(f, f->x, f->y, 0, &tile);
                    if (building_id) {
                        f->action_state = FIGURE_ACTION_160_NATIVE_TRADER_GOING_TO_STORAGE;
                        f->destination_building_id = building_id;
                        f->destination_x = tile.x;
                        f->destination_y = tile.y;
                    } else {
                        f->action_state = FIGURE_ACTION_161_NATIVE_TRADER_RETURNING;
                        f->destination_x = f->source_x;
                        f->destination_y = f->source_y;
                    }
                }
            }
            f->image_offset = 0;
            break;
    }
    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);

    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->image_id = image_group(GROUP_FIGURE_CARTPUSHER) + 96 + figure_image_corpse_offset(f);
        f->cart_image_id = 0;
    } else {
        f->image_id = image_group(GROUP_FIGURE_CARTPUSHER) + dir + 8 * f->image_offset;
        f->cart_image_id = image_group(GROUP_FIGURE_MIGRANT_CART) + 8 + 8 * f->resource_id;
    }
    if (f->cart_image_id) {
        f->cart_image_id += dir;
        figure_image_set_cart_offset(f, dir);
    }
}

int figure_trade_ship_is_trading(figure *ship)
{
    building *b = building_get(ship->destination_building_id);
    if (b->state != BUILDING_STATE_IN_USE || b->type != BUILDING_DOCK) {
        return TRADE_SHIP_BUYING;
    }
    for (int i = 0; i < 3; i++) {
        figure *f = figure_get(b->data.distribution.cartpusher_ids[i]);
        if (!b->data.distribution.cartpusher_ids[i] || f->state != FIGURE_STATE_ALIVE) {
            continue;
        }
        switch (f->action_state) {
            case FIGURE_ACTION_133_DOCKER_IMPORT_QUEUE:
            case FIGURE_ACTION_135_DOCKER_IMPORT_GOING_TO_STORAGE:
            case FIGURE_ACTION_138_DOCKER_IMPORT_RETURNING:
            case FIGURE_ACTION_139_DOCKER_IMPORT_AT_STORAGE:
                return TRADE_SHIP_BUYING;
            case FIGURE_ACTION_134_DOCKER_EXPORT_QUEUE:
            case FIGURE_ACTION_136_DOCKER_EXPORT_GOING_TO_STORAGE:
            case FIGURE_ACTION_137_DOCKER_EXPORT_RETURNING:
            case FIGURE_ACTION_140_DOCKER_EXPORT_AT_STORAGE:
                return TRADE_SHIP_SELLING;
        }
    }
    return TRADE_SHIP_NONE;
}

static int trade_dock_ignoring_ship(figure *f)
{
    building *b = building_get(f->destination_building_id);
    if (b->state == BUILDING_STATE_IN_USE && b->type == BUILDING_DOCK && b->num_workers > 0 &&
        b->data.dock.trade_ship_id == f->id) {
        for (int i = 0; i < 3; i++) {
            if (b->data.distribution.cartpusher_ids[i]) {
                figure *docker = figure_get(b->data.distribution.cartpusher_ids[i]);
                if (docker->state == FIGURE_STATE_ALIVE && docker->action_state != FIGURE_ACTION_132_DOCKER_IDLING) {
                    return 0;
                }
            }
        }
        f->trade_ship_failed_dock_attempts++;
        if (f->trade_ship_failed_dock_attempts >= 10) {
            f->trade_ship_failed_dock_attempts = 11;
            return 1;
        }
        return 0;
    }
    return 1;
}

static int record_dock(figure *ship, int dock_id)
{
    building *dock = building_get(dock_id);
    if (dock->data.dock.trade_ship_id != 0 && dock->data.dock.trade_ship_id != ship->id) {
        return 0;
    }
    ship->last_visited_index = figure_visited_buildings_add(ship->last_visited_index, dock_id);
    return 1;
}

void figure_trade_ship_action(figure *f)
{
    int move_speed = sea_trader_bonus_speed();
    f->is_ghost = 0;
    f->is_boat = 1;
    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_110_TRADE_SHIP_CREATED:
            f->loads_sold_or_carrying = figure_trade_sea_trade_units();
            f->trader_amount_bought = 0;
            f->is_ghost = 1;
            f->wait_ticks++;
            if (f->wait_ticks > TRADER_INITIAL_WAIT) {
                f->wait_ticks = 0;
                map_point queue_tile;
                int dock_id = building_dock_get_destination(f->id, 0, &queue_tile);
                if (dock_id) {
                    f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
                    f->destination_x = queue_tile.x;
                    f->destination_y = queue_tile.y;
                    f->destination_building_id = dock_id;
                    f->wait_ticks = FIGURE_REROUTE_DESTINATION_TICKS;
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            f->height_adjusted_ticks = 0;
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->wait_ticks = 0;
                f->action_state = FIGURE_ACTION_114_TRADE_SHIP_ANCHORED;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                f->wait_ticks = 0;
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->wait_ticks = 0;
                f->state = FIGURE_STATE_DEAD;
            } else if (f->wait_ticks++ >= FIGURE_REROUTE_DESTINATION_TICKS) {
                f->wait_ticks = 0;
                map_point tile;
                int dock_id;
                if (!f->destination_building_id &&
                    (dock_id = building_dock_get_destination(f->id, 0, &tile)) != 0) {
                    f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
                    f->destination_building_id = dock_id;
                    f->destination_x = tile.x;
                    f->destination_y = tile.y;
                    figure_route_remove(f);
                }
                if ((dock_id = building_dock_get_closer_free_destination(f->id,
                    SHIP_DOCK_REQUEST_2_FIRST_QUEUE, &tile)) != 0) {
                    f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
                    f->destination_building_id = dock_id;
                    f->destination_x = tile.x;
                    f->destination_y = tile.y;
                    figure_route_remove(f);
                } else if (!building_dock_is_working(f->destination_building_id) ||
                    !building_dock_accepts_ship(f->id, f->destination_building_id)) {
                    if ((dock_id = building_dock_get_destination(f->id, 0, &tile)) != 0) {
                        f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
                        f->destination_building_id = dock_id;
                        f->destination_x = tile.x;
                        f->destination_y = tile.y;
                        figure_route_remove(f);
                    }
                }
            }
            break;
        case FIGURE_ACTION_114_TRADE_SHIP_ANCHORED:
            f->wait_ticks++;
            if (f->wait_ticks > 40) {
                f->wait_ticks = 0;
                map_point tile;
                int dock_id;
                if (!building_dock_is_working(f->destination_building_id) ||
                    !building_dock_accepts_ship(f->id, f->destination_building_id)) {
                    if ((dock_id = building_dock_get_destination(f->id, f->destination_building_id, &tile)) != 0) {
                        f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
                        f->destination_building_id = dock_id;
                        f->destination_x = tile.x;
                        f->destination_y = tile.y;
                    } else {
                        f->action_state = FIGURE_ACTION_115_TRADE_SHIP_LEAVING;
                        f->wait_ticks = 0;
                        map_point river_exit = scenario_map_river_exit();
                        f->destination_x = river_exit.x;
                        f->destination_y = river_exit.y;
                        switch (building_get(f->destination_building_id)->data.dock.orientation) {
                            case 0: f->direction = DIR_2_RIGHT; break;
                            case 1: f->direction = DIR_4_BOTTOM; break;
                            case 2: f->direction = DIR_6_LEFT; break;
                            default:f->direction = DIR_0_TOP; break;
                        }
                        f->image_offset = 0;
                        city_message_reset_category_count(MESSAGE_CAT_BLOCKED_DOCK);
                    }
                } else if (building_dock_request_docking(f->id, f->destination_building_id, &tile)) {
                    f->action_state = FIGURE_ACTION_111_TRADE_SHIP_GOING_TO_DOCK;
                    f->destination_x = tile.x;
                    f->destination_y = tile.y;
                    building *dock = building_get(f->destination_building_id);
                    dock->data.dock.trade_ship_id = f->id;
                } else if (
                    (dock_id = building_dock_get_closer_free_destination(f->id, SHIP_DOCK_REQUEST_1_DOCKING, &tile)) != 0 &&
                    building_dock_request_docking(f->id, dock_id, &tile)
                    ) {
                    f->action_state = FIGURE_ACTION_111_TRADE_SHIP_GOING_TO_DOCK;
                    f->destination_building_id = dock_id;
                    f->destination_x = tile.x;
                    f->destination_y = tile.y;
                    building *dock = building_get(f->destination_building_id);
                    dock->data.dock.trade_ship_id = f->id;
                } else if ((dock_id = building_dock_reposition_anchored_ship(f->id, &tile)) != 0) {
                    f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
                    f->destination_building_id = dock_id;
                    f->destination_x = tile.x;
                    f->destination_y = tile.y;
                }
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_111_TRADE_SHIP_GOING_TO_DOCK:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            f->height_adjusted_ticks = 0;
            f->trade_ship_failed_dock_attempts = 0;
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                if (record_dock(f, f->destination_building_id)) {
                    f->action_state = FIGURE_ACTION_112_TRADE_SHIP_MOORED;
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
                if (!city_message_get_category_count(MESSAGE_CAT_BLOCKED_DOCK)) {
                    city_message_post(1, MESSAGE_NAVIGATION_IMPOSSIBLE, 0, 0);
                    city_message_increase_category_count(MESSAGE_CAT_BLOCKED_DOCK);
                }
            }
            break;
        case FIGURE_ACTION_112_TRADE_SHIP_MOORED:
            if (!building_dock_is_working(f->destination_building_id) ||
                !building_dock_accepts_ship(f->id, f->destination_building_id) ||
                trade_dock_ignoring_ship(f)) {
                building *dock = building_get(f->destination_building_id);
                if (dock->data.dock.trade_ship_id == f->id) {
                    dock->data.dock.trade_ship_id = 0;
                }
                map_point tile;
                int dock_id;
                if ((dock_id = building_dock_get_destination(f->id, f->destination_building_id, &tile)) != 0) {
                    f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
                    f->destination_building_id = dock_id;
                    f->destination_x = tile.x;
                    f->destination_y = tile.y;
                } else {
                    f->destination_building_id = 0;
                    f->trade_ship_failed_dock_attempts = 0;
                    f->action_state = FIGURE_ACTION_115_TRADE_SHIP_LEAVING;
                    f->wait_ticks = 0;
                    map_point river_entry = scenario_map_river_entry();
                    f->destination_x = river_entry.x;
                    f->destination_y = river_entry.y;
                    building *dst = building_get(f->destination_building_id);
                    dst->data.dock.queued_docker_id = 0;
                    dst->data.dock.num_ships = 0;
                }
            } else {
                switch (building_get(f->destination_building_id)->data.dock.orientation) {
                    case 0: f->direction = DIR_2_RIGHT; break;
                    case 1: f->direction = DIR_4_BOTTOM; break;
                    case 2: f->direction = DIR_6_LEFT; break;
                    default:f->direction = DIR_0_TOP; break;
                }
            }
            f->image_offset = 0;
            city_message_reset_category_count(MESSAGE_CAT_BLOCKED_DOCK);
            break;
        case FIGURE_ACTION_115_TRADE_SHIP_LEAVING:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            f->destination_building_id = 0;
            f->height_adjusted_ticks = 0;
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_110_TRADE_SHIP_CREATED;
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
    }
    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
    f->image_id = image_group(GROUP_FIGURE_SHIP) + dir;
}

int figure_trade_land_trade_units(void)
{
    int unit = 8;

    if (building_monument_working(BUILDING_GRAND_TEMPLE_MERCURY)) {
        int add_unit = 0;
        int monument_id = building_monument_get_id(BUILDING_GRAND_TEMPLE_MERCURY);
        building *b = building_get(monument_id);

        int pct_workers = calc_percentage(b->num_workers, model_get_building(b->type)->laborers);
        if (pct_workers >= 100) { // full laborers
            add_unit = 4;
        } else if (pct_workers > 0) {
            add_unit = 2;
        }
        unit += add_unit;
    }

    if (building_caravanserai_is_fully_functional()) {
        building *b = building_get(city_buildings_get_caravanserai());

        trade_policy policy = city_trade_policy_get(LAND_TRADE_POLICY);

        int add_unit = 0;
        if (policy == TRADE_POLICY_3) {
            int pct_workers = calc_percentage(b->num_workers, model_get_building(b->type)->laborers);
            if (pct_workers >= 100) { // full laborers
                add_unit = POLICY_3_BONUS;
            } else if (pct_workers > 0) {
                add_unit = POLICY_3_BONUS / 2;
            }
        }
        unit += add_unit;
    }
    return unit;
}

int figure_trade_sea_trade_units(void)
{
    int unit = 12;
    if (building_monument_working(BUILDING_GRAND_TEMPLE_MERCURY)) {
        int add_unit = 0;
        building *b = building_get(building_find(BUILDING_GRAND_TEMPLE_MERCURY));

        int pct_workers = calc_percentage(b->num_workers, model_get_building(b->type)->laborers);
        if (pct_workers >= 100) { // full laborers
            add_unit = 6;
        } else if (pct_workers > 0) {
            add_unit = 3;
        }
        unit += add_unit;
    }

    if (building_lighthouse_is_fully_functional()) {
        trade_policy policy = city_trade_policy_get(SEA_TRADE_POLICY);

        int add_unit = 0;
        if (policy == TRADE_POLICY_3) {
            building *b = building_get(building_find(BUILDING_LIGHTHOUSE));

            int pct_workers = calc_percentage(b->num_workers, model_get_building(b->type)->laborers);
            if (pct_workers >= 100) { // full laborers
                add_unit = POLICY_3_BONUS;
            } else if (pct_workers > 0) {
                add_unit = POLICY_3_BONUS / 2;
            }
        }
        unit += add_unit;
    }

    return unit;
}

// if ship is moored, do not forward to another dock unless it has more than one third of capacity available.
// otherwise better leave, free space for new ships with full load of imports
int figure_trader_ship_can_queue_for_import(figure *ship)
{
    if (ship->action_state == FIGURE_ACTION_112_TRADE_SHIP_MOORED) {
        return ship->loads_sold_or_carrying >= (figure_trade_sea_trade_units() / 3);
    }
    return 1;
}

int figure_trader_ship_can_queue_for_export(figure *ship)
{
    if (ship->action_state == FIGURE_ACTION_112_TRADE_SHIP_MOORED) {
        int available_space = figure_trade_sea_trade_units() - ship->trader_amount_bought;
        return available_space >= (figure_trade_sea_trade_units() / 3);
    }
    return 1;
}

int figure_trader_ship_get_distance_to_dock(const figure *ship, int dock_id)
{
    if (ship->destination_building_id == dock_id) {
        return ship->routing_path_length - ship->routing_path_current_tile;
    }
    building *dock = building_get(dock_id);
    map_routing_calculate_distances_water_boat(ship->x, ship->y);
    uint8_t path[500];
    map_point tile;
    building_dock_get_ship_request_tile(dock, SHIP_DOCK_REQUEST_1_DOCKING, &tile);
    int path_length = map_routing_get_path_on_water(&path[0], tile.x, tile.y, 0);
    return path_length;
}

int figure_trader_ship_other_ship_closer_to_dock(int dock_id, int distance)
{
    for (int route_id = 0; route_id < 20; route_id++) {
        if (empire_object_is_sea_trade_route(route_id) && empire_city_is_trade_route_open(route_id)) {
            int city_id = empire_city_get_for_trade_route(route_id);
            if (city_id != -1) {
                empire_city *city = empire_city_get(city_id);
                for (int i = 0; i < 3; i++) {
                    figure *other_ship = figure_get(city->trader_figure_ids[i]);
                    if (other_ship->destination_building_id == dock_id && other_ship->routing_path_length) {
                        int other_ship_distance_to_dock = figure_trader_ship_get_distance_to_dock(other_ship, dock_id);
                        if (other_ship_distance_to_dock < distance) {
                            return other_ship->id;
                        }
                    }
                }
            }
        }
    }
    return 0;
}
