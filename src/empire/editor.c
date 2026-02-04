#include "editor.h"

#include "core/config.h"
#include "core/image.h"
#include "core/image_group.h"
#include "core/log.h"
#include "input/mouse.h"
#include "input/hotkey.h"
#include "input/input.h"
#include "empire/city.h"
#include "empire/empire.h"
#include "empire/object.h"
#include "empire/trade_route.h"
#include "empire/import_xml.h"
#include "graphics/window.h"
#include "translation/translation.h"
#include "window/editor/empire.h"
#include "window/empire.h"
#include "window/popup_dialog.h"

#define BASE_BORDER_FLAG_IMAGE_ID 3323
#define BORDER_EDGE_DEFAULT_SPACING 50

static struct {
    empire_tool current_tool;
    int foreach_param1; // used for empire_object_foreach callbacks
    int foreach_param2;
    int currently_moving;
    int move_id;
    int deletion_success;
    int deletion_id;
    int trade_waypoint_parent_id;
    int nearest_trade_waypoint;
} data = {
    .current_tool = EMPIRE_TOOL_OUR_CITY,
    .foreach_param1 = -1,
    .foreach_param2 = 0,
    .currently_moving = 0,
    .move_id = 0,
    .deletion_success = 0,
    .deletion_id = 0,
    .trade_waypoint_parent_id = 0,
    .nearest_trade_waypoint = 0
};

static int place_object(int mouse_x, int mouse_y);
static int delete_object_at(int mouse_x, int mouse_y);

static void update_tool_bounds(void)
{
    if (data.current_tool > EMPIRE_TOOL_MAX) {
        data.current_tool = data.current_tool % (EMPIRE_TOOL_MAX + 1);
    }
}

int empire_editor_get_tool(void)
{
    return data.current_tool;
}

void empire_editor_set_tool(empire_tool tool)
{
    data.current_tool = tool;
}

void empire_editor_change_tool(int amount)
{
    if ((int)data.current_tool + amount < EMPIRE_TOOL_MIN) {
        data.current_tool = EMPIRE_TOOL_MAX - ((int)data.current_tool - amount) + 1;
    } else {
        data.current_tool += amount;
    }
    update_tool_bounds();
}

empire_tool empire_editor_get_tool_for_object(const full_empire_object *full)
{
    switch (full->obj.type) {
        case EMPIRE_OBJECT_CITY:
            switch (full->city_type) {
                case EMPIRE_CITY_DISTANT_ROMAN:
                    return EMPIRE_TOOL_ROMAN_CITY;
                case EMPIRE_CITY_OURS:
                    return EMPIRE_TOOL_OUR_CITY;
                case EMPIRE_CITY_TRADE:
                    return EMPIRE_TOOL_TRADE_CITY;
                case EMPIRE_CITY_FUTURE_TRADE:
                    return EMPIRE_TOOL_FUTURE_TRADE_CITY;
                case EMPIRE_CITY_DISTANT_FOREIGN:
                    return EMPIRE_TOOL_DISTANT_CITY;
                case EMPIRE_CITY_VULNERABLE_ROMAN:
                    return EMPIRE_TOOL_VULNERABLE_CITY;
                default:
                    return 0;
            }
        case EMPIRE_OBJECT_BATTLE_ICON:
            return EMPIRE_TOOL_BATTLE;
        case EMPIRE_OBJECT_BORDER_EDGE:
            return EMPIRE_TOOL_BORDER;
        case EMPIRE_OBJECT_ROMAN_ARMY:
            return EMPIRE_TOOL_DISTANT_LEGION;
        case EMPIRE_OBJECT_ENEMY_ARMY:
            return EMPIRE_TOOL_DISTANT_BABARIAN;
        case EMPIRE_OBJECT_TRADE_WAYPOINT:
            if (empire_object_get(full->obj.parent_object_id)->type == EMPIRE_OBJECT_LAND_TRADE_ROUTE) {
                return EMPIRE_TOOL_LAND_ROUTE;
            } else {
                return EMPIRE_TOOL_SEA_ROUTE;
            }
        default:
            return 0;
    }
}

