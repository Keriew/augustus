#include "empire.h"

#include "assets/assets.h"
#include "core/image_group_editor.h"
#include "core/string.h"
#include "empire/city.h"
#include "empire/editor.h"
#include "empire/empire.h"
#include "empire/object.h"
#include "empire/trade_route.h"
#include "empire/type.h"
#include "empire/import_xml.h"
#include "empire/export_xml.h"
#include "graphics/arrow_button.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "input/mouse.h"
#include "input/scroll.h"
#include "scenario/data.h"
#include "scenario/editor.h"
#include "scenario/empire.h"
#include "window/editor/map.h"
#include "window/empire.h"
#include "window/file_dialog.h"
#include "window/numeric_input.h"
#include "window/select_list.h"
#include "window/text_input.h"

#include <math.h>
#include <stdio.h>

#define WIDTH_BORDER 32
#define HEIGHT_BORDER 176

#define OUR_CITY -1

#define DEFAULT_TRADE_QUOTA 25

#define RESOURCE_ICON_WIDTH 26 //resource icon width in px
#define RESOURCE_ICON_HEIGHT 26

typedef struct {
    int x;
    int y;
    int enabled;
    int highlighted;
} resource_button;

static void button_change_empire(int is_down, int param2);
static void button_ok(const generic_button *button);
static void button_toggle_invasions(const generic_button *button);
static void button_toggle_waypoints(const generic_button *button);
static void button_refresh(const generic_button *button);
static void button_export_xml(const generic_button *button);
static void button_import_xml(const generic_button *button);
static void button_cycle_preview(const generic_button *button);
static void button_recycle_preview(const generic_button *button);
static void button_add_resource(int param1, int param2);
static void button_edit_city_name(int param1, int param2);
static void button_icon_preview(const generic_button *button);
static void button_delete_object(const generic_button *button);
static void button_move_object(const generic_button *button);
static void button_draw_route(const generic_button *button);

static arrow_button arrow_buttons_empire[] = {
    {8, 48, 17, 24, button_change_empire, 1},
    {32, 48, 15, 24, button_change_empire}
};
static generic_button generic_buttons[] = {
    {4, 48, 100, 24, button_ok},
    {124, 48, 150, 24, button_toggle_invasions},
    {294, 48, 150, 24, button_toggle_waypoints},
    {464, 48, 150, 24, button_refresh, 0},
    {654, 48, 150, 24, button_export_xml, 0},
    {824, 48, 150, 24, button_import_xml, 0},
    {0, 0, 0, 24, button_icon_preview, 0},
};
static generic_button preview_button[] = {
    {0, 0, 72, 72, button_cycle_preview, button_recycle_preview},
};

static image_button add_resource_buttons[] = {
    {0, 0, 39, 24, IB_NORMAL, 0, 0, button_add_resource, button_none, 0, 0, 1, "UI", "Plus_Button_Idle"},
    {0, 0, 39, 24, IB_NORMAL, 0, 0, button_add_resource, button_none, 1, 0, 1, "UI", "Plus_Button_Idle"}
};
#define NUM_PLUS_BUTTONS sizeof(add_resource_buttons) / sizeof(image_button)

static image_button edit_city_name_button[] = {
    {0, 0, 24, 24, IB_NORMAL, 0, 0, button_edit_city_name, button_none, 0, 0, 1, "UI", "Edit_Button_Idle"}
};

static generic_button top_buttons[] = {
    {280, 0, 120, 24, button_delete_object},
    {140, 0, 120, 24, button_move_object},
    {0, 0, 120, 24, button_draw_route, 0}
};

static resource_button sell_buttons[RESOURCE_MAX] = { 0 };
static resource_button buy_buttons[RESOURCE_MAX] = { 0 };

static struct {
    unsigned int selected_button;
    int selected_city;
    int add_to_buying;
    int selected_resource;
    int available_resources[RESOURCE_MAX];
    int x_min, x_max, y_min, y_max;
    int x_draw_offset, y_draw_offset;
    unsigned int focus_button_id;
    unsigned int focus_top_button_id; 
    int is_scrolling;
    int finished_scroll;
    int show_battle_objects;
    int show_edges;
    unsigned int preview_button_focused;
    unsigned int button_is_preview;
    int resource_pulse_start;
    int object_coords_x;
    struct {
        int x;
        int y;
        int active;
    } coordinates;
    struct {
        int x_min;
        int x_max;
    } panel;
} data;

static void image_buttons_init(void);

static void update_screen_size(void)
{
    int s_width = screen_width();
    int s_height = screen_height();
    int map_width, map_height;
    empire_get_map_size(&map_width, &map_height);
    int max_width = map_width + WIDTH_BORDER;
    int max_height = map_height + HEIGHT_BORDER;

    data.x_min = s_width <= max_width ? 0 : (s_width - max_width) / 2;
    data.x_max = s_width <= max_width ? s_width : data.x_min + max_width;
    data.y_min = s_height <= max_height ? 0 : (s_height - max_height) / 2;
    data.y_max = s_height <= max_height ? s_height : data.y_min + max_height;

    int bottom_panel_width = data.x_max - data.x_min;
    if (bottom_panel_width < 608) {
        bottom_panel_width = 640;
        int difference = bottom_panel_width - (data.x_max - data.x_min);
        int odd = difference % 1;
        difference /= 2;
        data.panel.x_min = data.x_min - difference - odd;
        data.panel.x_max = data.x_max + difference;
    } else {
        data.panel.x_min = data.x_min;
        data.panel.x_max = data.x_max;
    }
    image_buttons_init();
}

static int map_viewport_width(void)
{
    return data.x_max - data.x_min - WIDTH_BORDER;
}

static int map_viewport_height(void)
{
    return data.y_max - data.y_min - HEIGHT_BORDER;
}

static void image_buttons_init(void)
{
    add_resource_buttons[0].y_offset = data.y_max - 133;
    add_resource_buttons[1].y_offset = data.y_max - 93;
    edit_city_name_button[0].y_offset = data.y_max - 133;
}

static void init(void)
{
    data.show_edges = 1;
    data.button_is_preview = 1;
    data.resource_pulse_start = 0;
    update_screen_size();
    data.selected_button = 0;
    data.coordinates.active = 0;
    int selected_object = empire_selected_object();
    if (selected_object) {
        data.selected_city = empire_city_get_for_object(selected_object - 1);
    } else {
        data.selected_city = 0;
    }
    data.focus_button_id = 0;
    image_buttons_init();
    empire_center_on_our_city(map_viewport_width(), map_viewport_height());
    window_empire_collect_trade_edges();
    data.resource_pulse_start = time_get_millis();
    data.object_coords_x = 0;
}

