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

#define INFINITE 10000
#define TRADER_INITIAL_WAIT GAME_TIME_TICKS_PER_DAY
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define BASE_MULTIPLIER 100

static struct {
    int value_multiplier[RESOURCE_MAX];
} sell_multiplier;

static struct {
    int value_multiplier[RESOURCE_MAX];
} buy_multiplier;

// Initialize all multipliers to BASE_MULTIPLIER
static void resource_multiplier_init(void)
{
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        sell_multiplier.value_multiplier[r] = BASE_MULTIPLIER;
        buy_multiplier.value_multiplier[r] = BASE_MULTIPLIER;
        // add any rules that increase priority of a resource here, e.g.: resource_is_food(r) ? 150 : 100;
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
    if (b->type == BUILDING_GRANARY) {
        for (int r = RESOURCE_MIN_FOOD; r < RESOURCE_MAX_FOOD; r++) {
            if (empire_can_export_resource_to_city(city_id, r) &&
                building_granary_resource_amount(r, b) > 0) {
                return 1;
            }
        }
        return 0;
    }
    building *space = b;
    for (int i = 0; i < 8; i++) {
        space = building_next(space);
        if (space->id > 0 && space->resources[space->subtype.warehouse_resource_id] > 0 &&
            empire_can_export_resource_to_city(city_id, space->subtype.warehouse_resource_id)) {
            return 1;
        }
    }
    return 0;
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
    if (b->type == BUILDING_GRANARY) {
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            int resource = city_trade_next_caravan_import_resource();
            if (resource_is_food(resource) && !building_granary_is_not_accepting(resource, b) &&
                !building_granary_is_full(b) && empire_can_import_resource_from_city(city_id, resource)) {
                return 1;
            }
        }
        return 0;
    }
    int num_importable = 0;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (!building_warehouse_is_not_accepting(r, b)) {
            if (empire_can_import_resource_from_city(city_id, r)) {
                num_importable++;
            }
        }
    }
    if (num_importable <= 0) {
        return 0;
    }
    int can_import = 0;
    int resource = city_trade_current_caravan_import_resource();
    if (!building_warehouse_is_not_accepting(resource, b) &&
        empire_can_import_resource_from_city(city_id, resource)) {
        can_import = 1;
    } else {
        for (int i = RESOURCE_MIN; i < RESOURCE_MAX; i++) {
            resource = city_trade_next_caravan_import_resource();
            if (!building_warehouse_is_not_accepting(resource, b) &&
                empire_can_import_resource_from_city(city_id, resource)) {
                can_import = 1;
                break;
            }
        }
    }
    if (can_import) {
        // at least one resource can be imported and accepted by this warehouse
        // check if warehouse can store any importable goods
        building *space = b;
        for (int s = 0; s < 8; s++) {
            space = building_next(space);
            int loads_stored = space->resources[space->subtype.warehouse_resource_id];
            if (space->id > 0 && loads_stored < 4) {
                if (!loads_stored) {
                    // empty space
                    return 1;
                }
                if (empire_can_import_resource_from_city(city_id, space->subtype.warehouse_resource_id)) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

static int trader_get_buy_resource(int building_id, int city_id)
{
    building *b = building_get(building_id);
    if (b->type != BUILDING_WAREHOUSE && b->type != BUILDING_GRANARY) {
        return RESOURCE_NONE;
    }
    if (b->type == BUILDING_GRANARY) {
        if (!config_get(CONFIG_GP_CH_ALLOW_EXPORTING_FROM_GRANARIES)) {
            return RESOURCE_NONE;
        }
        for (int r = RESOURCE_MIN_FOOD; r < RESOURCE_MAX_FOOD; r++) {
            if (empire_can_export_resource_to_city(city_id, r) && building_granary_remove_export(b, r, 1)) {
                return r;
            }
        }
        return RESOURCE_NONE;
    }
    building *space = b;
    for (int i = 0; i < 8; i++) {
        space = building_next(space);
        if (space->id <= 0) {
            continue;
        }
        int resource = space->subtype.warehouse_resource_id;
        if (space->resources[resource] > 0 && empire_can_export_resource_to_city(city_id, resource)) {
            // update stocks
            city_resource_remove_from_warehouse(resource, 1);
            space->resources[resource]--;
            if (space->resources[resource] <= 0) {
                space->subtype.warehouse_resource_id = RESOURCE_NONE;
            }
            // update finances
            city_finance_process_export(trade_price_sell(resource, 1));

            // update graphics
            building_warehouse_space_set_image(space, resource);
            return resource;
        }
    }
    return RESOURCE_NONE;
}

static int trader_sell_resource(int building_id, int city_id)
{

    building *b = building_get(building_id);
    if (b->type != BUILDING_WAREHOUSE && b->type != BUILDING_GRANARY) {
        return 0;
    }

    if (b->type == BUILDING_GRANARY) {
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            int resource = city_trade_next_caravan_import_resource();
            if (!resource_is_food(resource) || !empire_can_import_resource_from_city(city_id, resource)) {
                continue;
            }
            if (building_granary_add_import(b, resource, 1)) {
                return resource;
            }
        }
        // find another importable resource that can be added to this granary
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            int resource = city_trade_next_caravan_backup_import_resource();
            if (!resource_is_food(resource) || !empire_can_import_resource_from_city(city_id, resource)) {
                continue;
            }
            if (building_granary_add_import(b, resource, 1)) {
                return resource;
            }
        }
        return 0;
    }

    int resource_to_import = city_trade_current_caravan_import_resource();
    int imp = RESOURCE_MIN;
    while (imp < RESOURCE_MAX && !empire_can_import_resource_from_city(city_id, resource_to_import)) {
        imp++;
        resource_to_import = city_trade_next_caravan_import_resource();
    }
    if (imp >= RESOURCE_MAX) {
        return 0;
    }



    // add to existing bay with room
    building *space = b;
    for (int i = 0; i < 8; i++) {
        space = building_next(space);
        if (space->id > 0 && space->resources[resource_to_import] > 0 && space->resources[resource_to_import] < 4 &&
            space->subtype.warehouse_resource_id == resource_to_import) {
            building_warehouse_space_add_import(space, resource_to_import, 1);
            city_trade_next_caravan_import_resource();
            return resource_to_import;
        }
    }
    // add to empty bay
    space = b;
    for (int i = 0; i < 8; i++) {
        space = building_next(space);
        if (space->id > 0 && space->subtype.warehouse_resource_id == RESOURCE_NONE) {
            building_warehouse_space_add_import(space, resource_to_import, 1);
            city_trade_next_caravan_import_resource();
            return resource_to_import;
        }
    }
    // find another importable resource that can be added to this warehouse
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        resource_to_import = city_trade_next_caravan_backup_import_resource();
        if (empire_can_import_resource_from_city(city_id, resource_to_import)) {
            space = b;
            for (int i = 0; i < 8; i++) {
                space = building_next(space);
                if (space->id > 0 && space->resources[resource_to_import] < 4
                    && space->subtype.warehouse_resource_id == resource_to_import) {
                    building_warehouse_space_add_import(space, resource_to_import, 1);
                    return resource_to_import;
                }
            }
        }
    }
    return 0;
}

