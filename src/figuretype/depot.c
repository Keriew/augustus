#include "depot.h"

#include "assets/assets.h"
#include "building/granary.h"
#include "building/industry.h"
#include "building/storage.h"
#include "building/warehouse.h"
#include "city/health.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/resource.h"
#include "map/road_access.h"
#include "map/road_network.h"
#include "map/routing.h"
#include "map/routing_terrain.h"

#define DEPOT_CART_PUSHER_SPEED 1

#define DEPOT_CART_PUSHER_FOOD_CAPACITY 16
#define DEPOT_CART_PUSHER_OTHER_CAPACITY 4

#define DEPOT_CART_REROUTE_DELAY 10
#define DEPOT_CART_LOAD_OFFLOAD_DELAY 10

static const int CART_OFFSETS_X[8] = { 24, 34, 29,  7, -15, -20, -13,  6 };
static const int CART_OFFSETS_Y[8] = { -5,  6, 17, 40,  15,   7,  -3, -6 };

static int cartpusher_carries_food(figure *f)
{
    return resource_is_food(f->resource_id);
}

static void set_cart_graphic(figure *f)
{
    int carried = f->loads_sold_or_carrying;
    if (carried == 0 || f->resource_id == RESOURCE_NONE) {
        f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART);
    } else if (carried == 1) {
        f->cart_image_id = resource_get_data(f->resource_id)->image.cart.single_load;
    } else if (cartpusher_carries_food(f) && carried >= 8) {
        f->cart_image_id = resource_get_data(f->resource_id)->image.cart.eight_loads;
    } else {
        f->cart_image_id = resource_get_data(f->resource_id)->image.cart.multiple_loads;
    }
}

static void set_cart_offset(figure *f, int direction)
{
    f->x_offset_cart = CART_OFFSETS_X[direction];
    f->y_offset_cart = CART_OFFSETS_Y[direction];

    if (f->loads_sold_or_carrying >= 8) {
        f->y_offset_cart -= 40;
    } else if (direction == 3) {
        f->y_offset_cart -= 20;
    }
}

static void update_image(figure *f)
{
    int dir = figure_image_normalize_direction(
        f->direction < 8 ? f->direction : f->previous_tile_direction);

    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->image_id = assets_lookup_image_id(ASSET_OX) + 1 + figure_image_corpse_offset(f);
        f->cart_image_id = 0;
    } else {
        f->image_id = assets_lookup_image_id(ASSET_OX) + 9 + dir * 12 + f->image_offset;
    }
    if (f->cart_image_id) {
        dir = (dir + 4) % 8;
        f->cart_image_id += dir;
        set_cart_offset(f, dir);
    }
}

static int get_storage_road_access(building *b, map_point *point)
{
    if (b->type == BUILDING_GRANARY) {
        map_point_store_result(b->x + 1, b->y + 1, point);
        return 1;
    } else if (b->type == BUILDING_WAREHOUSE) {
        if (b->has_road_access == 1) {
            map_point_store_result(b->x, b->y, point);
            return 1;
        } else {
            return map_has_road_access_warehouse(b->x, b->y, point);
        }
    } else {
        point->x = b->road_access_x;
        point->y = b->road_access_y;
        return b->has_road_access;
    }
}

static int is_order_condition_satisfied(const order *current_order)
{
    if (!current_order->src_storage_id || !current_order->dst_storage_id || !current_order->resource_type) {
        return 0;
    }

    if (current_order->condition.condition_type == ORDER_CONDITION_NEVER) {
        return 0;
    }
    building *src = building_get(current_order->src_storage_id);
    building *dst = building_get(current_order->dst_storage_id);
    if (!building_is_active(src) || !building_is_active(dst)) {
        return 0;
    }
    int src_amount = src->type == BUILDING_GRANARY ?
        building_granary_get_amount(src, current_order->resource_type) :
        building_warehouse_get_amount(src, current_order->resource_type);

    int dst_amount = dst->type == BUILDING_GRANARY ?
        building_granary_get_amount(dst, current_order->resource_type) :
        building_warehouse_get_amount(dst, current_order->resource_type);
    if (src_amount == 0) {
        return 0;
    }

    switch (current_order->condition.condition_type) {
        case ORDER_CONDITION_SOURCE_HAS_MORE_THAN:
            return src_amount >= current_order->condition.threshold;
        case ORDER_CONDITION_DESTINATION_HAS_LESS_THAN:
            return dst_amount < current_order->condition.threshold;
        default:
            return 1;
    }
}

