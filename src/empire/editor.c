#include "editor.h"

#include "core/image.h"
#include "core/image_group.h"
#include "core/log.h"
#include "input/mouse.h"
#include "empire/empire.h"
#include "empire/object.h"
#include "window/editor/empire.h"

#define BASE_BORDER_FLAG_IMAGE_ID 3323
#define BORDER_EDGE_DEFAULT_SPACING 50

static struct {
    empire_tool current_tool;
} data = {
    .current_tool = EMPIRE_TOOL_OUR_CITY
};

static int place_object(int mouse_x, int mouse_y);

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
    data.current_tool += amount;
    update_tool_bounds();
}

int empire_editor_handle_placement(const mouse *m)
{
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
                return 0;
            }
            break;
        case EMPIRE_TOOL_BORDER:
            if (!place_border(full)) {
                return 0;
            }
            break;
        case EMPIRE_TOOL_BATTLE:
            if (!place_battle(full)) {
                return 0;
            }
            break;
        case EMPIRE_TOOL_DISTANT_BABARIAN:
        case EMPIRE_TOOL_DISTANT_LEGION:
            if (!place_distant_battle(full)) {
                return 0;
            }
            break;
        default:
            return 0;
    }
    
    full->obj.x = editor_empire_mouse_to_empire_x(mouse_x) - ((full->obj.width / 2) * (full->obj.type == EMPIRE_OBJECT_CITY));
    full->obj.y = editor_empire_mouse_to_empire_y(mouse_y) - ((full->obj.height / 2) * (full->obj.type == EMPIRE_OBJECT_CITY));
    empire_transform_coordinates(&full->obj.x, &full->obj.y);
    
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
    edge->in_use = 1;
    edge->obj.type = EMPIRE_OBJECT_BORDER_EDGE;
    edge->obj.parent_object_id = parent_id;
    edge->obj.order_index = empire_object_get_highest_index(parent_id) + 1;
    edge->obj.image_id = BASE_BORDER_FLAG_IMAGE_ID;
    
    return 1;
}

static int place_battle(full_empire_object *battle)
{
    return 1;
}

static int place_distant_battle(full_empire_object *distant_battle)
{
    return 1;
}