static int determine_resource_to_trade()
{
    // replace all of the logic from city_trade_next_caravan and stuff with a loop that determines if a resource can be traded in a given place
}

static int get_closest_storage(const figure *f, int x, int y, int city_id, map_point *dst)
{
    const int max_trade_units = figure_trade_land_trade_units();
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
    // Reminder: Caravan can trade max 8 units each direction
    int sell_capacity = max_trade_units - f->loads_sold_or_carrying;
    int buy_capacity = max_trade_units - f->trader_amount_bought;

    int best_score = -1;
    building *best_building = NULL;

    //DEBUG AND TESTING BLOCK ++++++ REMOVE BEFORE RELEASE ++++++
#define MAX_STORAGE_DEBUG_ENTRIES 256
    typedef struct {
        int building_id;
        int building_type;
        int buy_score;
        int sell_score;
        int distance;
        int total_score;
    } storage_debug_entry;

    storage_debug_entry debug_entries[MAX_STORAGE_DEBUG_ENTRIES];
    int debug_entry_count = 0;
    //DEBUG AND TESTING BLOCK ------ REMOVE BEFORE RELEASE ------

    int building_types[] = { BUILDING_GRANARY, BUILDING_WAREHOUSE };
    for (int t = 0; t < 2; t++) {
        // Loop over all buildings of the current type (granary or warehouse)
        for (building *b = building_first_of_type(building_types[t]); b; b = b->next_of_type) {

            // Skip buildings that are not usable for trade
            if (b->state != BUILDING_STATE_IN_USE || b->has_plague || !b->has_road_access) {
                continue; // Not active, infected, or unreachable by road
            }

            int sell_score = 0; // Score for how many units the trader can sell to this building
            int buy_score = 0;  // Score for how many units the trader can buy from this building

            // Loop through all resource types
            for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {

                // Granaries only deal with food; skip non-food resources
                if (building_types[t] == BUILDING_GRANARY && !resource_is_food(r)) {
                    continue;
                }

                // === SELL SCORING: Trader ->Building ===
                if (sellable[r] > 0 && sell_capacity > 0) {
                    // Get how much of this resource the building can accept
                    int receptable = (building_types[t] == BUILDING_GRANARY)
                        ? building_granary_maximum_receptible_amount(r, b)
                        : building_warehouse_maximum_receptible_amount(r, b);

                    // Limit to how much can actually be sold here
                    int can_add = MIN(MIN(sellable[r], receptable), sell_capacity);
                    sell_score += can_add; // Add this to the total sell score
                }

                // === BUY SCORING: Building → Trader ===
                if (buyable[r] > 0 && buy_capacity > 0) {
                    // Get how much of this resource the building currently holds
                    int available = (building_types[t] == BUILDING_GRANARY)
                        ? building_granary_resource_amount(r, b)
                        : building_warehouse_get_available_amount(b, r);

                    // Limit to how much the trader can actually buy from here
                    int can_take = MIN(MIN(buyable[r], available), buy_capacity);
                    buy_score += can_take; // Add this to the total buy score
                }
            }
            // Calculate total value of this building to the trader
            int total_score = sell_score + buy_score;
            // If this building is the best candidate so far, store it
            if (total_score > best_score) {
                best_score = total_score;
                best_building = b;
            }

            // DEBUG AND TESTING BLOCK ++++++ REMOVE BEFORE RELEASE ++++++
            // Store debug info for this building to analyze scoring later
            if (debug_entry_count < MAX_STORAGE_DEBUG_ENTRIES) {
                debug_entries[debug_entry_count].building_id = b->id;
                debug_entries[debug_entry_count].building_type = building_types[t];
                debug_entries[debug_entry_count].buy_score = buy_score;
                debug_entries[debug_entry_count].sell_score = sell_score;
                debug_entries[debug_entry_count].total_score = total_score;
                debug_entry_count++;
            }
            // DEBUG AND TESTING BLOCK ------ REMOVE BEFORE RELEASE ------
        }
    }
    // 5. Return result (for now just return building ID and skip distance handling)
    if (best_building) {
        // Destination handling will be added later
        *dst = (map_point) { best_building->x, best_building->y };
        return best_building->id;
    }

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
            if (building_get(f->destination_building_id)->state != BUILDING_STATE_IN_USE) {
                f->state = FIGURE_STATE_DEAD;
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
                    int resource = trader_sell_resource(f->destination_building_id, f->empire_city_id);
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
                if (figure_trade_caravan_can_buy(f, f->destination_building_id, 0)) {
                    int resource = trader_get_buy_resource(f->destination_building_id, 0);
                    trader_record_bought_resource(f->trader_id, resource);
                    city_health_update_sickness_level_in_building(f->destination_building_id);
                    f->trader_amount_bought += 3;
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