int empire_editor_handle_placement(const mouse *m, const hotkeys *h)
{
    if (h->delete_empire_object && (m->left.is_down || !config_get(CONFIG_UI_EMPIRE_CLICK_TO_DELETE))) {
        return delete_object_at(m->x, m->y);
    }
    if (m->left.went_down) {
        if (!place_object(m->x, m->y)) {
            return 0;
        } else {
            return 1;
        }
    }

    return 0;
}

static int place_city(full_empire_object *full);
static int place_border(full_empire_object *full);
static int place_battle(full_empire_object *full);
static int place_distant_battle(full_empire_object *full);
static int place_trade_waypoint(full_empire_object *full);

static void shift_edge_indices(const empire_object *const_obj)
{
    if (const_obj->order_index < data.foreach_param1) {
        return;
    }
    if (const_obj->parent_object_id != empire_object_get_border()->id) {
        return;
    }
    empire_object *obj = empire_object_get(const_obj->id);
    obj->order_index += data.foreach_param2;
}

static void shift_trade_point_indices(const empire_object *const_obj)
{
    if (const_obj->order_index < data.foreach_param1) {
        return;
    }
    if (const_obj->parent_object_id != data.foreach_param2) {
        return;
    }
    empire_object *obj = empire_object_get(const_obj->id);
    obj->order_index++;
}

static int condition_is_trade_type(const empire_object *obj)
{
    if (data.current_tool != EMPIRE_TOOL_LAND_ROUTE && data.current_tool != EMPIRE_TOOL_SEA_ROUTE) {
        return 0;
    }
    if (obj->parent_object_id != data.trade_waypoint_parent_id && data.trade_waypoint_parent_id != 0) {
        return 0;
    }
    if (data.current_tool == EMPIRE_TOOL_LAND_ROUTE &&
        empire_object_get(obj->parent_object_id)->type != EMPIRE_OBJECT_LAND_TRADE_ROUTE) {
        return 0;
    }
    if (data.current_tool == EMPIRE_TOOL_SEA_ROUTE &&
        empire_object_get(obj->parent_object_id)->type != EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
        return 0;
    }

    return 1;
}

static int condition_is_trade_city(const empire_object *obj)
{
    if (data.current_tool != EMPIRE_TOOL_LAND_ROUTE && data.current_tool != EMPIRE_TOOL_SEA_ROUTE) {
        return 0;
    }
    empire_city_type type = empire_object_get_full(obj->id)->city_type;
    if (type != EMPIRE_CITY_FUTURE_TRADE && type != EMPIRE_CITY_TRADE) {
        return 0;
    }
    if (data.current_tool == EMPIRE_TOOL_LAND_ROUTE &&
        empire_object_get(obj->parent_object_id)->type != EMPIRE_OBJECT_LAND_TRADE_ROUTE) {
        return 0;
    }
    if (data.current_tool == EMPIRE_TOOL_SEA_ROUTE &&
        empire_object_get(obj->parent_object_id)->type != EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
        return 0;
    }
    return 1;
}

