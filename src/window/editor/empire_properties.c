#include "empire_properties.h"

#include "core/hotkey_config.h"
#include "core/string.h"
#include "empire/editor.h"
#include "empire/empire.h"
#include "empire/object.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
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

static struct {
    unsigned int focus_button_id;
} data;

static void button_select_image(const generic_button *button);
static void button_default_image(const generic_button *button);
static void button_border_density(const generic_button *button);
static void button_change_invasion_path(const generic_button *button);
static void button_empire_settings(const generic_button *button);
static void button_hotkeys(const generic_button *button);

static generic_button generic_buttons[] = {
    {16, 16, 200, 30, button_select_image},
    {16, 56, 200, 30, button_default_image},
    {16, 106, 200, 30, button_border_density},
    {16, 146, 200, 30, button_change_invasion_path},
    {16, 196, 200, 30, button_empire_settings},
    {16, 236, 200, 30, button_hotkeys},
};
#define NUM_GENERIC_BUTTONS sizeof(generic_buttons) / sizeof(generic_button)

static void init(void)
{
    
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
        button_border_draw(generic_buttons[i].x, generic_buttons[i].y, generic_buttons[i].width,
            generic_buttons[i].height, data.focus_button_id == i + 1);
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_EMPIRE_PROPERTIES_SELECT_IAMGE + i, generic_buttons[i].x,
            generic_buttons[i].y + 8, generic_buttons[i].width, FONT_NORMAL_BLACK);
    }

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    if (generic_buttons_handle_mouse(m_dialog, 0, 0, generic_buttons, NUM_GENERIC_BUTTONS, &data.focus_button_id)) {
        return;
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
    string_copy(string_from_ascii("\0"), (uint8_t *)scenario.empire.custom_name, 300);
    resource_set_mapping(RESOURCE_CURRENT_VERSION);
    empire_clear();
    empire_object_clear();
    empire_object_init_cities(SCENARIO_CUSTOM_EMPIRE);

    window_editor_empire_show();
}

static void set_density(int value)
{
    empire_object *border = (empire_object *)empire_object_get_border();
    if (!border) {
        return;
    }
    border->width = value;
}

static void button_border_density(const generic_button *button)
{
    window_numeric_input_bound_show(0, 0, button, 3, 4, 300, set_density);
}

static void set_path(int value)
{
    empire_editor_set_current_invasion_path(value);
}

static void button_change_invasion_path(const generic_button *button)
{
    window_numeric_input_bound_show(0, 0, button, 2, 1, 50, set_path);
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
