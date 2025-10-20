#include "sidebar.h"

#include "building/menu.h"
#include "city/message.h"
#include "city/view.h"
#include "city/warning.h"
#include "core/config.h"
#include "game/campaign.h"
#include "game/orientation.h"
#include "game/state.h"
#include "game/undo.h"
#include "graphics/image.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "scenario/property.h"
#include "translation/translation.h"
#include "widget/city.h"
#include "widget/minimap.h"
#include "widget/sidebar/common.h"
#include "widget/sidebar/extra.h"
#include "window/advisors.h"
#include "window/build_menu.h"
#include "window/city.h"

#include "widget/sidebar/city.h"
#include "window/empire.h"
#include "window/message_dialog.h"
#include "window/message_list.h"
#include "window/overlay_menu.h"

#define MINIMAP_Y_OFFSET 59
#define SIDEBAR_EXPANDED_WIDTH 162

static void button_overlay_click(int param1, int param2);
static void button_collapse(int param1, int param2);
static void button_build(int submenu, int param2);
static void button_undo(int param1, int param2);
static void button_messages(int param1, int param2);
static void button_help(int param1, int param2);
static void button_go_to_problem(int param1, int param2);
static void button_advisors(int param1, int param2);
static void button_empire(int param1, int param2);
static void button_toggle_grid(int param1, int param2);
static void button_rotate_north(int param1, int param2);
static void button_rotate(int clockwise, int param2);

static image_button buttons_overlays_collapse_sidebar[] = {
    {127, 5, 31, 20, IB_NORMAL, 90, 0, button_collapse, button_none, 0, 0, 1},
    {4, 3, 117, 31, IB_NORMAL, 93, 0, button_overlay_click, button_help, 0, MESSAGE_DIALOG_OVERLAYS, 1}
};

static image_button buttons_build_expanded[] = {
    {13, 277, 39, 26, IB_NORMAL, GROUP_SIDEBAR_BUTTONS, 0, button_build, button_none, BUILD_MENU_VACANT_HOUSE, 0, 1},
    {63, 277, 39, 26, IB_NORMAL, GROUP_SIDEBAR_BUTTONS, 8, button_build, button_none, BUILD_MENU_CLEAR_LAND, 0, 1},
    {113, 277, 39, 26, IB_NORMAL, GROUP_SIDEBAR_BUTTONS, 12, button_build, button_none, BUILD_MENU_ROAD, 0, 1},
    {13, 313, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 4, button_build, button_none, BUILD_MENU_WATER, 0, 1},
    {63, 313, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 40, button_build, button_none, BUILD_MENU_HEALTH, 0, 1, "UI", "Asclepius Button"},
    {113, 313, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 28, button_build, button_none, BUILD_MENU_TEMPLES, 0, 1},
    {13, 349, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 24, button_build, button_none, BUILD_MENU_EDUCATION, 0, 1},
    {63, 349, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 20, button_build, button_none, BUILD_MENU_ENTERTAINMENT, 0, 1},
    {113, 349, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 16, button_build, button_none, BUILD_MENU_ADMINISTRATION, 0, 1},
    {13, 385, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 44, button_build, button_none, BUILD_MENU_ENGINEERING, 0, 1},
    {63, 385, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 36, button_build, button_none, BUILD_MENU_SECURITY, 0, 1},
    {113, 385, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 32, button_build, button_none, BUILD_MENU_INDUSTRY, 0, 1},
    {13, 421, 39, 26, IB_NORMAL, GROUP_SIDEBAR_BUTTONS, 48, button_undo, button_none, 0, 0, 1},
    {63, 421, 39, 26, IB_NORMAL, GROUP_MESSAGE_ICON, 18, button_messages, button_help, 0, MESSAGE_DIALOG_MESSAGES, 1},
    {113, 421, 39, 26, IB_BUILD, GROUP_MESSAGE_ICON, 22, button_go_to_problem, button_none, 0, 0, 1},
};