void set_battles_shown(int show)
{
    data.show_battle_objects = show;
}

static void draw_paneling(void)
{
    int image_base = image_group(GROUP_EDITOR_EMPIRE_PANELS);
    int bottom_panel_is_larger = data.x_min != data.panel.x_min;
    int vertical_y_limit = bottom_panel_is_larger ? data.y_max - 120 : data.y_max;

    graphics_set_clip_rectangle(data.panel.x_min, data.y_min,
        data.panel.x_max - data.panel.x_min, data.y_max - data.y_min);

    // bottom panel background
    for (int x = data.panel.x_min; x < data.panel.x_max; x += 70) {
        image_draw(image_base + 3, x, data.y_max - 160, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base + 3, x, data.y_max - 120, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base + 3, x, data.y_max - 80, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base + 3, x, data.y_max - 40, COLOR_MASK_NONE, SCALE_NONE);
    }

    // horizontal bar borders
    for (int x = data.panel.x_min; x < data.panel.x_max; x += 86) {
        image_draw(image_base + 1, x, data.y_max - 10, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base + 1, x, data.y_max - 160, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base + 1, x, data.y_max - 16, COLOR_MASK_NONE, SCALE_NONE);
    }

    // extra vertical bar borders
    if (bottom_panel_is_larger) {
        for (int y = vertical_y_limit + 16; y < data.y_max; y += 86) {
            image_draw(image_base, data.panel.x_min, y, COLOR_MASK_NONE, SCALE_NONE);
            image_draw(image_base, data.panel.x_max - 16, y, COLOR_MASK_NONE, SCALE_NONE);
        }
    }

    graphics_set_clip_rectangle(data.x_min, data.y_min, data.x_max - data.x_min, vertical_y_limit - data.y_min);

    for (int x = data.x_min; x < data.x_max; x += 86) {
        image_draw(image_base + 1, x, data.y_min, COLOR_MASK_NONE, SCALE_NONE);
    }

    // vertical bar borders
    for (int y = data.y_min + 16; y < vertical_y_limit; y += 86) {
        image_draw(image_base, data.x_min, y, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base, data.x_max - 16, y, COLOR_MASK_NONE, SCALE_NONE);
    }

    graphics_reset_clip_rectangle();

    // crossbars
    image_draw(image_base + 2, data.x_min, data.y_min, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(image_base + 2, data.x_min, data.y_max - 160, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(image_base + 2, data.panel.x_min, data.y_max - 16, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(image_base + 2, data.x_max - 16, data.y_min, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(image_base + 2, data.x_max - 16, data.y_max - 160, COLOR_MASK_NONE, SCALE_NONE);
    image_draw(image_base + 2, data.panel.x_max - 16, data.y_max - 16, COLOR_MASK_NONE, SCALE_NONE);

    if (bottom_panel_is_larger) {
        image_draw(image_base + 2, data.panel.x_min, data.y_max - 120, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_base + 2, data.panel.x_max - 16, data.y_max - 120, COLOR_MASK_NONE, SCALE_NONE);
    }
}

static int get_preview_image_group(empire_tool tool) {
    switch (tool) {
        case EMPIRE_TOOL_OUR_CITY:
            return GROUP_EMPIRE_CITY;
        case EMPIRE_TOOL_TRADE_CITY:
            return GROUP_EMPIRE_CITY_TRADE;
        case EMPIRE_TOOL_ROMAN_CITY:
        case EMPIRE_TOOL_VULNERABLE_CITY:
        case EMPIRE_TOOL_FUTURE_TRADE_CITY:
            return GROUP_EMPIRE_CITY_DISTANT_ROMAN;
        case EMPIRE_TOOL_DISTANT_CITY:
            return GROUP_EMPIRE_FOREIGN_CITY;
        case EMPIRE_TOOL_BORDER:
            return GROUP_EMPIRE_BORDER_EDGE;
        case EMPIRE_TOOL_BATTLE:
            return GROUP_EMPIRE_BATTLE;
        case EMPIRE_TOOL_DISTANT_BABARIAN:
            return GROUP_EMPIRE_ENEMY_ARMY;
        case EMPIRE_TOOL_DISTANT_LEGION:
            return GROUP_EMPIRE_ROMAN_ARMY;
        case EMPIRE_TOOL_LAND_ROUTE:
            return GROUP_EMPIRE_TRADE_LAND;
        case EMPIRE_TOOL_SEA_ROUTE:
            return GROUP_EMPIRE_TRADE_SEA;
        default:
            return 0;
    }
}

static int get_preview_translation_key(int tool_or_icon) {
    if (data.button_is_preview) {
        return TR_EMPIRE_TOOL_OUR_CITY + tool_or_icon;
    } else {
        return TR_EMPIRE_CITY_ICON_DEFAULT + tool_or_icon;
    }
}

static int draw_preview_image(int x, int y, int center, color_t color_mask, int draw_borders, int is_icon)
{
    int preview_image_group = get_preview_image_group(empire_editor_get_tool());
    
    if (!preview_image_group && !is_icon) {
        return 0;
    }
    
    if (preview_image_group == GROUP_EMPIRE_BORDER_EDGE) {
        draw_borders = 0;
    }
    if (is_icon && !empire_selected_object()) {
        return 0;
    }
    int image_id;
    if (is_icon) {
        image_id = empire_city_get_icon_image_id(empire_object_get(empire_selected_object() - 1)->empire_city_icon);
    } else {
        image_id = image_group(preview_image_group);
        if (preview_image_group == GROUP_EMPIRE_TRADE_LAND) {
            image_id = assets_get_image_id("UI", "LandWaypoint");
        } else if (preview_image_group == GROUP_EMPIRE_TRADE_SEA) {
            image_id = assets_get_image_id("UI", "SeaWaypoint");
        }
    }
    
    if (!image_id) {
        return 0;
    }
    const image *img = image_get(image_id);
    int x_offset = x + (center - img->width) / 2;
    int y_offset = y + (center - img->height) / 2;
    image_draw(image_id, x_offset, y_offset, color_mask, SCALE_NONE);
    if (img->animation && img->animation->speed_id) {
        image_draw(image_id + 1, x_offset + img->animation->sprite_offset_x,
            y_offset + img->animation->sprite_offset_y, color_mask, SCALE_NONE);
    }
    if (draw_borders) {
        graphics_draw_rect(x_offset, y_offset, img->width, img->height, COLOR_BLACK);
    }
    return 1;
}

static void draw_background(void)
{
    update_screen_size();
    if (data.x_min || data.y_min) {
        image_draw_blurred_fullscreen(image_group(GROUP_EDITOR_EMPIRE_MAP), 3);
        graphics_shade_rect(0, 0, screen_width(), screen_height(), 7);
    }
    draw_paneling();
    if (scenario_empire_id() == SCENARIO_CUSTOM_EMPIRE) {
        draw_preview_image(data.panel.x_max - 92, data.y_max - 100, 72, COLOR_MASK_NONE, 0, !data.button_is_preview);
    }
}

static void draw_shadowed_number(int value, int x, int y, color_t color)
{
    text_draw_number(value, '@', " ", x + 1, y - 1, FONT_SMALL_PLAIN, COLOR_BLACK);
    text_draw_number(value, '@', " ", x, y, FONT_SMALL_PLAIN, color);
}

static void draw_empire_object(const empire_object *obj)
{
    int x = obj->x;
    int y = obj->y;
    int image_id = obj->image_id;

    if (obj->type == EMPIRE_OBJECT_BORDER_EDGE) {
        if (!data.show_edges) {
            return;
        }
        image_id = assets_get_image_id("UI", "BorderWaypoint");
        x -= 19 / 2;
        y -= 18 / 2;
    }
    
    if (obj->type == EMPIRE_OBJECT_TRADE_WAYPOINT) {
        if (!data.show_edges) {
            return;
        }
        int is_sea_route = empire_object_get(obj->parent_object_id)->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE;
        image_id = assets_get_image_id("UI", is_sea_route ? "SeaWaypoint" : "LandWaypoint");
        x -= 19 / 2;
        y -= 18 / 2;
    }
    
    if (!data.show_battle_objects && (
        obj->type == EMPIRE_OBJECT_BATTLE_ICON ||
        obj->type == EMPIRE_OBJECT_ROMAN_ARMY ||
        obj->type == EMPIRE_OBJECT_ENEMY_ARMY)) {
        return;
    }
    if (obj->type == EMPIRE_OBJECT_BORDER) {
        window_empire_draw_border(obj, data.x_draw_offset, data.y_draw_offset);
    }
    if (obj->type == EMPIRE_OBJECT_CITY) {
        image_id = empire_city_get_icon_image_id(obj->empire_city_icon);
    } else if (obj->type == EMPIRE_OBJECT_BATTLE_ICON) {
        draw_shadowed_number(obj->invasion_path_id,
            data.x_draw_offset + x - 9, data.y_draw_offset + y - 9, COLOR_WHITE);
        draw_shadowed_number(obj->invasion_years,
            data.x_draw_offset + x + 15, data.y_draw_offset + y - 9, COLOR_FONT_RED);
    } else if (obj->type == EMPIRE_OBJECT_ROMAN_ARMY || obj->type == EMPIRE_OBJECT_ENEMY_ARMY) {
        draw_shadowed_number(obj->distant_battle_travel_months,
            data.x_draw_offset + x + 7, data.y_draw_offset + y - 9,
            obj->type == EMPIRE_OBJECT_ROMAN_ARMY ? COLOR_WHITE : COLOR_FONT_RED);
    }

    if (obj->type == EMPIRE_OBJECT_ORNAMENT) {
        if (image_id < 0) {
            image_id = assets_lookup_image_id(ASSET_FIRST_ORNAMENT) - 1 - image_id;
        }
    }
    image_draw(image_id, data.x_draw_offset + x, data.y_draw_offset + y, COLOR_MASK_NONE, SCALE_NONE);
    const image *img = image_get(image_id);
    if (img->animation && img->animation->speed_id) {
        int new_animation = empire_object_update_animation(obj, image_id);
        image_draw(image_id + new_animation,
            data.x_draw_offset + x + img->animation->sprite_offset_x,
            data.y_draw_offset + y + img->animation->sprite_offset_y,
            COLOR_MASK_NONE, SCALE_NONE);
    }
    // Manually fix the Hagia Sophia
    if (obj->image_id == 3372) {
        image_id = assets_lookup_image_id(ASSET_HAGIA_SOPHIA_FIX);
        image_draw(image_id, data.x_draw_offset + x, data.y_draw_offset + y, COLOR_MASK_NONE, SCALE_NONE);
    }
}

static void show_coords(int x_offset, int y_offset, const uint8_t *title, int x_coord, int y_coord)
{
    uint8_t text[100];
    int len = 0;
    string_copy(title, text, 100);
    len = string_length(text);
    string_from_int(text + len, x_coord, 0);
    len = string_length(text);
    string_copy(string_from_ascii(", "), text + len, 100 - len);
    len += 2;
    string_from_int(text + len, y_coord, 0);

    graphics_draw_rect(x_offset, y_offset, 220, 25, COLOR_BLACK);
    graphics_fill_rect(x_offset + 1, y_offset + 1, 218, 23, COLOR_WHITE);

    text_draw_centered(text, x_offset, y_offset + 7, 220, FONT_NORMAL_BLACK, 0);
}

static int is_outside_map(int x, int y)
{
    return (x < data.x_min + 16 || x >= data.x_max - 16 ||
            y < data.y_min + 16 || y >= data.y_max - 160);
}

int editor_empire_mouse_to_empire_x(int x)
{
    int x_coord = 0;
    
    if (x < data.x_min + 16) {
        x_coord = data.x_min + 16 - data.x_draw_offset;
    } else if (x >= data.x_max - 16) {
        x_coord = data.x_max - 17 - data.x_draw_offset;
    } else {
        x_coord = x - data.x_draw_offset;
    }
    return x_coord;
}

int editor_empire_mouse_to_empire_y(int y)
{
    int y_coord = 0;
    
    if (y < data.y_min + 16) {
        y_coord = data.y_min + 16 - data.y_draw_offset;
    } else if (y >= data.y_max - 16) {
        y_coord = data.y_max - 17 - data.y_draw_offset;
    } else {
        y_coord = y - data.y_draw_offset;
    }
    return y_coord;
}

static void draw_coordinates(void)
{
    if (scenario.empire.id != SCENARIO_CUSTOM_EMPIRE) {
        return;
    }
    const mouse *m = mouse_get();

    int x_coord = editor_empire_mouse_to_empire_x(m->x);
    int y_coord = editor_empire_mouse_to_empire_y(m->y);

    empire_restore_coordinates(&x_coord, &y_coord);

    show_coords(data.x_min + 20, data.y_min + 20,
        lang_get_string(CUSTOM_TRANSLATION, TR_EDITOR_CURRENT_COORDS), x_coord, y_coord);
    if (!(m->left.is_down && !is_outside_map(m->x, m->y))) {
        draw_preview_image(m->x, m->y, 0, ALPHA_FONT_SEMI_TRANSPARENT, 0, 0);
    }

    if (data.coordinates.active) {
        x_coord = data.coordinates.x + data.x_draw_offset;
        y_coord = data.coordinates.y + data.y_draw_offset;
        empire_transform_coordinates(&x_coord, &y_coord);
        show_coords(data.x_min + 20, data.y_min + 50,
            lang_get_string(CUSTOM_TRANSLATION, TR_EDITOR_SELECTED_COORDS),
                data.coordinates.x, data.coordinates.y);
        if (!draw_preview_image(x_coord, y_coord, 0, ALPHA_FONT_SEMI_TRANSPARENT, 1, 0)) {
            graphics_draw_rect(x_coord - 3, y_coord - 3, 7, 7, COLOR_BLACK);
            graphics_draw_rect(x_coord - 1, y_coord - 1, 3, 3, COLOR_WHITE);
        }
    }
}

static void draw_trade_waypoints(const empire_object *obj)
{
    window_empire_draw_static_trade_waypoints(obj, data.x_draw_offset, data.y_draw_offset);
}

static void draw_map(void)
{
    int viewport_width = map_viewport_width();
    int viewport_height = map_viewport_height();
    graphics_set_clip_rectangle(data.x_min + 16, data.y_min + 16, viewport_width, viewport_height);

    empire_reset_route_drawn_flags();
    empire_set_viewport(viewport_width, viewport_height);

    data.x_draw_offset = data.x_min + 16;
    data.y_draw_offset = data.y_min + 16;
    empire_adjust_scroll(&data.x_draw_offset, &data.y_draw_offset);
    image_draw(empire_get_image_id(), data.x_draw_offset, data.y_draw_offset,
        COLOR_MASK_NONE, SCALE_NONE);
    
    if (data.resource_pulse_start == 0) {
        data.resource_pulse_start = time_get_millis();
    }

    empire_object_foreach(draw_empire_object);
    empire_object_foreach_of_type(draw_trade_waypoints, EMPIRE_OBJECT_SEA_TRADE_ROUTE);
    empire_object_foreach_of_type(draw_trade_waypoints, EMPIRE_OBJECT_LAND_TRADE_ROUTE);
    empire_object_foreach_of_type(draw_empire_object, EMPIRE_OBJECT_TRADE_WAYPOINT);
    empire_object_foreach_of_type(draw_empire_object, EMPIRE_OBJECT_LAND_TRADE_ROUTE);
    empire_object_foreach_of_type(draw_empire_object, EMPIRE_OBJECT_SEA_TRADE_ROUTE);
    empire_object_foreach_of_type(draw_empire_object, EMPIRE_OBJECT_CITY);

    draw_coordinates();

    graphics_reset_clip_rectangle();
}

static int draw_resource(resource_type resource, int trade_max, int x_offset, int y_offset, int is_highlighted)
{
    graphics_draw_inset_rect(x_offset, y_offset, RESOURCE_ICON_WIDTH, RESOURCE_ICON_HEIGHT, COLOR_INSET_DARK, COLOR_INSET_LIGHT);
    image_draw(resource_get_data(resource)->image.editor.empire, x_offset + 1, y_offset + 1,
        COLOR_MASK_NONE, SCALE_NONE);
    if (is_highlighted) {
        time_millis elapsed = time_get_millis() - data.resource_pulse_start;
        float time_seconds = elapsed / 1000.0f; // Convert to seconds
        float pulse = sinf(time_seconds * 1.0f * 3.14f); // 1 full cycle per second
        int alpha = 96 + (int) (pulse * 64); // Range: 32–160
        graphics_tint_rect(x_offset, y_offset, RESOURCE_ICON_WIDTH - 1, RESOURCE_ICON_HEIGHT - 1,
            COLOR_MASK_DARK_PINK, alpha);
    }
    if (trade_max == OUR_CITY) {
        return 0;
    }
    window_empire_draw_resource_shields(trade_max, x_offset, y_offset);
    return text_draw_number(trade_max, '\0', "", x_offset + 28, y_offset + 9, FONT_NORMAL_GREEN, 0);
}

static void draw_city_info(const empire_city *city)
{
    int x_offset = data.panel.x_min + 28;
    int y_offset = data.y_max - 125;
    const uint8_t *city_name = empire_city_get_name(city);
    int width = text_draw(city_name, x_offset, y_offset, FONT_NORMAL_WHITE, 0);
    if (scenario.empire.id == SCENARIO_CUSTOM_EMPIRE) {
        width += 10;
    }
    edit_city_name_button[0].x_offset = width + x_offset;
    if (scenario.empire.id == SCENARIO_CUSTOM_EMPIRE) {
        width += 24;
    }
    int width_after_name = width;
    int etc_width = text_get_width(string_from_ascii("..."), FONT_NORMAL_GREEN);
    
    top_buttons[2].parameter1 = 1;

    switch (city->type) {
        case EMPIRE_CITY_DISTANT_ROMAN:
        case EMPIRE_CITY_VULNERABLE_ROMAN:
            lang_text_draw(47, 12, x_offset + 20 + width, y_offset, FONT_NORMAL_GREEN);
            break;
        case EMPIRE_CITY_DISTANT_FOREIGN:
        case EMPIRE_CITY_FUTURE_ROMAN:
            lang_text_draw(47, 0, x_offset + 20 + width, y_offset, FONT_NORMAL_GREEN);
            break;
        case EMPIRE_CITY_OURS:
        {
            add_resource_buttons[1].dont_draw = 1;
            width += lang_text_draw(47, 1, x_offset + 20 + width, y_offset, FONT_NORMAL_GREEN);
            int resource_x_offset = x_offset + 30 + width;
            for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                if (empire_object_city_sells_resource(city->empire_object_id, r)) {
                    if (resource_x_offset + etc_width + 32 + 8 + 39 >= data.panel.x_max - 280) {
                        text_draw(string_from_ascii("..."), resource_x_offset + 2, y_offset + 4, FONT_NORMAL_GREEN, COLOR_MASK_NONE);
                        resource_x_offset += etc_width + 8;
                        break;
                    }
                    draw_resource(r, OUR_CITY, resource_x_offset, y_offset - 9, sell_buttons[r].highlighted);
                    sell_buttons[r] = (resource_button){resource_x_offset, y_offset, 1, 0};
                    resource_x_offset += 32;
                } else {
                    sell_buttons[r] = (resource_button){0, 0, 0, 0};
                }
            }
            add_resource_buttons[0].x_offset = resource_x_offset;
            if (scenario_empire_id() == SCENARIO_CUSTOM_EMPIRE) {
                image_buttons_draw(0, 0, add_resource_buttons, NUM_PLUS_BUTTONS);
            }
            break;
        }
        case EMPIRE_CITY_TRADE:
        case EMPIRE_CITY_FUTURE_TRADE:
        {
            top_buttons[2].parameter1 = 0;
            add_resource_buttons[1].dont_draw = 0;
            int text_width = lang_text_draw(47, 5, x_offset + 20 + width, y_offset, FONT_NORMAL_GREEN);
            width += text_width;
            int resource_x_offset = x_offset + 30 + width;
            for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                if (empire_object_city_sells_resource(city->empire_object_id, r)) {
                    int max_trade = trade_route_limit(city->route_id, r, 0);
                    int resource_width = 32 + text_get_number_width(max_trade, '\0', "", FONT_NORMAL_GREEN);
                    if (resource_x_offset + etc_width + resource_width + 8 + 39 >= data.panel.x_max - 420) {
                        text_draw(string_from_ascii("..."), resource_x_offset + 2, y_offset + 4, FONT_NORMAL_GREEN, COLOR_MASK_NONE);
                        resource_x_offset += etc_width + 8;
                        break;
                    }
                    width = draw_resource(r, max_trade, resource_x_offset, y_offset - 9, sell_buttons[r].highlighted);
                    sell_buttons[r] = (resource_button){resource_x_offset, y_offset, 1, 0};
                    resource_x_offset += 32 + width;
                } else {
                    sell_buttons[r] = (resource_button){0, 0, 0, 0};
                }
            }
            add_resource_buttons[0].x_offset = resource_x_offset;
            y_offset = data.y_max - 85;
            resource_x_offset = x_offset + 20 + width_after_name;
            int coords_bonus = 0;
            if (data.object_coords_x >= resource_x_offset) {
                coords_bonus = data.object_coords_x - resource_x_offset;
            }
            text_width = lang_text_draw(47, 4, resource_x_offset + coords_bonus, y_offset, FONT_NORMAL_GREEN);
            width = width_after_name + text_width + coords_bonus;
            resource_x_offset = x_offset + 30 + width;
            for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                if (empire_object_city_buys_resource(city->empire_object_id, r)) {
                    int max_trade = trade_route_limit(city->route_id, r, 1);
                    int resource_width = 32 + text_get_number_width(max_trade, '\0', "", FONT_NORMAL_GREEN);
                    if (resource_x_offset + etc_width + generic_buttons[6].width + 96 + resource_width + 8 + 39 >= data.panel.x_max) {
                        text_draw(string_from_ascii("..."), resource_x_offset + 2, y_offset + 4, FONT_NORMAL_GREEN, COLOR_MASK_NONE);
                        resource_x_offset += etc_width + 8;
                        break;
                    }
                    width = draw_resource(r, max_trade, resource_x_offset, y_offset - 9, buy_buttons[r].highlighted);
                    buy_buttons[r] = (resource_button){resource_x_offset, y_offset, 1, 0};
                    resource_x_offset += 32 + width;
                } else {
                    buy_buttons[r] = (resource_button){0, 0, 0, 0};
                }
            }
            add_resource_buttons[1].x_offset = resource_x_offset;
            if (scenario_empire_id() == SCENARIO_CUSTOM_EMPIRE) {
                image_buttons_draw(0, 0, add_resource_buttons, NUM_PLUS_BUTTONS);
            }
            
            if (!top_buttons[2].parameter1) {
                button_border_draw(data.panel.x_max - 420, data.y_max - 133, 120, 24, data.focus_top_button_id == 3);
                lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EMPIRE_DRAW_TRADE_ROUTE,
                    data.panel.x_max - 420, data.y_max - 126, 120, FONT_NORMAL_GREEN);
            }
            
            break;
        }
    }
    if (scenario.empire.id == SCENARIO_CUSTOM_EMPIRE) {
        image_buttons_draw(0, 0, edit_city_name_button, 1);
    }
}

