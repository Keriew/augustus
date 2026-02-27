#include "empire_properties.h"

#include "assets/assets.h"
#include "core/hotkey_config.h"
#include "core/image.h"
#include "core/image_group.h"
#include "core/image_group_editor.h"
#include "core/string.h"
#include "editor/editor.h"
#include "empire/editor.h"
#include "empire/empire.h"
#include "empire/object.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/list_box.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/hotkey.h"
#include "input/input.h"
#include "input/mouse.h"
#include "scenario/empire.h"
#include "scenario/data.h"
#include "translation/translation.h"
#include "window/hotkey_config.h"
#include "window/config.h"
#include "window/editor/empire.h"
#include "window/file_dialog.h"
#include "window/numeric_input.h"
#include "window/select_list.h"

static struct {
    unsigned int focus_button_id;
    int listed_ornaments[TOTAL_ORNAMENTS];
} data;

static void button_select_image(const generic_button *button);
static void button_default_image(const generic_button *button);
static void button_border_density(const generic_button *button);
static void button_change_invasion_path(const generic_button *button);
static void button_add_ornament(const generic_button *button);
static void button_add_all_ornaments(const generic_button *button);
static void button_toggle_ireland(const generic_button *button);
static void button_add_all_cities(const generic_button *button);
static void button_empire_settings(const generic_button *button);
static void button_hotkeys(const generic_button *button);

static generic_button generic_buttons[] = {
    {16, 16, 200, 30, button_select_image},
    {16, 56, 200, 30, button_default_image},
    {16, 106, 200, 30, button_border_density},
    {16, 146, 200, 30, button_change_invasion_path},
    {16, 196, 200, 30, button_add_ornament},
    {16, 236, 200, 30, button_add_all_ornaments},
    {16, 276, 200, 30, button_toggle_ireland},
    {16, 316, 200, 30, button_add_all_cities},
    {16, 366, 200, 30, button_empire_settings},
    {16, 406, 200, 30, button_hotkeys},
};
#define NUM_GENERIC_BUTTONS (sizeof(generic_buttons) / sizeof(generic_button))

typedef struct {
    const char name[50];
    int x;
    int y;
} default_city;

static void default_cities_list_box_draw_item(const list_box_item *item);
static void default_cities_list_box_on_select(unsigned int index, int is_double_click);

static const default_city default_cities[] = {
    {"Roma", 752, 504},
    {"Tarentum", 905, 558},
    {"Brundisium", 945, 543},
    {"Syracusae", 835, 733},
    {"Carthago Nova", 225, 728},
    {"Mediolanum", 614, 310},
    {"Tarraco", 382, 494},
    {"Valentia", 277, 617},
    {"Toletum", 114, 603},
    {"Tingis", 115, 794},
    {"Sinope", 1625, 491},
    {"Tarsus", 1657, 733},
    {"Leptis Magna", 835, 894},
    {"Cyrene", 1128, 883},
    {"Lutetia", 373, 191},
    {"Massilia", 473, 400},
    {"Narbo", 395, 426},
    {"Lugdunum", 473, 284},
    {"Caesarea", 382, 755},
    {"Alexandria", 1508, 929},
    {"Augusta Trevorum", 506, 125},
    {"Argentoratum", 552, 157},
    {"Volubilis", 61, 903},
    {"Lindum", 266, 16},
    {"Byzantium", 1380, 521},
    {"Sarmizegetusa", 1041, 307},
    {"Thamugadi", 579, 857},
    {"Calleva", 210, 77},
    {"Londinium", 322, 67},
    {"Antiocha", 1718, 755},
    {"Heliopolis", 1739, 785},
    {"Damascus", 1742, 815},
    {"Hierosolyma", 1698, 875},
    {"Pergamum", 1366, 613},
    {"Ephesus", 1337, 678},
    {"Miletus", 1353, 712},
    {"Athenae", 1172, 659},
    {"Corinthus", 1133, 681},
    {"Carthago", 656, 735},
    {"Capua", 802, 517}
};
#define NUM_DEFAULT_CITIES sizeof(default_cities) / sizeof(default_city)