static image_button buttons_top_expanded[] = {
    {7, 155, 71, 23, IB_NORMAL, GROUP_SIDEBAR_ADVISORS_EMPIRE, 0, button_advisors, button_none, 0, 0, 1},
    {84, 155, 71, 23, IB_NORMAL, GROUP_SIDEBAR_ADVISORS_EMPIRE, 3, button_empire, button_help, 0, MESSAGE_DIALOG_EMPIRE_MAP, 1},
    {7, 184, 33, 22, IB_NORMAL, 0, 0, button_toggle_grid, button_none, 0, 0, 1, "UI", "Toggle Grid Button" },
    {46, 184, 33, 22, IB_NORMAL, GROUP_SIDEBAR_BRIEFING_ROTATE_BUTTONS, 3, button_rotate_north, button_none, 0, 0, 1},
    {84, 184, 33, 22, IB_NORMAL, GROUP_SIDEBAR_BRIEFING_ROTATE_BUTTONS, 6, button_rotate, button_none, 0, 0, 1},
    {123, 184, 33, 22, IB_NORMAL, GROUP_SIDEBAR_BRIEFING_ROTATE_BUTTONS, 9, button_rotate, button_none, 1, 0, 1},
};

static struct {
    unsigned int focus_button_for_tooltip;
} data;

static void draw_overlay_text(const int x_offset)
{
    if (game_state_overlay()) {
        const uint8_t *text = get_current_overlay_text();
        text_draw_centered(text, x_offset, 32, 117, FONT_NORMAL_GREEN, 0);
    } else {
        lang_text_draw_centered(6, 4, x_offset, 32, 117, FONT_NORMAL_GREEN);
    }
}

static void draw_sidebar_remainder(const int x_offset)
{
    const int width = SIDEBAR_EXPANDED_WIDTH;

    const int available_height = sidebar_common_get_height() - SIDEBAR_MAIN_SECTION_HEIGHT;
    const int extra_height = sidebar_extra_draw_background(x_offset, SIDEBAR_FILLER_Y_OFFSET,
        width, available_height, 0, SIDEBAR_EXTRA_DISPLAY_ALL);

    sidebar_extra_draw_foreground();

    const int relief_y_offset = SIDEBAR_FILLER_Y_OFFSET + extra_height;
    sidebar_common_draw_relief(x_offset, relief_y_offset, GROUP_SIDE_PANEL, 0, 0);
}

static void draw_number_of_messages(const int x_offset)
{
    const int messages = city_message_count();
    const int show_messages = game_campaign_is_original() || messages > 0 || scenario_intro_message();
    buttons_build_expanded[13].enabled = show_messages;
    buttons_build_expanded[14].enabled = city_message_problem_area_count();
    if (show_messages) {
        const int width = text_get_number_width(messages, '@', "", FONT_SMALL_PLAIN);
        text_draw_number(messages, '@', "", (x_offset + 100) - width, 452, FONT_SMALL_PLAIN, COLOR_BLACK); //top
        text_draw_number(messages, '@', "", (x_offset + 100) - width, 454, FONT_SMALL_PLAIN, COLOR_BLACK); //bottom
        text_draw_number(messages, '@', "", (x_offset + 99) - width, 453, FONT_SMALL_PLAIN, COLOR_BLACK); //left
        text_draw_number(messages, '@', "", (x_offset + 101) - width, 453, FONT_SMALL_PLAIN, COLOR_BLACK); //right
        text_draw_number(messages, '@', "", (x_offset + 100) - width, 453, FONT_SMALL_PLAIN, COLOR_WHITE);
    }
}

static void draw_buttons_expanded(const int x_offset)
{
    buttons_build_expanded[12].enabled = game_can_undo();
    image_buttons_draw(x_offset, 24, buttons_overlays_collapse_sidebar, 2);
    image_buttons_draw(x_offset, 24, buttons_build_expanded, 15);
    image_buttons_draw(x_offset, 24, buttons_top_expanded, 6);
}

static void enable_building_buttons(void)
{
    for (int i = 0; i < 12; i++) {
        buttons_build_expanded[i].enabled = 1;
        if (building_menu_count_items(buttons_build_expanded[i].parameter1) <= 0) {
            buttons_build_expanded[i].enabled = 0;
        }
    }
}

int expanded_sidebar_width(void)
{
    return SIDEBAR_EXPANDED_WIDTH;
}

int calculate_x_offset_sidebar_expanded(void)
{
    return screen_width() - SIDEBAR_EXPANDED_WIDTH;
}