static int place_object(int mouse_x, int mouse_y)
{
    if (data.currently_moving && data.current_tool == empire_editor_get_tool_for_object(empire_object_get_full(data.move_id))) {
        empire_editor_move_object_end(mouse_x, mouse_y);
        return 1;
    }
    full_empire_object *full = empire_object_get_new();
    
    if (!full) {
        log_error("Error creating new object - out of memory", 0, 0);
        return 0;
    }
    
    switch (data.current_tool) {
        case EMPIRE_TOOL_OUR_CITY:
        case EMPIRE_TOOL_TRADE_CITY:
        case EMPIRE_TOOL_ROMAN_CITY:
        case EMPIRE_TOOL_VULNERABLE_CITY:
        case EMPIRE_TOOL_FUTURE_TRADE_CITY:
        case EMPIRE_TOOL_DISTANT_CITY:
            if (!place_city(full)) {
                empire_object_remove(full->obj.id);
                return 0;
            }
            break;
        case EMPIRE_TOOL_BORDER:
            if (!place_border(full)) {
                empire_object_remove(full->obj.id);
                return 0;
            }
            break;
        case EMPIRE_TOOL_BATTLE:
            if (!place_battle(full)) {
                empire_object_remove(full->obj.id);
                return 0;
            }
            break;
        case EMPIRE_TOOL_DISTANT_BABARIAN:
        case EMPIRE_TOOL_DISTANT_LEGION:
            if (!place_distant_battle(full)) {
                empire_object_remove(full->obj.id);
                return 0;
            }
            break;
        case EMPIRE_TOOL_LAND_ROUTE:
        case EMPIRE_TOOL_SEA_ROUTE:
            if (!place_trade_waypoint(full)) {
                empire_object_remove(full->obj.id);
                return 0;
            }
            break;
        default:
            empire_object_remove(full->obj.id);
            return 0;
    }
    
    int is_edge = full->obj.type == EMPIRE_OBJECT_BORDER_EDGE;
    int is_trade_waypoint = full->obj.type == EMPIRE_OBJECT_TRADE_WAYPOINT;
    int x = editor_empire_mouse_to_empire_x(mouse_x) - ((full->obj.width / 2) * !is_edge);
    int y = editor_empire_mouse_to_empire_y(mouse_y) - ((full->obj.height / 2) * !is_edge);
    empire_transform_coordinates(&x, &y);
    // find nearest before assigning coordinates so it doesn't always find itself
    if (is_edge && config_get(CONFIG_UI_EMPIRE_SMART_BORDER_PLACMENT)) {
        int nearest_id = empire_object_get_nearest_of_type(x, y, EMPIRE_OBJECT_BORDER_EDGE);
        data.foreach_param1 = empire_object_get(nearest_id)->order_index;
        data.foreach_param2 = 1;
        empire_object_foreach_of_type(shift_edge_indices, EMPIRE_OBJECT_BORDER_EDGE);
        full->obj.order_index = data.foreach_param1;
    }
    
    if (is_trade_waypoint) {
        if (!data.trade_waypoint_parent_id || !data.nearest_trade_waypoint) {
            data.nearest_trade_waypoint = empire_object_get_nearest_of_type_with_condition(x, y,
                EMPIRE_OBJECT_TRADE_WAYPOINT, condition_is_trade_type);
            data.trade_waypoint_parent_id = empire_object_get(data.nearest_trade_waypoint)->parent_object_id;
        }
        int no_waypoint_found = !data.trade_waypoint_parent_id || !data.nearest_trade_waypoint;
        if (no_waypoint_found) {
            data.trade_waypoint_parent_id = empire_object_get_nearest_of_type_with_condition(x, y,
                EMPIRE_OBJECT_CITY, condition_is_trade_city) + 1;
        }
        if (data.trade_waypoint_parent_id <= 1) {
            empire_object_remove(full->obj.id);
            return 0;
        }

        full->obj.parent_object_id = data.trade_waypoint_parent_id;
        data.foreach_param1 = no_waypoint_found ? 1 : empire_object_get(data.nearest_trade_waypoint)->order_index;
        data.foreach_param2 = full->obj.parent_object_id;
        empire_object_foreach_of_type(shift_trade_point_indices, EMPIRE_OBJECT_TRADE_WAYPOINT);
        full->obj.order_index = data.foreach_param1;
        full->obj.trade_route_id = empire_object_get(full->obj.parent_object_id)->trade_route_id;
    }
    
    full->obj.x = x;
    full->obj.y = y;
    
    if (full->city_type == EMPIRE_CITY_TRADE || full->city_type == EMPIRE_CITY_FUTURE_TRADE || is_trade_waypoint) {
        window_empire_collect_trade_edges();
        empire_object_set_trade_route_coords(empire_object_get_our_city());
    }
    
    return 1;
}