static list_box_type default_cities_list_box = {
    240, // x
    16, // y
    23, // width
    28, // height
    24, // item height
    1, // inner panel
    0, // extend to hidden scrollbar
    1, // decorate scrollbar
    default_cities_list_box_draw_item,
    default_cities_list_box_on_select,
    NULL
};

static void init(void)
{
    list_box_init(&default_cities_list_box, NUM_DEFAULT_CITIES);
}

static void default_cities_list_box_draw_item(const list_box_item *item)
{
    font_t font = item->is_selected ? FONT_NORMAL_WHITE : FONT_NORMAL_GREEN;
    const uint8_t display_text[256] = "Roma";
    const default_city *city = &default_cities[item->index];
    color_t color = empire_city_get_at(city->x, city->y, string_from_ascii(city->name)) ? COLOR_FONT_GRAY : COLOR_MASK_NONE;
    snprintf((char *)display_text, 256, "%s: %i, %i", city->name, city->x, city->y);
    text_draw_ellipsized(display_text, item->x + 5, item->y + 4, item->width - 10, font, color);
}

static void add_city(const default_city *city)
{
    if (empire_city_get_at(city->x, city->y, string_from_ascii(city->name))) {
        return;
    }
    full_empire_object *full = empire_object_get_new();
    full->in_use = 1;
    full->obj.type = EMPIRE_OBJECT_CITY;
    full->city_type = EMPIRE_CITY_DISTANT_ROMAN;
    full->obj.x = city->x;
    full->obj.y = city->y;
    full->obj.empire_city_icon = EMPIRE_CITY_ICON_ROMAN_CITY;
    full->empire_city_icon = EMPIRE_CITY_ICON_ROMAN_CITY;
    full->obj.image_id = empire_city_get_icon_image_id(EMPIRE_CITY_ICON_ROMAN_CITY);
    string_copy(string_from_ascii(city->name), full->city_custom_name, 50);
    empire_object_add_to_cities(full);
}

static void default_cities_list_box_on_select(unsigned int index, int is_double_click)
{
    if (!is_double_click) {
        return;
    }
    const default_city *city = &default_cities[index];
    add_city(city);
    
    window_request_refresh();
}

static void draw_background(void)
{
    window_draw_underlying_window();
    graphics_in_dialog();

    outer_panel_draw(0, 0, 40, 30);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    
    for (int i = 0; i < NUM_GENERIC_BUTTONS; i++) {
        font_t font = !(EMPIRE_IS_DEFAULT_IMAGE) && i >= 4 && i <= 7 ? FONT_NORMAL_RED : FONT_NORMAL_BLACK;
        button_border_draw(generic_buttons[i].x, generic_buttons[i].y, generic_buttons[i].width,
            generic_buttons[i].height, data.focus_button_id == i + 1 && (i >= 4 && i <= 7 ? EMPIRE_IS_DEFAULT_IMAGE : 1));
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_EMPIRE_PROPERTIES_SELECT_IAMGE + i, generic_buttons[i].x,
            generic_buttons[i].y + 8, generic_buttons[i].width, font);
    }
    if (EMPIRE_IS_DEFAULT_IMAGE) {
        list_box_request_refresh(&default_cities_list_box);
        list_box_draw(&default_cities_list_box);
    }
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    if (generic_buttons_handle_mouse(m_dialog, 0, 0, generic_buttons, NUM_GENERIC_BUTTONS, &data.focus_button_id)) {
        return;
    }
    if (EMPIRE_IS_DEFAULT_IMAGE) {
        if (list_box_handle_input(&default_cities_list_box, m_dialog, 1)) {
            return;
        }
    }
    if (input_go_back_requested(m, h)) {
        window_editor_empire_show();
    }
}