/*
static int is_current_state_still_valid_for_order(const figure *f, const order *current_order)
{
    building *destination = building_get(f->destination_building_id);
    if (destination->state != BUILDING_STATE_IN_USE) {
        return 0;
    }

    switch (f->action_state) {
        case FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE:
        case FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE:
        {
            if (f->destination_building_id == current_order->src_storage_id) {
                return 1;
            } else {
                return 0;
            }
            break;
        }
        case FIGURE_ACTION_241_DEPOT_CART_HEADING_TO_DESTINATION:
        case FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION:
        {
            if (f->resource_id == current_order->resource_type
                && f->destination_building_id == current_order->dst_storage_id
                && f->loads_sold_or_carrying > 0) {
                return 1;
            } else {
                return 0;
            }
            break;
        }
    }
    return 1;
}

static void dump_goods(figure *f)
{
    if (f->loads_sold_or_carrying > 0) {
        // TODO maybe add a dump goods animation?
        f->loads_sold_or_carrying = 0;
        f->resource_id = RESOURCE_NONE;
        set_cart_graphic(f);
    }
}
*/

static int storage_remove_resource(building *b, int resource, int amount)
{
    if (b->type == BUILDING_GRANARY) {
        return building_granary_try_remove_resource(b, resource, amount);
    } else if (b->type == BUILDING_WAREHOUSE) {
        return building_warehouse_try_remove_resource(b, resource, amount);
    } else {
        return 0;
    }
}

static int storage_add_resource(building *b, int resource, int amount)
{

    while (amount > 0) {
        int unload_amount = (amount > 2) ? 2 : 1; // unload in 2s if possible
        unsigned char added_amount = building_storage_try_add_resource(b, resource, unload_amount, 0);
        if (added_amount <= 0) {
            return amount; // not enough space
        }
        amount -= added_amount;
    }
    return amount;
}

/*
static int check_valid_storages(order *current_order, int action_state)
{
    int valid_storages = 1;
    building *src = building_get(current_order->src_storage_id);
    if (building_storage_get_state(src, current_order->resource_type, 0) == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        current_order->src_storage_id = 0;
    }
    if (!src || !src->storage_id || src->state != BUILDING_STATE_IN_USE) {
        if (action_state == FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE ||
            action_state == FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE) {
            valid_storages = 0;
        }
        current_order->src_storage_id = 0;
    }
    building *dst = building_get(current_order->dst_storage_id);
    if (building_storage_get_state(dst, current_order->resource_type, 0) == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        current_order->dst_storage_id = 0;
    }
    if (!dst || !dst->storage_id || dst->state != BUILDING_STATE_IN_USE) {
        valid_storages = 0;
        current_order->dst_storage_id = 0;
    }
    return valid_storages || action_state == FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING;
}
*/

// Set figure destination and handle cart graphic and routing
static void figure_cart_set_destination(figure *f, int building_id, int action_state)
{
    if (!building_id) {
        return;
    }
    building *dest = building_get(building_id);
    if (!dest) {
        return;
    }

    map_point road_access;
    get_storage_road_access(dest, &road_access);

    f->destination_building_id = building_id;
    f->destination_x = road_access.x;
    f->destination_y = road_access.y;
    f->action_state = action_state;

    figure_route_remove(f);
    set_cart_graphic(f);
}

// Check if building can accept resource
static int figure_cart_destination_valid(building *b, int resource)
{
    return b && b->state == BUILDING_STATE_IN_USE &&
        building_storage_get_state(b, resource, 0) != BUILDING_STORAGE_STATE_NOT_ACCEPTING;
}