static void draw_object_info(void)
{
    empire_object *obj = empire_object_get(empire_selected_object() - 1);
    if (!obj) {
        return;
    }
    if (obj->image_id == -1) {
        return;
    }
    if (obj->type == EMPIRE_OBJECT_ROMAN_ARMY || obj->type == EMPIRE_OBJECT_ENEMY_ARMY) {
        if (!data.show_battle_objects) {
            return;
        }
    }
    uint8_t coords_string[16];
    snprintf((char *)coords_string, 16, "%i, %i", obj->x, obj->y);
    data.object_coords_x = data.panel.x_min + 28 +
        text_draw(coords_string, data.panel.x_min + 28, data.y_max - 85, FONT_NORMAL_GREEN, COLOR_MASK_NONE);
    button_border_draw(data.panel.x_max - 140, data.y_max - 133, 120, 24, data.focus_top_button_id == 1);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EMPIRE_DELETE_OBJECT,
        data.panel.x_max - 140, data.y_max - 126, 120, FONT_NORMAL_GREEN);
    button_border_draw(data.panel.x_max - 280, data.y_max - 133, 120, 24, data.focus_top_button_id == 2);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EMPIRE_MOVE_OBJECT,
        data.panel.x_max - 280, data.y_max - 126, 120, FONT_NORMAL_GREEN);
    if (obj->type == EMPIRE_OBJECT_BORDER_EDGE) {
        int width = lang_text_draw(CUSTOM_TRANSLATION, TR_EMPIRE_EDGE_INDEX, data.panel.x_min + 28,
            data.y_max - 125, FONT_NORMAL_GREEN);
        text_draw_number(obj->order_index, '\0', NULL, data.panel.x_min + 28 + width, data.y_max - 125,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
    }
    if (obj->type == EMPIRE_OBJECT_TRADE_WAYPOINT) {
        int width = lang_text_draw(CUSTOM_TRANSLATION, TR_EMPIRE_WAYPOINT_INDEX, data.panel.x_min + 28,
            data.y_max - 125, FONT_NORMAL_GREEN);
        width += text_draw_number(obj->order_index, '\0', NULL, data.panel.x_min + 28 + width, data.y_max - 125,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
        width += lang_text_draw(CUSTOM_TRANSLATION, TR_EMPIRE_ROUTE_PARENT, data.panel.x_min + 28 + width,
            data.y_max - 125, FONT_NORMAL_GREEN);
        empire_city *route_city = empire_city_get(empire_city_get_for_object(obj->parent_object_id - 1));
        text_draw(empire_city_get_name(route_city), data.panel.x_min + 28 + width, data.y_max - 125,
            FONT_NORMAL_GREEN, COLOR_MASK_NONE);
    }
    if (obj->type == EMPIRE_OBJECT_ORNAMENT) {
        int ornament_number = obj->image_id < 0 ? ORIGINAL_ORNAMENTS - 2 - obj->image_id : obj->image_id - BASE_ORNAMENT_IMAGE_ID;
        translation_key key = TR_EMPIRE_ORNAMENT_STONEHENGE + ornament_number;
        lang_text_draw(CUSTOM_TRANSLATION, key, data.panel.x_min + 28, data.y_max - 125, FONT_NORMAL_GREEN);
    }
}

static void draw_panel_buttons(const empire_city *city)
{
    int x_offset;
    if (scenario_empire_id() != SCENARIO_CUSTOM_EMPIRE) {
        arrow_buttons_draw(data.panel.x_min + 20, data.y_max - 100, arrow_buttons_empire, 2);
        x_offset = 104;
    } else {
        x_offset = 24;
    }

    if (city) {
        draw_city_info(city);
    } else {
        lang_text_draw_centered(150, scenario_empire_id(),
            data.panel.x_min, data.y_max - 85, data.x_max - data.x_min, FONT_NORMAL_GREEN);
    }
    lang_text_draw(151, scenario_empire_id(), data.panel.x_min + 220, data.y_max - 45, FONT_NORMAL_GREEN);
    
    if (empire_selected_object() && scenario.empire.id == SCENARIO_CUSTOM_EMPIRE) {
        draw_object_info();
    }

    button_border_draw(data.panel.x_min + x_offset, data.y_max - 52, 100, 24, data.focus_button_id == 1);
    lang_text_draw_centered(44, 7, data.panel.x_min + x_offset, data.y_max - 45, 100, FONT_NORMAL_GREEN);

    if (scenario.empire.id == SCENARIO_CUSTOM_EMPIRE) {
        button_border_draw(data.panel.x_min + 144, data.y_max - 52, 150, 24, data.focus_button_id == 2);
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_TOGGLE_INVASIONS,
            data.panel.x_min + 144, data.y_max - 45, 150, FONT_NORMAL_GREEN);
        
        button_border_draw(data.panel.x_min + 314, data.y_max - 52, 150, 24, data.focus_button_id == 3);
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EMPIRE_TOGGLE_EDGES,
            data.panel.x_min + 314, data.y_max - 45, 150, FONT_NORMAL_GREEN);
        
        int width = lang_text_get_width(CUSTOM_TRANSLATION, data.button_is_preview ?
            TR_EDITOR_EMPIRE_TOOL : TR_EDITOR_CURRENT_ICON, FONT_NORMAL_GREEN);
        if (data.panel.x_min + 484 + 150 + width + 96 < data.panel.x_max) {
            generic_buttons[3].parameter1 = 0;
            button_border_draw(data.panel.x_min + 484, data.y_max - 52, 150, 24, data.focus_button_id == 4);
            lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_REFRESH_EMPIRE,
                data.panel.x_min + 484, data.y_max - 45, 150, FONT_NORMAL_GREEN);
        } else {
            generic_buttons[3].parameter1 = 1;
        }
        
        if (data.panel.x_min + 654 + 150 + width + 96 < data.panel.x_max) {
            generic_buttons[4].parameter1 = 0;
            button_border_draw(data.panel.x_min + 654, data.y_max - 52, 150, 24, data.focus_button_id == 5);
            lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_EMPIRE_EXPORT,
                data.panel.x_min + 654, data.y_max - 45, 150, FONT_NORMAL_GREEN);
        } else {
            generic_buttons[4].parameter1 = 1;
        }
        if (data.panel.x_min + 824 + 150 + width + 96 < data.panel.x_max) {
            generic_buttons[5].parameter1 = 0;
            button_border_draw(data.panel.x_min + 824, data.y_max - 52, 150, 24, data.focus_button_id == 6);
            lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_EMPIRE_IMPORT,
                data.panel.x_min + 824, data.y_max - 45, 150, FONT_NORMAL_GREEN);
        } else {
            generic_buttons[5].parameter1 = 1;
        }

        if (data.panel.x_min + 564 + width + 16 < data.panel.x_max) {
            generic_buttons[6].parameter1 = 0;
            generic_buttons[6].x = data.panel.x_max - 96 - width - 16 - 20;
            generic_buttons[6].width = width + 16;
            button_border_draw(generic_buttons[6].x + 20, data.y_max - 92, width + 16, 24, data.focus_button_id == 7);
            lang_text_draw_centered(CUSTOM_TRANSLATION, data.button_is_preview ? TR_EDITOR_EMPIRE_TOOL : TR_EDITOR_CURRENT_ICON,
                generic_buttons[6].x + 20, data.y_max - 85, width + 16, FONT_NORMAL_GREEN);
            empire_city_icon_type icon = empire_selected_object() ? empire_object_get(empire_selected_object() - 1)->empire_city_icon : 0;
            lang_text_draw_centered(CUSTOM_TRANSLATION, get_preview_translation_key(data.button_is_preview ? empire_editor_get_tool() : icon),
                generic_buttons[6].x + 20, data.y_max - 45, width + 16, FONT_NORMAL_GREEN);
        } else {
            generic_buttons[6].parameter1 = 1;
        }

        button_border_draw(data.panel.x_max - 92, data.y_max - 100, 72, 72, data.preview_button_focused);
    }
}

