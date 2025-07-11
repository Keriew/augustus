#include "overlay_menu.h"

#include "city/view.h"
#include "core/time.h"
#include "game/state.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "translation/translation.h"
#include "window/city.h"

#define MENU_X_OFFSET 170
#define SUBMENU_X_OFFSET 348
#define MENU_Y_OFFSET 72
#define MENU_ITEM_HEIGHT 24
#define MENU_CLICK_MARGIN 20

#define MAX_BUTTONS 8

static void button_menu_item(const generic_button *button);
static void button_submenu_item(const generic_button *button);

#define OVERLAY_BUTTONS 11

static generic_button menu_buttons[] = {
    {0, 0, 160, 24, button_menu_item, 0, 0},
    {0, 24, 160, 24, button_menu_item, 0, 1},
    {0, 48, 160, 24, button_menu_item, 0, 2},
    {0, 72, 160, 24, button_menu_item, 0, 3},
    {0, 96, 160, 24, button_menu_item, 0, 4},
    {0, 120, 160, 24, button_menu_item, 0, 5},
    {0, 144, 160, 24, button_menu_item, 0, 6},
    {0, 168, 160, 24, button_menu_item, 0, 7},
    {0, 192, 160, 24, button_menu_item, 0, 8},
    {0, 216, 160, 24, button_menu_item, 0, 9},
    {0, 240, 160, 24, button_menu_item, 0, 10},
};
static generic_button submenu_buttons[] = {
    {0, 0, 160, 24, button_submenu_item, 0, 0},
    {0, 24, 160, 24, button_submenu_item, 0, 1},
    {0, 48, 160, 24, button_submenu_item, 0, 2},
    {0, 72, 160, 24, button_submenu_item, 0, 3},
    {0, 96, 160, 24, button_submenu_item, 0, 4},
    {0, 120, 160, 24, button_submenu_item, 0, 5},
    {0, 144, 160, 24, button_submenu_item, 0, 6},
    {0, 168, 160, 24, button_submenu_item, 0, 7},
    {0, 192, 160, 24, button_submenu_item, 0, 8},
    {0, 216, 160, 24, button_submenu_item, 0, 9},
};

static const int MENU_ID_TO_OVERLAY[OVERLAY_BUTTONS] = { OVERLAY_NONE, OVERLAY_WATER, 1, 3, 5, 6, 7, OVERLAY_RELIGION, OVERLAY_DESIRABILITY, OVERLAY_SENTIMENT, OVERLAY_ROADS };
static const int MENU_ID_TO_SUBMENU_ID[OVERLAY_BUTTONS] = { 0, 0, 1, 2, 3, 4, 5, 0, 0 };
static const int ADDITIONAL_OVERLAY_TR[] = { TR_OVERLAY_ROADS, TR_OVERLAY_LEVY, TR_OVERLAY_TAVERN, TR_OVERLAY_ARENA_COL, TR_OVERLAY_SENTIMENT, TR_OVERLAY_MOTHBALL, TR_OVERLAY_ENEMY, TR_OVERLAY_LOGISTICS, TR_OVERLAY_SICKNESS, TR_OVERLAY_EFFICIENCY, TR_OVERLAY_STORAGES, TR_OVERLAY_HEALTH, TR_OVERLAY_EMPLOYMENT };

static const int SUBMENU_ID_TO_OVERLAY[6][OVERLAY_BUTTONS] = {
    {0},
    {OVERLAY_FIRE, OVERLAY_DAMAGE, OVERLAY_CRIME, OVERLAY_NATIVE, OVERLAY_PROBLEMS, OVERLAY_ENEMY, OVERLAY_SICKNESS},
    {OVERLAY_ENTERTAINMENT, OVERLAY_TAVERN, OVERLAY_THEATER, OVERLAY_AMPHITHEATER, OVERLAY_ARENA, OVERLAY_COLOSSEUM, OVERLAY_HIPPODROME},
    {OVERLAY_EDUCATION, OVERLAY_SCHOOL, OVERLAY_LIBRARY, OVERLAY_ACADEMY},
    {OVERLAY_HEALTH, OVERLAY_BARBER, OVERLAY_BATHHOUSE, OVERLAY_CLINIC, OVERLAY_HOSPITAL},
    {OVERLAY_LOGISTICS, OVERLAY_FOOD_STOCKS, OVERLAY_EFFICIENCY, OVERLAY_MOTHBALL, OVERLAY_TAX_INCOME, OVERLAY_LEVY, OVERLAY_EMPLOYMENT},
};

static struct {
    int selected_menu;
    int selected_submenu;
    unsigned int num_submenu_items;
    time_millis submenu_focus_time;

    unsigned int menu_focus_button_id;
    unsigned int submenu_focus_button_id;

    int keep_submenu_open;
} data;

static void init(void)
{
    data.selected_submenu = 0;
    data.num_submenu_items = 0;
}

static void draw_background(void)
{
    window_city_draw_panels();
}

static int get_sidebar_x_offset(void)
{
    int view_x, view_y, view_width, view_height;
    city_view_get_viewport(&view_x, &view_y, &view_width, &view_height);
    return view_x + view_width;
}

