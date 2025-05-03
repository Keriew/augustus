#include "invasions.h"

#include "core/string.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/grid_box.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "scenario/data.h"
#include "scenario/editor.h"
#include "scenario/invasion.h"
#include "scenario/property.h"
#include "window/editor/attributes.h"
#include "window/editor/edit_invasion.h"
#include "window/editor/map.h"

static void button_invasion(const grid_box_item *item);
static void button_new_invasion(const generic_button *button);
static void draw_invasion_button(const grid_box_item *item);

static struct {
    const invasion_t **invasions;
    unsigned int total_invasions;
    unsigned int invasions_in_use;
    unsigned int new_invasion_button_focused;
    void (*on_select)(int);
} data;

static generic_button new_invasion_button = {
    195, 350, 250, 25, button_new_invasion
};

static grid_box_type invasion_buttons = {
    .x = 10,
    .y = 65,
    .width = 38 * BLOCK_SIZE,
    .height = 19 * BLOCK_SIZE,
    .num_columns = 1,
    .item_height = 28,
    .item_margin.horizontal = 10,
    .item_margin.vertical = 4,
    .extend_to_hidden_scrollbar = 1,
    .on_click = button_invasion,
    .draw_item = draw_invasion_button
};

static void limit_and_sort_list(void)
{
    data.invasions_in_use = 0;
    for (unsigned int i = 0; i < data.total_invasions; i++) {
        const invasion_t *invasion = scenario_invasion_get(i);
        if (!invasion->year) {
            continue;
        }
        data.invasions[data.invasions_in_use] = invasion;
        data.invasions_in_use++;
    }
    for (unsigned int i = 0; i < data.invasions_in_use; i++) {
        for (unsigned int j = data.invasions_in_use - 1; j > 0; j--) {
            const invasion_t *current = data.invasions[j];
            const invasion_t *prev = data.invasions[j - 1];
            if (current->type && (!prev->type || prev->year > current->year)) {
                const invasion_t *tmp = data.invasions[j];
                data.invasions[j] = data.invasions[j - 1];
                data.invasions[j - 1] = tmp;
            }
        }
    }
}

static void update_invasion_list(void)
{
    int current_invasions = scenario_invasion_count_total();
    if (current_invasions != data.total_invasions) {
        free(data.invasions);
        data.invasions = 0;
        if (current_invasions) {
            data.invasions = malloc(current_invasions * sizeof(invasion_t *));
            if (!data.invasions) {
                grid_box_update_total_items(&invasion_buttons, 0);
                data.total_invasions = 0;
                data.invasions_in_use = 0;
                return;
            }
        }
        data.total_invasions = current_invasions;
    }
    limit_and_sort_list();
    grid_box_update_total_items(&invasion_buttons, data.invasions_in_use);
}

static void draw_background(void)
{
    update_invasion_list();

    window_editor_map_draw_all();

    graphics_in_dialog();

    outer_panel_draw(0, 0, 40, 30);
    lang_text_draw(44, 15, 20, 12, FONT_LARGE_BLACK);

    lang_text_draw(CUSTOM_TRANSLATION, TR_EDITOR_INVASION_DATE, 20, 50, FONT_SMALL_PLAIN); // Invasion date:
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_INVASION_SIZE, 130, 50, 105, FONT_SMALL_PLAIN); // Invasion size:
    lang_text_draw(CUSTOM_TRANSLATION, TR_EDITOR_INVASION_ENEMY_TYPE, 240, 50, FONT_SMALL_PLAIN); // Enemy type:
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_INVASION_FROM, 330, 50, 70, FONT_SMALL_PLAIN); // From:
    lang_text_draw_centered(50, 21, 380, 50, 85, FONT_SMALL_PLAIN); // Priority
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_REPEAT_FREQUENCY2, 460, 30, 120, FONT_SMALL_PLAIN); // Repeat frequency
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_REPEAT_TIMES2, 460, 50, 45, FONT_SMALL_PLAIN); // Times:
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_REPEAT_FREQUENCY_YEARS2, 520, 50, 60, FONT_SMALL_PLAIN); // Years:

    lang_text_draw_centered(13, 3, 0, 456, 640, FONT_NORMAL_BLACK);
    lang_text_draw_multiline(152, 2, 20, 380, 600, FONT_NORMAL_BLACK);

    if (!data.invasions_in_use) {
        lang_text_draw_centered(44, 20, 0, 165, 640, FONT_LARGE_BLACK);
    }

    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_NEW_INVASION, new_invasion_button.x + 8,
        new_invasion_button.y + 8, new_invasion_button.width - 16, FONT_NORMAL_BLACK);

    graphics_reset_dialog();

    grid_box_request_refresh(&invasion_buttons);
}