void draw_expanded_sidebar_background(const int x_offset)
{
    image_draw(image_group(GROUP_SIDE_PANEL) + 1, x_offset, 24, COLOR_MASK_NONE, SCALE_NONE);
    draw_buttons_expanded(x_offset);
    draw_overlay_text(x_offset + 4);
    draw_number_of_messages(x_offset);
    image_draw(window_build_menu_image(), x_offset + 6, 239, COLOR_MASK_NONE, SCALE_NONE);
    widget_minimap_update(0);
    widget_minimap_draw_decorated(x_offset + 8, MINIMAP_Y_OFFSET, MINIMAP_WIDTH, MINIMAP_HEIGHT);

    draw_sidebar_remainder(x_offset);
}

void draw_expanded_sidebar_foreground(void)
{
    if (building_menu_has_changed()) {
        enable_building_buttons();
    }

    const int x_offset = calculate_x_offset_sidebar_expanded();
    draw_buttons_expanded(x_offset);
    draw_overlay_text(x_offset + 4);
    widget_minimap_draw_decorated(x_offset + 8, MINIMAP_Y_OFFSET, MINIMAP_WIDTH, MINIMAP_HEIGHT);
    draw_number_of_messages(x_offset);

    sidebar_extra_draw_foreground();
}

int handle_expanded_sidebar_mouse(const mouse *m)
{
    if (widget_city_has_input()) {
        return 0;
    }
    int handled = 0;
    unsigned int button_id;
    data.focus_button_for_tooltip = 0;

    if (widget_minimap_handle_mouse(m)) {
        return 1;
    }

    const int x_offset = calculate_x_offset_sidebar_expanded();
    handled |= image_buttons_handle_mouse(m, x_offset, 24, buttons_overlays_collapse_sidebar, 2, &button_id);
    if (button_id) {
        data.focus_button_for_tooltip = button_id + 9;
    }
    handled |= image_buttons_handle_mouse(m, x_offset, 24, buttons_build_expanded, 15, &button_id);
    if (button_id) {
        data.focus_button_for_tooltip = button_id + 19;
    }
    handled |= image_buttons_handle_mouse(m, x_offset, 24, buttons_top_expanded, 6, &button_id);
    if (button_id) {
        data.focus_button_for_tooltip = button_id + 39;
    }
    handled |= sidebar_extra_handle_mouse(m);

    return handled;
}

int handle_expanded_sidebar_mouse_build_menu(const mouse *m)
{
    return image_buttons_handle_mouse(m,
        calculate_x_offset_sidebar_expanded(), 24, buttons_build_expanded, 15, 0);
}

unsigned int get_expanded_sidebar_tooltip_text(tooltip_context *c)
{
    if (data.focus_button_for_tooltip) {
        if (data.focus_button_for_tooltip == 42) {
            c->translation_key = TR_TOGGLE_GRID;
            return 0;
        }
        return data.focus_button_for_tooltip;
    }
    return sidebar_extra_get_tooltip(c);
}

static void button_overlay_click(int param1, int param2)
{
    window_overlay_menu_show();
}

static void button_collapse(int param1, int param2)
{
    sidebar_next();
}

static void button_build(const int submenu, int param2)
{
    window_build_menu_show(submenu);
}

static void button_undo(int param1, int param2)
{
    window_build_menu_hide();
    game_undo_perform();
    window_invalidate();
}

static void button_messages(int param1, int param2)
{
    window_build_menu_hide();
    window_message_list_show();
}

static void button_help(int param1, const int param2)
{
    window_build_menu_hide();
    window_message_dialog_show(param2, window_city_draw_all);
}

static void button_go_to_problem(int param1, int param2)
{
    window_build_menu_hide();
    const int grid_offset = city_message_next_problem_area_grid_offset();
    if (grid_offset) {
        city_view_go_to_grid_offset(grid_offset);
        window_city_show();
    } else {
        window_invalidate();
    }
}

static void button_advisors(int param1, int param2)
{
    window_advisors_show_checked();
}

static void button_empire(int param1, int param2)
{
    window_empire_show_checked();
}

static void button_toggle_grid(int param1, int param2)
{
    config_set(CONFIG_UI_SHOW_GRID, config_get(CONFIG_UI_SHOW_GRID) ^ 1);
}

static void button_rotate_north(int param1, int param2)
{
    game_orientation_rotate_north();
    window_invalidate();
}

static void button_rotate(const int clockwise, int param2)
{
    if (clockwise) {
        game_orientation_rotate_right();
    } else {
        game_orientation_rotate_left();
    }
    window_invalidate();
}