static int create_trade_route_default(full_empire_object *full) {
    full_empire_object *route_obj = empire_object_get_new();
    if (!route_obj) {
        log_error("Error creating new object - out of memory", 0, 0);
        return 0;
    }
    route_obj->in_use = 1;
    route_obj->obj.type = EMPIRE_OBJECT_LAND_TRADE_ROUTE;
    full->trade_route_cost = 500;
    
    return 1;
}

static int place_city(full_empire_object *city_obj)
{
    city_obj->in_use = 1;
    city_obj->obj.type = EMPIRE_OBJECT_CITY;
    
    switch (data.current_tool) {
        case EMPIRE_TOOL_OUR_CITY:
            if (empire_object_get_our_city()) {
                return 0;
            }
            city_obj->city_type = EMPIRE_CITY_OURS;
            city_obj->obj.empire_city_icon = EMPIRE_CITY_ICON_OUR_CITY;
            city_obj->empire_city_icon = EMPIRE_CITY_ICON_OUR_CITY;
            break;
        case EMPIRE_TOOL_TRADE_CITY:
            if (!empire_object_get_our_city()) {
                return 0;
            }
            city_obj->city_type = EMPIRE_CITY_TRADE;
            city_obj->obj.empire_city_icon = EMPIRE_CITY_ICON_TRADE_CITY;
            city_obj->empire_city_icon = EMPIRE_CITY_ICON_TRADE_CITY;
            if (!create_trade_route_default(city_obj)) {
                return 0;
            }
            break;
        case EMPIRE_TOOL_ROMAN_CITY:
            city_obj->city_type = EMPIRE_CITY_DISTANT_ROMAN;
            city_obj->obj.empire_city_icon = EMPIRE_CITY_ICON_ROMAN_CITY;
            city_obj->empire_city_icon = EMPIRE_CITY_ICON_ROMAN_CITY;
            break;
        case EMPIRE_TOOL_VULNERABLE_CITY:
            if (empire_city_get_vulnerable_roman()) {
                return 0;
            }
            city_obj->city_type = EMPIRE_CITY_VULNERABLE_ROMAN;
            city_obj->obj.empire_city_icon = EMPIRE_CITY_ICON_ROMAN_CITY;
            city_obj->empire_city_icon = EMPIRE_CITY_ICON_ROMAN_CITY;
            break;
        case EMPIRE_TOOL_FUTURE_TRADE_CITY:
            if (!empire_object_get_our_city()) {
                return 0;
            }
            city_obj->city_type = EMPIRE_CITY_FUTURE_TRADE;
            city_obj->obj.empire_city_icon = EMPIRE_CITY_ICON_ROMAN_CITY;
            city_obj->empire_city_icon = EMPIRE_CITY_ICON_ROMAN_CITY;
            city_obj->obj.future_trade_after_icon = EMPIRE_CITY_ICON_TRADE_CITY;
            if (!create_trade_route_default(city_obj)) {
                return 0;
            }
            break;
        case EMPIRE_TOOL_DISTANT_CITY:
            city_obj->city_type = EMPIRE_CITY_DISTANT_FOREIGN;
            city_obj->obj.empire_city_icon = EMPIRE_CITY_ICON_DISTANT_CITY;
            city_obj->empire_city_icon = EMPIRE_CITY_ICON_DISTANT_CITY;
            break;
        default:
            return 0; 
    }
    
    city_obj->obj.image_id = empire_city_get_icon_image_id(city_obj->empire_city_icon);
    const image *img = image_get(city_obj->obj.image_id);
    city_obj->obj.width = img->width;
    city_obj->obj.height = img->height;

    empire_object_add_to_cities(city_obj);
    
    return 1;
}