static void button_select_image(const generic_button *button)
{
     window_file_dialog_show(FILE_TYPE_EMPIRE_IMAGE, FILE_DIALOG_LOAD);
}

static void button_default_image(const generic_button *button)
{
    scenario.empire.id = SCENARIO_CUSTOM_EMPIRE;
    resource_set_mapping(RESOURCE_CURRENT_VERSION);
    empire_clear();
    empire_object_clear();
    empire_object_init_cities(SCENARIO_CUSTOM_EMPIRE);

    window_editor_empire_show();
}

static void button_border_density(const generic_button *button)
{
    window_numeric_input_bound_show(0, 0, button, 3, 4, 300, empire_object_change_border_width);
}

static void set_path(int value)
{
    empire_editor_set_current_invasion_path(value);
}

static void button_change_invasion_path(const generic_button *button)
{
    window_numeric_input_bound_show(0, 0, button, 2, 1, 50, set_path);
}

static void add_ornament(int value)
{
    empire_object_add_ornament(data.listed_ornaments[value]);
}

static void button_add_ornament(const generic_button *button)
{
    if (!(EMPIRE_IS_DEFAULT_IMAGE)) {
        return;
    }
    static const uint8_t *ornament_texts[TOTAL_ORNAMENTS];
    int ornament_count = 0;
    for (int ornament_id = 0; ornament_id < TOTAL_ORNAMENTS; ornament_id++) {
        if (empire_object_get_ornament(empire_object_ornament_image_id_get(ornament_id))) {
            continue;
        }
        ornament_texts[ornament_count] = translation_for(TR_EMPIRE_ORNAMENT_STONEHENGE + ornament_id);
        data.listed_ornaments[ornament_count] = ornament_id;
        ornament_count++;
    }
    if (!ornament_count) {
        return;
    }
    window_select_list_show_text(0, 0, button, ornament_texts, ornament_count, add_ornament);
}

static void button_add_all_ornaments(const generic_button *button)
{
    if (!(EMPIRE_IS_DEFAULT_IMAGE)) {
        return;
    }
    for (int ornament_id = 0; ornament_id < TOTAL_ORNAMENTS; ornament_id++) {
        empire_object_add_ornament(ornament_id);
    }
    window_request_refresh();
}

static void button_toggle_ireland(const generic_button *button)
{
    if (!(EMPIRE_IS_DEFAULT_IMAGE)) {
        return;
    }
    if (empire_object_get_ornament(-1)) {
        empire_object_remove(empire_object_get_ornament(-1));
    } else {
        full_empire_object *obj = empire_object_get_new();
        obj->in_use = 1;
        obj->obj.type = EMPIRE_OBJECT_ORNAMENT;
        obj->obj.image_id = -1;
        const image *img = image_get(assets_lookup_image_id(ASSET_FIRST_ORNAMENT));
        obj->obj.width = img->width;
        obj->obj.height = img->height;
    }
    window_request_refresh();
}

static void button_add_all_cities(const generic_button *button)
{
    if (!(EMPIRE_IS_DEFAULT_IMAGE)) {
        return;
    }
    for (int i = 0; i < NUM_DEFAULT_CITIES; i++) {
        add_city(&default_cities[i]);
    }
    window_request_refresh();
}

static void button_empire_settings(const generic_button *button)
{
    window_config_show(CONFIG_PAGE_UI_CHANGES, CATEGORY_UI_EMPIRE, 0);
}

static void button_hotkeys(const generic_button *button)
{
    window_hotkey_config_show(get_position_for_widget(TR_HOTKEY_HEADER_EDITOR));
}

void window_empire_properties_show(void)
{
    init();
    window_type window = {
        WINDOW_EDITOR_EMPIRE_PROPERTIES,
        draw_background,
        draw_foreground,
        handle_input,
    };
    window_show(&window);
}