static void try_reroute_order_dst(figure *f, building *b)
{
    if (f->action_state != FIGURE_ACTION_241_DEPOT_CART_HEADING_TO_DESTINATION &&
        f->action_state != FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION) {
        return;
    }

    if (f->loads_sold_or_carrying <= 0 || f->resource_id == RESOURCE_NONE) {
        return;
    }

    int new_dst_id = b->data.depot.current_order.dst_storage_id;

    // If standing at destination and destination didn't change, skip reroute
    if (f->action_state == FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION &&
        (new_dst_id == f->destination_building_id || new_dst_id == 0)) {
        return;
    }

    building *new_dst = building_get(new_dst_id);
    if (figure_cart_destination_valid(new_dst, f->resource_id)) {
        figure_cart_set_destination(f, new_dst_id, FIGURE_ACTION_241_DEPOT_CART_HEADING_TO_DESTINATION);
    } else {
        f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
    }
}

static void figure_cart_unload_or_return(figure *f, building *b)
{
    if (f->loads_sold_or_carrying <= 0 || f->resource_id == RESOURCE_NONE) {
        f->action_state = FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING;
        figure_cart_set_destination(f, f->building_id, FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING);
        return;
    }

    building *dst = building_get(b->data.depot.current_order.dst_storage_id);
    int src_id = b->data.depot.current_order.src_storage_id;

    // If both the source and the destination are destroyed at the same time, do nothing
    if ((!dst || !figure_cart_destination_valid(dst, f->resource_id)) && !src_id) {
        return;
    }

    if (!figure_cart_destination_valid(dst, f->resource_id)) {
        if (src_id) {
            figure_cart_set_destination(f, src_id, FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE);
        } else {
            figure_cart_set_destination(f, f->building_id, FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING);
        }
        return;
    }

    // unload
    f->loads_sold_or_carrying = storage_add_resource(dst, f->resource_id, f->loads_sold_or_carrying);
    if (f->loads_sold_or_carrying == 0) {
        city_health_dispatch_sickness(f);
        f->action_state = FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING;
        f->resource_id = RESOURCE_NONE;
        figure_cart_set_destination(f, f->building_id, FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING);
    } else {
        set_cart_graphic(f);
    }
}