static void draw_foreground(void)
{
    draw_map();

    const empire_city *city = 0;
    int selected_object = empire_selected_object();
    if (selected_object) {
        const empire_object *object = empire_object_get(selected_object - 1);
        if (object->type == EMPIRE_OBJECT_CITY) {
            data.selected_city = empire_city_get_for_object(object->id);
            city = empire_city_get(data.selected_city);
        }
    }
    draw_panel_buttons(city);
}

static void determine_selected_object(const mouse *m)
{
    if (!m->left.went_up || data.finished_scroll || is_outside_map(m->x, m->y)) {
        data.finished_scroll = 0;
        return;
    }
    empire_select_object(m->x - data.x_min - 16, m->y - data.y_min - 16);
    window_invalidate();
}

static void refresh_empire(void)
{
    if (scenario.empire.id != SCENARIO_CUSTOM_EMPIRE) {
        return;
    }
    const char *filename = dir_get_file_at_location(scenario.empire.custom_name, PATH_LOCATION_EDITOR_CUSTOM_EMPIRES);
    if (!filename) {
        return;
    }
    empire_xml_parse_file(filename);
    window_empire_collect_trade_edges();
    window_invalidate();
}

static void set_quota(int value)
{
    trade_route_set_limit(empire_city_get(data.selected_city)->route_id, data.selected_resource, value, data.add_to_buying);
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    pixel_offset position;
    if (scroll_get_delta(m, &position, SCROLL_TYPE_EMPIRE)) {
        empire_scroll_map(position.x, position.y);
    }
    if (h->toggle_editor_battle_info) {
        data.show_battle_objects = !data.show_battle_objects;
    } else if (h->f5_pressed) {
        refresh_empire();
    }
    if (m->is_touch) {
        const touch *t = touch_get_earliest();
        if (!is_outside_map(t->current_point.x, t->current_point.y)) {
            if (t->has_started) {
                data.is_scrolling = 1;
                scroll_drag_start(1);
            }
        }
        if (t->has_ended) {
            data.is_scrolling = 0;
            data.finished_scroll = !touch_was_click(t);
            scroll_drag_end();
        }
    }
    data.focus_button_id = 0;
    int x_offset = scenario.empire.id == SCENARIO_CUSTOM_EMPIRE ? 20 : 100;
    if (scenario.empire.id != SCENARIO_CUSTOM_EMPIRE &&
        arrow_buttons_handle_mouse(m, data.panel.x_min + 20, data.y_max - 100, arrow_buttons_empire, 2, 0)) {
        return;
    }
    if (generic_buttons_handle_mouse(m, data.panel.x_min + x_offset, data.y_max - 100, generic_buttons,
        scenario.empire.id == SCENARIO_CUSTOM_EMPIRE ? 7 : 1, &data.focus_button_id)) {
        if (!generic_buttons[data.focus_button_id - 1].parameter1) {
            return;
        }
    }
    if (scenario.empire.id == SCENARIO_CUSTOM_EMPIRE && 
        generic_buttons_handle_mouse(m, data.panel.x_max - 420, data.y_max - 133, top_buttons, 3, &data.focus_top_button_id)) {
        if (!top_buttons[data.focus_top_button_id - 1].parameter1) {
            return;
        }
    }
    if (scenario.empire.id == SCENARIO_CUSTOM_EMPIRE &&
        generic_buttons_handle_mouse(m, data.panel.x_max - 92, data.y_max - 100,
            preview_button, 1, &data.preview_button_focused)) {
        return;
    }
    if (scenario.empire.id == SCENARIO_CUSTOM_EMPIRE &&
        image_buttons_handle_mouse(m, 0, 0, edit_city_name_button, 1, NULL)) {
        return;
    }
    if (scenario.empire.id == SCENARIO_CUSTOM_EMPIRE &&
        image_buttons_handle_mouse(m, 0, 0, add_resource_buttons, NUM_PLUS_BUTTONS, NULL)) {
        return;
    }
    for (int i = 0; i < 2; i++) {
        for (resource_type r = RESOURCE_NONE; r < RESOURCE_MAX; r++) {
            resource_button *btn = i ? &buy_buttons[r] : &sell_buttons[r];
            if (m->x >= btn->x && m->x < btn->x + RESOURCE_ICON_WIDTH &&
                m->y >= btn->y && m->y < btn->y + RESOURCE_ICON_HEIGHT) {
                btn->highlighted = 1;
                if (m->left.went_up && scenario.empire.id == SCENARIO_CUSTOM_EMPIRE && 
                    empire_city_get(data.selected_city)->type != EMPIRE_CITY_OURS) {
                    data.add_to_buying = i;
                    data.selected_resource = r;
                    window_numeric_input_bound_show((data.x_min + data.x_max) / 2 - 4 * BLOCK_SIZE - screen_dialog_offset_x(),
                        ((data.y_min + data.y_max) - 15 * BLOCK_SIZE) / 2 - screen_dialog_offset_y(), NULL, 5, 0, 99999, set_quota);
                    window_request_refresh();
                    return;
                }
                if (m->right.went_up && scenario.empire.id == SCENARIO_CUSTOM_EMPIRE) {
                    empire_city *city = empire_city_get(data.selected_city);
                    if (i) {
                        empire_city_change_buying_of_resource(city, r, 0);
                    } else {
                        empire_city_change_selling_of_resource(city, r, 0);
                    }
                    window_request_refresh();
                    return;
                }
            } else {
                btn->highlighted = 0;
            }
        }
    }
    
    if (!is_outside_map(m->x, m->y) && scenario.empire.id == SCENARIO_CUSTOM_EMPIRE) {
        if (empire_editor_handle_placement(m, h)) {
            return;
        }
    }
    determine_selected_object(m);
    int selected_object = empire_selected_object();
    if (selected_object) {
        if (empire_object_get(selected_object - 1)->type == EMPIRE_OBJECT_CITY) {
            data.selected_city = empire_city_get_for_object(selected_object - 1);
        }
        if (input_go_back_requested(m, h)) {
            empire_editor_move_object_stopp();
            empire_editor_clear_trade_point_parent();
            empire_clear_selected_object();
            window_invalidate();
        }
    } else {
        if (is_outside_map(m->x, m->y)) {
            return;
        }
        if (m->right.went_down) {
            scroll_drag_start(0);
        }
        if (m->right.went_up) {
            int has_scrolled = scroll_drag_end();
            if (!has_scrolled && input_go_back_requested(m, h)) {
                empire_editor_move_object_stopp();
                if (data.coordinates.active) {
                    data.coordinates.active = 0;
                } else {
                    window_editor_map_show();
                }
            }
        }
        if (scenario.empire.id == SCENARIO_CUSTOM_EMPIRE && m->left.is_down && !is_outside_map(m->x, m->y)) {
            data.coordinates.active = 1;
            data.coordinates.x = m->x - data.x_draw_offset;
            data.coordinates.y = m->y - data.y_draw_offset;
            empire_restore_coordinates(&data.coordinates.x, &data.coordinates.y);
        }
    }
}

