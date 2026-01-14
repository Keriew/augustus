#include "editor.h"

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
#include "empire/xml.h"
#include "window/editor/empire.h"
#include "window/empire.h"

#define BASE_BORDER_FLAG_IMAGE_ID 3323
#define BORDER_EDGE_DEFAULT_SPACING 50

static struct {
    empire_tool current_tool;
    int current_route_obj_id;
    int current_object_index;
} data = {
    .current_tool = EMPIRE_TOOL_OUR_CITY,
    .current_route_obj_id = -1,
    .current_object_index = 0
};

static int place_object(int mouse_x, int mouse_y);
static int delete_object(int mouse_x, int mouse_y);

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

int empire_editor_handle_placement(const mouse *m, const hotkeys *h)
{
    if (h->delete_empire_object && m->left.is_down) {
        return delete_object(m->x, m->y);
    }
    if (m->left.went_down) {
        if (h->delete_empire_object) {
            return delete_object(m->x, m->y);
        }
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

static void shift_edge_indices(const empire_object *const_obj)
{
    if (const_obj->order_index < data.current_object_index) {
        return;
    }
    if (const_obj->parent_object_id != empire_object_get_border()->id) {
        return;
    }
    empire_object *obj = empire_object_get(const_obj->id);
    obj->order_index++;
}

static int place_object(int mouse_x, int mouse_y)
{
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
        default:
            empire_object_remove(full->obj.id);
            return 0;
    }
    
    int is_edge = full->obj.type != EMPIRE_OBJECT_BORDER_EDGE;
    full->obj.x = editor_empire_mouse_to_empire_x(mouse_x) - ((full->obj.width / 2) * is_edge);
    full->obj.y = editor_empire_mouse_to_empire_y(mouse_y) - ((full->obj.height / 2) * is_edge);
    empire_transform_coordinates(&full->obj.x, &full->obj.y);
    
    if (is_edge) {
        data.current_object_index = empire_object_get_nearest_of_type(full->obj.x, full->obj.y, EMPIRE_OBJECT_BORDER_EDGE);
        empire_object_foreach_of_type(shift_edge_indices, EMPIRE_OBJECT_BORDER_EDGE);
        full->obj.order_index = data.current_object_index;
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
    empire_object_set_trade_route_coords(empire_object_get_our_city());
    
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

static void remove_trade_waypoints(const empire_object *obj)
{
    if (obj->parent_object_id == data.current_route_obj_id) {
        empire_object_remove(obj->id);
    }
}

static int delete_object(int mouse_x, int mouse_y)
{
    int empire_x = editor_empire_mouse_to_empire_x(mouse_x);
    int empire_y = editor_empire_mouse_to_empire_y(mouse_y);
    unsigned int obj_id = empire_object_get_at(empire_x, empire_y);
    if (!obj_id) {
        return 0;
    }
    full_empire_object *full = empire_object_get_full(obj_id);
    if (full->city_type == EMPIRE_CITY_OURS) {
        return 0;
    }
    if (full->obj.type == EMPIRE_OBJECT_CITY) {
        if (full->city_type == EMPIRE_CITY_FUTURE_TRADE || full->city_type == EMPIRE_CITY_TRADE) {
            empire_object *route_obj = empire_object_get(full->obj.id + 1);
            if (route_obj->type != EMPIRE_OBJECT_LAND_TRADE_ROUTE && route_obj->type != EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
                return 0;
            }
            data.current_route_obj_id = route_obj->id;
            empire_object_foreach_of_type(remove_trade_waypoints, EMPIRE_OBJECT_TRADE_WAYPOINT);
            data.current_route_obj_id = -1;
            empire_object_remove(route_obj->id);
            window_empire_collect_trade_edges();
        }
        empire_city_remove(empire_city_get_for_object(obj_id));
    }
    empire_object_remove(obj_id);
    
    if (full->obj.type == EMPIRE_OBJECT_TRADE_WAYPOINT) {
        window_empire_collect_trade_edges();
        empire_object_set_trade_route_coords(empire_object_get_our_city());
    }
    return 1;
}