static int place_border(full_empire_object *edge)
{
    edge->in_use = 1;
    unsigned int parent_id = 0; 
    const empire_object *current_border = empire_object_get_border();
    if (!current_border) {
        // create border
        full_empire_object *border = empire_object_get_new();
        if (!border) {
            log_error("Error creating new object - out of memory", 0, 0);
            return 0;
        }
        border->in_use = 1;
        border->obj.type = EMPIRE_OBJECT_BORDER;
        border->obj.width = BORDER_EDGE_DEFAULT_SPACING;
        
        parent_id = border->obj.id;
    } else {
        parent_id = current_border->id;
    }
    // place border edge
    edge->obj.type = EMPIRE_OBJECT_BORDER_EDGE;
    edge->obj.parent_object_id = parent_id;
    edge->obj.order_index = empire_object_get_highest_index(parent_id) + 1;
    edge->obj.image_id = BASE_BORDER_FLAG_IMAGE_ID;
    
    return 1;
}

static int place_battle(full_empire_object *battle_obj)
{
    set_battles_shown(1); // toggle the show invasions as you wont see your placed object otherwise
    
    battle_obj->in_use = 1;
    battle_obj->obj.type = EMPIRE_OBJECT_BATTLE_ICON;
    battle_obj->obj.invasion_path_id = *get_current_invasion_path_id();
    battle_obj->obj.invasion_years = empire_object_get_latest_battle(battle_obj->obj.invasion_path_id)->invasion_years + 1;

    battle_obj->obj.image_id = image_group(GROUP_EMPIRE_BATTLE);
    const image *img = image_get(battle_obj->obj.image_id);
    battle_obj->obj.width = img->width;
    battle_obj->obj.height = img->height;
    
    return 1;
}

static int place_distant_battle(full_empire_object *distant_battle)
{
    set_battles_shown(1); // toggle the show invasions as you wont see your placed object otherwise
    
    distant_battle->in_use = 1;
    distant_battle->obj.type = EMPIRE_OBJECT_BATTLE_ICON;
    
    if (data.current_tool == EMPIRE_TOOL_DISTANT_BABARIAN) {
        distant_battle->obj.type = EMPIRE_OBJECT_ENEMY_ARMY;
        distant_battle->obj.image_id = image_group(GROUP_EMPIRE_ENEMY_ARMY);
        distant_battle->obj.distant_battle_travel_months = empire_object_get_latest_distant_battle(1)->distant_battle_travel_months + 1;
    } else if (data.current_tool == EMPIRE_TOOL_DISTANT_LEGION) {
        distant_battle->obj.type = EMPIRE_OBJECT_ROMAN_ARMY;
        distant_battle->obj.image_id = image_group(GROUP_EMPIRE_ROMAN_ARMY);
        distant_battle->obj.distant_battle_travel_months = empire_object_get_latest_distant_battle(0)->distant_battle_travel_months + 1;
    } else {
        return 0;
    }
    
    const image *img = image_get(distant_battle->obj.image_id);
    distant_battle->obj.width = img->width;
    distant_battle->obj.height = img->height;
    
    return 1;
}

static int place_trade_waypoint(full_empire_object *waypoint)
{
    waypoint->in_use = 1;
    waypoint->obj.type = EMPIRE_OBJECT_TRADE_WAYPOINT;
    
    return 1;
}

static void remove_trade_waypoints(const empire_object *obj)
{
    if (obj->parent_object_id == data.foreach_param1) {
        empire_object_remove(obj->id);
    }
}

static void shift_trade_waypoints(const empire_object *const_obj)
{
    if (const_obj->order_index < data.foreach_param1) {
        return;
    }
    if (const_obj->parent_object_id != data.foreach_param2) {
        return;
    }
    empire_object *obj = empire_object_get(const_obj->id);
    obj->order_index--;
}

static void deletion_confirmed(int confirmed, int checked);

static int delete_object_at(int mouse_x, int mouse_y)
{    
    int empire_x = editor_empire_mouse_to_empire_x(mouse_x);
    int empire_y = editor_empire_mouse_to_empire_y(mouse_y);
    data.deletion_id = empire_object_get_at(empire_x, empire_y);
    if (!data.deletion_id) {
        return 0;
    }
    if (empire_object_get_full(data.deletion_id)->city_type == EMPIRE_CITY_OURS) {
        return 0;
    }
    if (config_get(CONFIG_UI_EMPIRE_CONFIRM_DELETE)) {
        window_popup_dialog_show_confirmation(translation_for(TR_EMPIRE_DELETE_OBJECT), 0, 0, deletion_confirmed);
    } else {
        deletion_confirmed(1, 0);
    }
    return data.deletion_success;
}