static void button_change_empire(int is_down, int param2)
{
    scenario_editor_change_empire(is_down ? -1 : 1);
    empire_load_editor(scenario_empire_id(), map_viewport_width(), map_viewport_height());
    window_request_refresh();
}

static void button_ok(const generic_button *button)
{
    window_editor_map_show();
}

static void button_toggle_invasions(const generic_button *button)
{
    data.show_battle_objects = !data.show_battle_objects;
}

static void button_toggle_waypoints(const generic_button *button)
{
    data.show_edges = !data.show_edges;
}

static void button_cycle_preview(const generic_button *button)
{
    if (data.button_is_preview) {
        empire_editor_change_tool(1);
        empire_editor_move_object_stopp();
        empire_editor_clear_trade_point_parent();
    } else if (empire_selected_object()) {
        empire_object *obj = empire_object_get(empire_selected_object() - 1);
        full_empire_object *full = empire_object_get_full(empire_selected_object() - 1);
        obj->empire_city_icon++;
        full->empire_city_icon++;
        if (obj->empire_city_icon > EMPIRE_CITY_ICON_TOWER) {
            obj->empire_city_icon = full->empire_city_icon = EMPIRE_CITY_ICON_CONSTRUCTION;
        }
    }
    window_request_refresh();
}

static void button_recycle_preview(const generic_button *button)
{
    if (data.button_is_preview) {
        empire_editor_change_tool(-1);
        empire_editor_move_object_stopp();
        empire_editor_clear_trade_point_parent();
    } else if (empire_selected_object()) {
        empire_object *obj = empire_object_get(empire_selected_object() - 1);
        full_empire_object *full = empire_object_get_full(empire_selected_object() - 1);
        obj->empire_city_icon--;
        full->empire_city_icon--;
        if (obj->empire_city_icon < EMPIRE_CITY_ICON_CONSTRUCTION) {
            obj->empire_city_icon = full->empire_city_icon = EMPIRE_CITY_ICON_TOWER;
        }
    }
    window_request_refresh();
}

