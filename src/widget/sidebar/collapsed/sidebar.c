#include "sidebar.h"

#include "building/menu.h"
#include "city/warning.h"
#include "graphics/image.h"
#include "graphics/image_button.h"
#include "graphics/window.h"
#include "translation/translation.h"
#include "widget/city.h"
#include "widget/sidebar/city.h"
#include "widget/sidebar/common.h"
#include "widget/sidebar/extra.h"
#include "window/build_menu.h"

static void button_collapse_expand(int param1, int param2);
static void button_build(int submenu, int param2);

static image_button button_expand_sidebar[] = {
    {6, 4, 31, 20, IB_NORMAL, 90, 4, button_collapse_expand, button_none, 0, 0, 1}
};

static image_button buttons_build_collapsed[] = {
    {2, 32, 39, 26, IB_NORMAL, GROUP_SIDEBAR_BUTTONS, 0, button_build, button_none, BUILD_MENU_VACANT_HOUSE, 0, 1},
    {2, 67, 39, 26, IB_NORMAL, GROUP_SIDEBAR_BUTTONS, 8, button_build, button_none, BUILD_MENU_CLEAR_LAND, 0, 1},
    {2, 102, 39, 26, IB_NORMAL, GROUP_SIDEBAR_BUTTONS, 12, button_build, button_none, BUILD_MENU_ROAD, 0, 1},
    {2, 137, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 4, button_build, button_none, BUILD_MENU_WATER, 0, 1},
    {2, 172, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 40, button_build, button_none, BUILD_MENU_HEALTH, 0, 1, "UI", "Asclepius Button"},
    {2, 207, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 28, button_build, button_none, BUILD_MENU_TEMPLES, 0, 1},
    {2, 242, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 24, button_build, button_none, BUILD_MENU_EDUCATION, 0, 1},
    {2, 277, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 20, button_build, button_none, BUILD_MENU_ENTERTAINMENT, 0, 1},
    {2, 312, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 16, button_build, button_none, BUILD_MENU_ADMINISTRATION, 0, 1},
    {2, 347, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 44, button_build, button_none, BUILD_MENU_ENGINEERING, 0, 1},
    {2, 382, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 36, button_build, button_none, BUILD_MENU_SECURITY, 0, 1},
    {2, 417, 39, 26, IB_BUILD, GROUP_SIDEBAR_BUTTONS, 32, button_build, button_none, BUILD_MENU_INDUSTRY, 0, 1},
};

static struct {
    unsigned int focus_button_for_tooltip;
} data;


static void draw_sidebar_remainder(const int x_offset)
{
    const int width = SIDEBAR_COLLAPSED_WIDTH; // still comming from common

    const int available_height = sidebar_common_get_height() - SIDEBAR_MAIN_SECTION_HEIGHT;
    const int extra_height = sidebar_extra_draw_background(x_offset, SIDEBAR_FILLER_Y_OFFSET,
        width, available_height, 1, SIDEBAR_EXTRA_DISPLAY_ALL);
    sidebar_extra_draw_foreground();

    const int relief_y_offset = SIDEBAR_FILLER_Y_OFFSET + extra_height;
    sidebar_common_draw_relief(x_offset, relief_y_offset, GROUP_SIDE_PANEL, 1);
}

static void draw_buttons_collapsed(const int x_offset)
{
    image_buttons_draw(x_offset, 24, button_expand_sidebar, 1);
    image_buttons_draw(x_offset, 24, buttons_build_collapsed, 12);
}

static void enable_building_buttons(void)
{
    for (int i = 0; i < 12; i++) {
        buttons_build_collapsed[i].enabled = 1;
        if (building_menu_count_items(buttons_build_collapsed[i].parameter1) <= 0) {
            buttons_build_collapsed[i].enabled = 0;
        }
    }
}

int collapsed_sidebar_width(void)
{
    return SIDEBAR_COLLAPSED_WIDTH;
}


void draw_collapsed_sidebar_background(int x_offset)
{
    image_draw(image_group(GROUP_SIDE_PANEL), x_offset, 24, COLOR_MASK_NONE, SCALE_NONE);

    draw_buttons_collapsed(x_offset);
    draw_sidebar_remainder(x_offset);
}

void draw_collapsed_sidebar_foreground()
{
    if (building_menu_has_changed()) {
        enable_building_buttons();
    }

    const int x_offset = sidebar_common_get_x_offset_collapsed();
    draw_buttons_collapsed(x_offset);

    sidebar_extra_draw_foreground();
}

int handle_collapsed_sidebar_mouse(const mouse *m)
{
    if (widget_city_has_input()) {
        return 0;
    }
    int handled = 0;
    unsigned int button_id;
    data.focus_button_for_tooltip = 0;

    const int x_offset = sidebar_common_get_x_offset_collapsed();
    handled |= image_buttons_handle_mouse(m, x_offset, 24, button_expand_sidebar, 1, &button_id);
    if (button_id) {
        data.focus_button_for_tooltip = 12;
    }
    handled |= image_buttons_handle_mouse(m, x_offset, 24, buttons_build_collapsed, 12, &button_id);
    if (button_id) {
        data.focus_button_for_tooltip = button_id + 19;
    }

    return handled;
}

int handle_collapsed_sidebar_mouse_build_menu(const mouse *m)
{
    return image_buttons_handle_mouse(m,
        sidebar_common_get_x_offset_collapsed(), 24, buttons_build_collapsed, 12, 0);
}

unsigned int get_collapsed_sidebar_tooltip_text(tooltip_context *c)
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

static void button_collapse_expand(int param1, int param2)
{
    sidebar_next();
}

static void button_build(const int submenu, int param2)
{
    window_build_menu_show(submenu);
}