void figure_depot_cartpusher_action(figure *f)
{
    figure_image_increase_offset(f, 12);
    f->cart_image_id = 0;
    int speed_factor = DEPOT_CART_PUSHER_SPEED;
    int percentage_speed = 0;

    if (config_get(CONFIG_GP_CARAVANS_MOVE_OFF_ROAD)) {
        f->terrain_usage = TERRAIN_USAGE_ANY;
    } else {
        f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    }

    building *b = building_get(f->building_id);

    if (!b || b->type != BUILDING_DEPOT || b->state != BUILDING_STATE_IN_USE) {
        f->state = FIGURE_STATE_DEAD;
        update_image(f);
        return;
    }

    int is_linked = 0;
    for (int i = 0; i < 3; i++) {
        if (b->data.distribution.cartpusher_ids[i] == f->id) {
            is_linked = 1;
        }
    }
    if (!is_linked) {
        f->state = FIGURE_STATE_DEAD;
        update_image(f);
        return;
    }

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_238_DEPOT_CART_PUSHER_INITIAL:
        {
            set_cart_graphic(f);
            if (!map_routing_citizen_is_passable(f->grid_offset)) {
                f->state = FIGURE_STATE_DEAD;
                //break;
            }

            if (is_order_condition_satisfied(&b->data.depot.current_order)) {
                figure_cart_set_destination(f, b->data.depot.current_order.src_storage_id,
                    FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE);
                f->wait_ticks = DEPOT_CART_REROUTE_DELAY + 1;
            } else {
                f->state = FIGURE_STATE_DEAD;
            }
            f->image_offset = 0;
            break;
        }

        case FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE:
        case FIGURE_ACTION_241_DEPOT_CART_HEADING_TO_DESTINATION:
        case FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE:
        {
            set_cart_graphic(f);

            if (f->wait_ticks > DEPOT_CART_REROUTE_DELAY) {
                figure_movement_move_ticks_with_percentage(f, speed_factor, percentage_speed);

                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    if (f->action_state == FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE ||
                        f->action_state == FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE) {
                        f->action_state = FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE;
                    } else {
                        f->action_state = FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION;
                    }
                    f->wait_ticks = 0;
                } else if (f->direction == DIR_FIGURE_LOST) {
                    //f->action_state = FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING;
                    f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
                    f->wait_ticks = 0;
                } else if (f->direction == DIR_FIGURE_REROUTE) {
                    figure_route_remove(f);
                    f->wait_ticks = 0;
                }
            } else {
                f->wait_ticks++;
            }

            if (f->action_state == FIGURE_ACTION_241_DEPOT_CART_HEADING_TO_DESTINATION ||
                f->action_state == FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION) {
                try_reroute_order_dst(f, b);
            }
            break;
        }

        case FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE:
        {
            set_cart_graphic(f);
            f->wait_ticks++;
            if (f->wait_ticks > DEPOT_CART_LOAD_OFFLOAD_DELAY) {
                building *src = building_get(b->data.depot.current_order.src_storage_id);

                if (f->loads_sold_or_carrying > 0 && f->resource_id != RESOURCE_NONE) {
                    f->loads_sold_or_carrying = storage_add_resource(src, f->resource_id, f->loads_sold_or_carrying);
                    if (f->loads_sold_or_carrying == 0) {
                        f->resource_id = RESOURCE_NONE;
                        set_cart_graphic(f);
                        figure_cart_set_destination(f, f->building_id, FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING);
                    }
                    f->wait_ticks = 0;
                    break;
                }

                // Depot cartpusher waits if not enough goods
                int src_amount = src->type == BUILDING_GRANARY ?
                    building_granary_get_amount(src, b->data.depot.current_order.resource_type) :
                    building_warehouse_get_amount(src, b->data.depot.current_order.resource_type);
                if (b->data.depot.current_order.condition.condition_type == ORDER_CONDITION_SOURCE_HAS_MORE_THAN &&
                    src_amount < b->data.depot.current_order.condition.threshold) {
                    break;
                }

                // loading TODO upgradable?
                int capacity = resource_is_food(b->data.depot.current_order.resource_type) ?
                    DEPOT_CART_PUSHER_FOOD_CAPACITY : DEPOT_CART_PUSHER_OTHER_CAPACITY;
                int amount_loaded = storage_remove_resource(src, b->data.depot.current_order.resource_type, capacity);
                if (amount_loaded > 0) {
                    city_health_dispatch_sickness(f);
                    f->resource_id = b->data.depot.current_order.resource_type;
                    f->loads_sold_or_carrying = amount_loaded;

                    figure_cart_set_destination(f, b->data.depot.current_order.dst_storage_id,
                                                FIGURE_ACTION_241_DEPOT_CART_HEADING_TO_DESTINATION);
                    f->wait_ticks = DEPOT_CART_REROUTE_DELAY + 1;
                }
            }
            f->image_offset = 0;
            break;
        }

        case FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION:
            set_cart_graphic(f);
            f->wait_ticks++;
            if (f->wait_ticks > DEPOT_CART_LOAD_OFFLOAD_DELAY) {
                figure_cart_unload_or_return(f, b);
                f->wait_ticks = 0;
            }
            try_reroute_order_dst(f, b);
            break;

        case FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING:
            set_cart_graphic(f);
            figure_movement_move_ticks_with_percentage(f, speed_factor, percentage_speed);

            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_238_DEPOT_CART_PUSHER_INITIAL;
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_LOST) {
                //If source and destination are lost, wait for new instructions. Attempt not to lose the resources.
                //f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                figure_route_remove(f);
                f->wait_ticks = 0;
            }
            break;

        case FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER:
        {
            if (f->loads_sold_or_carrying > 0 && f->resource_id != RESOURCE_NONE) {
                figure_cart_set_destination(f, b->data.depot.current_order.src_storage_id,
                                            FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE);
            } else {
                figure_cart_set_destination(f, f->building_id, FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING);
            }
            break;
        }
    }

    update_image(f);
}

void figure_depot_recall(figure *f)
{
    building *b = building_get(f->building_id);
    if (f->loads_sold_or_carrying > 0 && f->resource_id != RESOURCE_NONE) {
        figure_cart_set_destination(f, b->data.depot.current_order.src_storage_id,
                                    FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE);
    } else {
        figure_cart_set_destination(f, f->building_id,
                                    FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING);
    }
}