static void button_icon_preview(const generic_button *button)
{
    data.button_is_preview = 1 - data.button_is_preview;
    window_request_refresh();
}

static void add_resource(int value)
{
    resource_type resource = data.available_resources[value];
    empire_city *city = empire_city_get(data.selected_city);
    if (data.add_to_buying) {
        empire_city_change_buying_of_resource(city, resource, DEFAULT_TRADE_QUOTA);
    } else {
        empire_city_change_selling_of_resource(city, resource, DEFAULT_TRADE_QUOTA);
    }
}

static void button_add_resource(int param1, int param2)
{
    data.add_to_buying = param1;
    static const uint8_t *resource_texts[RESOURCE_MAX];
    static int total_resources = 0;
    if (!total_resources) {
        for (resource_type resource = RESOURCE_NONE; resource < RESOURCE_MAX; resource++) {
            if (!resource_is_storable(resource)) {
                continue;
            }
            resource_texts[total_resources] = resource_get_data(resource)->text;
            data.available_resources[total_resources] = resource;
            total_resources++;
        }
    }
    window_select_list_show_text((data.x_min + data.x_max) / 2 - 100, ((data.y_min + data.y_max) -
        (20 * total_resources + 24)) / 2, NULL, resource_texts, total_resources, add_resource);
}

