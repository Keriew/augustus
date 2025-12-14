#include "editor.h"

#include "core/image.h"
#include "core/image_group.h"
#include "core/log.h"
#include "input/mouse.h"
#include "empire/empire.h"
#include "empire/object.h"
#include "window/editor/empire.h"

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
    
    const image *img = image_get(full->obj.image_id);
    full->obj.width = img->width;
    full->obj.height = img->height;
    
    full->obj.x = editor_empire_mouse_to_empire_x(mouse_x) - full->obj.width / 2;
    full->obj.y = editor_empire_mouse_to_empire_y(mouse_y) - full->obj.height / 2;
    empire_transform_coordinates(&full->obj.x, &full->obj.y);
    
    return 1;
}

static void create_trade_route_default(full_empire_object *full) {
    full_empire_object *route_obj = empire_object_get_new();
    if (!route_obj) {
        log_error("Error creating new object - out of memory", 0, 0);
        return;
    }
    route_obj->in_use = 1;
    route_obj->obj.type = EMPIRE_OBJECT_LAND_TRADE_ROUTE;
    full->trade_route_cost = 500;
}

static int place_city(full_empire_object *full)
{
    full->in_use = 1;
    full->obj.type = EMPIRE_OBJECT_CITY;
    
    switch (data.current_tool) {
        case EMPIRE_TOOL_OUR_CITY:
            if (empire_object_get_our_city()) {
                return 0;
            }
            full->city_type = EMPIRE_CITY_OURS;
            full->obj.image_id = image_group(GROUP_EMPIRE_CITY);
            break;
        case EMPIRE_TOOL_TRADE_CITY:
            full->city_type = EMPIRE_CITY_TRADE;
            full->obj.image_id = image_group(GROUP_EMPIRE_CITY_TRADE);
            create_trade_route_default(full);
            break;
        case EMPIRE_TOOL_ROMAN_CITY:
            full->city_type = EMPIRE_CITY_DISTANT_ROMAN;
            full->obj.image_id = image_group(GROUP_EMPIRE_CITY_DISTANT_ROMAN);
            break;
        case EMPIRE_TOOL_VULNERABLE_CITY:
            if (empire_city_get_vulnerable_roman()) {
                return 0;
            }
            full->city_type = EMPIRE_CITY_VULNERABLE_ROMAN;
            full->obj.image_id = image_group(GROUP_EMPIRE_CITY_DISTANT_ROMAN);
            break;
        case EMPIRE_TOOL_FUTURE_TRADE_CITY:
            full->city_type = EMPIRE_CITY_FUTURE_TRADE;
            full->obj.image_id = image_group(GROUP_EMPIRE_CITY_DISTANT_ROMAN);
            create_trade_route_default(full);
            break;
        case EMPIRE_TOOL_DISTANT_CITY:
            full->city_type = EMPIRE_CITY_DISTANT_FOREIGN;
            full->obj.image_id = image_group(GROUP_EMPIRE_FOREIGN_CITY);
            break;
        default:
            return 0; 
    }
    
    empire_object_add_to_cities(full);
    
    return 1;
}

static int place_border(full_empire_object *full)
{
    return 1;
}

static int place_battle(full_empire_object *full)
{
    return 1;
}

static int place_distant_battle(full_empire_object *full)
{
    return 1;
}