static void draw_foreground(void)
{
    window_city_draw();
    int x_offset = get_sidebar_x_offset();
    for (unsigned int i = 0; i < OVERLAY_BUTTONS; i++) {
        label_draw(x_offset - 170, 74 + 24 * i, 10, data.menu_focus_button_id == i + 1 ? 1 : 2);
        int overlay = MENU_ID_TO_OVERLAY[i];
        int translation = get_overlay_translation(overlay);
        if (translation) {
            text_draw_centered(translation_for(translation), x_offset - 170, 78 + 24 * i, 160, FONT_NORMAL_GREEN, 0);
        } else {
            lang_text_draw_centered(14, MENU_ID_TO_OVERLAY[i], x_offset - 170, 78 + 24 * i, 160, FONT_NORMAL_GREEN);
        }
    }
    if (data.selected_submenu > 0) {
        image_draw(image_group(GROUP_BULLET), x_offset - 185, 80 + 24 * data.selected_menu,
            COLOR_MASK_NONE, SCALE_NONE);
        for (unsigned int i = 0; i < data.num_submenu_items; i++) {
            int overlay = SUBMENU_ID_TO_OVERLAY[data.selected_submenu][i];
            int translation = get_overlay_translation(overlay);

            label_draw(x_offset - 348, 74 + 24 * (i + data.selected_menu),
                10, data.submenu_focus_button_id == i + 1 ? 1 : 2);

            if (translation) {
                text_draw_centered(translation_for(translation), x_offset - 348, 78 + 24 * (i + data.selected_menu), 160, FONT_NORMAL_GREEN, 0);
            } else {
                lang_text_draw_centered(14, overlay, x_offset - 348, 78 + 24 * (i + data.selected_menu), 160, FONT_NORMAL_GREEN);
            }
        }
    }
}

static int count_submenu_items(int submenu_id)
{
    int total = 0;
    for (int i = 0; i < OVERLAY_BUTTONS && SUBMENU_ID_TO_OVERLAY[submenu_id][i] > 0; i++) {
        total++;
    }
    return total;
}

static void open_submenu(int index, int keep_open)
{
    data.keep_submenu_open = keep_open;
    data.selected_menu = index;
    data.selected_submenu = MENU_ID_TO_SUBMENU_ID[index];
    data.num_submenu_items = count_submenu_items(data.selected_submenu);
    window_invalidate();
}

static void close_submenu(void)
{
    data.keep_submenu_open = 0;
    data.selected_menu = 0;
    data.selected_submenu = 0;
    data.num_submenu_items = 0;
    window_invalidate();
}

static void handle_submenu_focus(void)
{
    if (data.menu_focus_button_id || data.submenu_focus_button_id) {
        data.submenu_focus_time = time_get_millis();
        if (data.menu_focus_button_id) {
            open_submenu(data.menu_focus_button_id - 1, 0);
        }
    } else if (time_get_millis() - data.submenu_focus_time > 500) {
        close_submenu();
    }
}

static int click_outside_menu(const mouse *m, int x_offset)
{
    return m->left.went_up &&
        (m->x < x_offset - MENU_CLICK_MARGIN - (data.selected_submenu ? SUBMENU_X_OFFSET : MENU_X_OFFSET) ||
        m->x > x_offset + MENU_CLICK_MARGIN ||
        m->y < MENU_Y_OFFSET - MENU_CLICK_MARGIN ||
        m->y > MENU_Y_OFFSET + MENU_CLICK_MARGIN + MENU_ITEM_HEIGHT * MAX_BUTTONS);
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    int x_offset = get_sidebar_x_offset();
    int handled = 0;
    handled |= generic_buttons_handle_mouse(m, x_offset - MENU_X_OFFSET, MENU_Y_OFFSET,
        menu_buttons, OVERLAY_BUTTONS, &data.menu_focus_button_id);

    if (!data.keep_submenu_open) {
        handle_submenu_focus();
    }
    if (data.selected_submenu) {
        handled |= generic_buttons_handle_mouse(
            m, x_offset - SUBMENU_X_OFFSET, MENU_Y_OFFSET + MENU_ITEM_HEIGHT * data.selected_menu,
            submenu_buttons, data.num_submenu_items, &data.submenu_focus_button_id);
    }
    if (!handled && input_go_back_requested(m, h)) {
        if (data.keep_submenu_open) {
            close_submenu();
        } else {
            window_city_show();
        }
        return;
    }
    if (!handled && click_outside_menu(m, x_offset)) {
        close_submenu();
        window_city_show();
    }
}

static void button_menu_item(const generic_button *button)
{
    int index = button->parameter1;

    if (MENU_ID_TO_SUBMENU_ID[index] == 0) {
        game_state_set_overlay(MENU_ID_TO_OVERLAY[index]);
        close_submenu();
        window_city_show();
    } else {
        if (data.keep_submenu_open && data.selected_submenu == MENU_ID_TO_SUBMENU_ID[index]) {
            close_submenu();
        } else {
            open_submenu(index, 1);
        }
    }
}

static void button_submenu_item(const generic_button *button)
{
    int index = button->parameter1;

    int overlay = SUBMENU_ID_TO_OVERLAY[data.selected_submenu][index];
    if (overlay) {
        game_state_set_overlay(overlay);
    }
    close_submenu();
    window_city_show();
}

void window_overlay_menu_show(void)
{
    window_type window = {
        WINDOW_OVERLAY_MENU,
        draw_background,
        draw_foreground,
        handle_input
    };
    init();
    window_show(&window);
}

int get_overlay_translation(int overlay)
{
    if (overlay >= OVERLAY_ROADS) {
        return ADDITIONAL_OVERLAY_TR[overlay - OVERLAY_ROADS];
    } else {
        return 0;
    }
}