static void set_city_name(const uint8_t *name)
{
    full_empire_object *city_obj = empire_object_get_full(empire_city_get(data.selected_city)->empire_object_id);
    string_copy(name, city_obj->city_custom_name, 50);
}

static void button_edit_city_name(int param1, int param2)
{
    // 49 because city_custom_name is declared as uint8_t [50]
    window_text_input_show(string_from_ascii("Edit city name"), NULL, NULL, 49, set_city_name);
}

static void button_delete_object(const generic_button *button)
{
    empire_editor_delete_object(empire_selected_object() - 1);
    window_request_refresh();
}

static void button_move_object(const generic_button *button)
{
    empire_editor_move_object_start(empire_selected_object() - 1);
}

static void remove_trade_waypoints(const empire_object *obj)
{
    if (obj->parent_object_id == empire_editor_get_trade_point_parent()) {
        empire_object_remove(obj->id);
    }
}

static void button_draw_route(const generic_button *button)
{
    if (button->parameter1) {
        return;
    }
    int obj_id = empire_selected_object() - 1;
    if (obj_id < 0) {
        return;
    }
    empire_object *route_obj = empire_object_get(obj_id + 1);
    int is_sea = route_obj->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE;
    empire_editor_set_trade_point_parent(obj_id + 1);
    empire_editor_set_tool(is_sea ? EMPIRE_TOOL_SEA_ROUTE : EMPIRE_TOOL_LAND_ROUTE);
    empire_object_foreach(remove_trade_waypoints);
    window_empire_collect_trade_edges();
    empire_object_set_trade_route_coords(empire_object_get_our_city());
    window_request_refresh();
}

static void button_refresh(const generic_button *button)
{
    if (button->parameter1) {
        return;
    }
    refresh_empire();
}

static void button_export_xml(const generic_button *button)
{
    if (button->parameter1) {
        return;
    }
    window_file_dialog_show(FILE_TYPE_EMPIRE, FILE_DIALOG_SAVE);
}

static void button_import_xml(const generic_button *button)
{
    if (button->parameter1) {
        return;
    }
    resource_set_mapping(RESOURCE_CURRENT_VERSION);
    window_file_dialog_show(FILE_TYPE_EMPIRE, FILE_DIALOG_LOAD);
}

void window_editor_empire_show(void)
{
    window_type window = {
        WINDOW_EDITOR_EMPIRE,
        draw_background,
        draw_foreground,
        handle_input
    };
    init();
    window_show(&window);
}