static void deletion_confirmed(int confirmed, int checked)
{
    if (!confirmed) {
        return;
    }
    data.deletion_success = empire_editor_delete_object(data.deletion_id);
}

int empire_editor_delete_object(unsigned int obj_id)
{
    if (!obj_id) {
        return 0;
    }
    full_empire_object *full = empire_object_get_full(obj_id);
    if (full->city_type == EMPIRE_CITY_OURS) {
        return 0;
    }
    empire_clear_selected_object();
    if (full->obj.type == EMPIRE_OBJECT_CITY) {
        if (full->city_type == EMPIRE_CITY_FUTURE_TRADE || full->city_type == EMPIRE_CITY_TRADE) {
            empire_object *route_obj = empire_object_get(full->obj.id + 1);
            if (route_obj->type != EMPIRE_OBJECT_LAND_TRADE_ROUTE && route_obj->type != EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
                return 0;
            }
            data.foreach_param1 = route_obj->id;
            empire_object_foreach_of_type(remove_trade_waypoints, EMPIRE_OBJECT_TRADE_WAYPOINT);
            data.foreach_param1 = -1;
            empire_object_remove(route_obj->id);
            window_empire_collect_trade_edges();
        }
        empire_city_remove(empire_city_get_for_object(obj_id));
    }
    empire_object_remove(obj_id);
    
    if (full->obj.type == EMPIRE_OBJECT_TRADE_WAYPOINT) {
        data.foreach_param1 = empire_object_get(obj_id)->order_index;
        data.foreach_param2 = full->obj.parent_object_id;
        empire_object_foreach_of_type(shift_trade_waypoints, EMPIRE_OBJECT_TRADE_WAYPOINT);
        window_empire_collect_trade_edges();
        empire_object_set_trade_route_coords(empire_object_get_our_city());
    }
    
    if (full->obj.type == EMPIRE_OBJECT_BORDER_EDGE) {
        data.foreach_param1 = empire_object_get(obj_id)->order_index;
        data.foreach_param2 = -1;
        empire_object_foreach_of_type(shift_edge_indices, EMPIRE_OBJECT_BORDER_EDGE);
    }
    
    return 1;
}

void empire_editor_move_object_start(unsigned int obj_id)
{
    data.currently_moving = 1;
    data.move_id = obj_id;
    data.current_tool = empire_editor_get_tool_for_object(empire_object_get_full(obj_id));
    window_request_refresh();
}

void empire_editor_move_object_end(int mouse_x, int mouse_y)
{
    if (!data.currently_moving) {
        return;
    }
    data.currently_moving = 0;
    empire_object *obj = empire_object_get(data.move_id);
    int is_edge = obj->type == EMPIRE_OBJECT_BORDER_EDGE;
    obj->x = editor_empire_mouse_to_empire_x(mouse_x) - ((obj->width / 2) * !is_edge);
    obj->y = editor_empire_mouse_to_empire_y(mouse_y) - ((obj->height / 2) * !is_edge);
    data.move_id = 0;
    window_empire_collect_trade_edges();
    empire_object_set_trade_route_coords(empire_object_get_our_city());
}

void empire_editor_move_object_stopp(void)
{
    data.currently_moving = 0;
    data.move_id = 0;
}

void empire_editor_set_trade_point_parent(int parent_id)
{
    data.trade_waypoint_parent_id = parent_id;
}

void empire_editor_clear_trade_point_parent(void)
{
    data.trade_waypoint_parent_id = 0;
}

int empire_editor_get_trade_point_parent(void)
{
    return data.trade_waypoint_parent_id;
}