static void draw_invasion_button(const grid_box_item *item)
{
    button_border_draw(item->x, item->y, item->width, item->height, item->is_focused);
    const invasion_t *invasion = data.invasions[item->index];
    text_draw_number(invasion->year, '+', " ", item->x + 5, item->y + 7, FONT_NORMAL_BLACK, 0);
    lang_text_draw_year(scenario_property_start_year() + invasion->year, item->x + 40, item->y + 7, FONT_NORMAL_BLACK);

    int width1 = text_get_number_width(invasion->amount.min, '@', " ", FONT_NORMAL_BLACK);
    if (invasion->amount.min == invasion->amount.max) {
        text_draw_number(invasion->amount.min, '@', " ", 180 - (width1 / 2), item->y + 7, FONT_NORMAL_BLACK, 0);
    } else if (invasion->amount.max > invasion->amount.min) {
        text_draw_number(invasion->amount.min, '@', " ", 175 - width1, item->y + 7, FONT_NORMAL_BLACK, 0);
        text_draw(string_from_ascii("-"), 180, item->y + 7, FONT_NORMAL_BLACK, 0);
        text_draw_number(invasion->amount.max, '@', " ", 185, item->y + 7, FONT_NORMAL_BLACK, 0);
    }

    lang_text_draw(34, invasion->type, 230, item->y + 7, FONT_NORMAL_BLACK);

    if (invasion->type != INVASION_TYPE_DISTANT_BATTLE) {
        if (invasion->from + 1 == 9) {
            text_draw(string_from_ascii("RND"), 352, item->y + 7, FONT_SMALL_PLAIN, 0);
        } else {
            text_draw_number(invasion->from + 1, '@', " ", 355, item->y + 7, FONT_NORMAL_BLACK, 0);
        }
    }

    if (invasion->type != INVASION_TYPE_DISTANT_BATTLE) {
        if (invasion->attack_type == FORMATION_ATTACK_FOOD_CHAIN) {
            text_draw(string_from_ascii("F"), 420, item->y + 7, FONT_SMALL_PLAIN, COLOR_FONT_BLUE);
        } else if (invasion->attack_type == FORMATION_ATTACK_GOLD_STORES) {
            text_draw(string_from_ascii("G"), 420, item->y + 7, FONT_SMALL_PLAIN, COLOR_MASK_DARK_GREEN);
        } else if (invasion->attack_type == FORMATION_ATTACK_BEST_BUILDINGS) {
            text_draw(string_from_ascii("B"), 420, item->y + 7, FONT_SMALL_PLAIN, 0);
        } else if (invasion->attack_type == FORMATION_ATTACK_TROOPS) {
            text_draw(string_from_ascii("T"), 419, item->y + 7, FONT_SMALL_PLAIN, COLOR_FONT_RED);
        } else if (invasion->attack_type == FORMATION_ATTACK_RANDOM) {
            text_draw(string_from_ascii("RND"), 410, item->y + 7, FONT_SMALL_PLAIN, 0);
        }
    }

    if (invasion->repeat.times == INVASIONS_REPEAT_INFINITE) {
        text_draw(string_from_ascii("INF"), 473, item->y + 7, FONT_SMALL_PLAIN, 0);
    } else if (invasion->repeat.times == 0) {
        text_draw(string_from_ascii("-"), 480, item->y + 7, FONT_NORMAL_BLACK, 0);
    } else {
        int width = text_get_number_width(invasion->repeat.times, '@', " ", FONT_NORMAL_BLACK);
        text_draw_number(invasion->repeat.times, '@', " ", 480 - (width / 2), item->y + 7, FONT_NORMAL_BLACK, 0);
    }

    if (invasion->repeat.times == 0) {
        text_draw(string_from_ascii(" "), 480, item->y + 7, FONT_NORMAL_BLACK, 0);
    } else if (invasion->repeat.times > 0 || invasion->repeat.times == INVASIONS_REPEAT_INFINITE) {
        int width2 = text_get_number_width(invasion->repeat.interval.min, '@', " ", FONT_NORMAL_BLACK);
        if (invasion->repeat.interval.min == invasion->repeat.interval.max) {
            text_draw_number(invasion->repeat.interval.min, '@', " ", 550 - (width2 / 2), item->y + 7, FONT_NORMAL_BLACK, 0);
        } else if (invasion->repeat.interval.max > invasion->repeat.interval.min) {
            text_draw_number(invasion->repeat.interval.min, '@', " ", 545 - width2, item->y + 7, FONT_NORMAL_BLACK, 0);
            text_draw(string_from_ascii("-"), 550, item->y + 7, FONT_NORMAL_BLACK, 0);
            text_draw_number(invasion->repeat.interval.max, '@', " ", 555, item->y + 7, FONT_NORMAL_BLACK, 0);
        }
    }

}

static void draw_foreground(void)
{
    graphics_in_dialog();

    if (data.invasions_in_use) {
        grid_box_draw(&invasion_buttons);
    }
    button_border_draw(new_invasion_button.x, new_invasion_button.y,
        new_invasion_button.width, new_invasion_button.height, data.new_invasion_button_focused);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (grid_box_handle_input(&invasion_buttons, m_dialog, 1) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, &new_invasion_button, 1, &data.new_invasion_button_focused)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        window_editor_attributes_show();
    }
}

static void button_invasion(const grid_box_item *item)
{
    window_editor_edit_invasion_show(data.invasions[item->index]->id);
}

static void button_new_invasion(const generic_button *button)
{
    int new_invasion_id = scenario_invasion_new();
    if (new_invasion_id >= 0) {
        window_editor_edit_invasion_show(new_invasion_id);
    }
}

void window_editor_invasions_show(void)
{
    window_type window = {
        WINDOW_EDITOR_INVASIONS,
        draw_background,
        draw_foreground,
        handle_input
    };
    grid_box_init(&invasion_buttons, scenario_invasion_count_active());
    window_show(&window);
}
