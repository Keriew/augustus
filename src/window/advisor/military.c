#include "military.h"

#include "city/data_private.h"
#include "city/figures.h"
#include "city/military.h"
#include "city/view.h"
#include "core/calc.h"
#include "figure/formation_legion.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/scrollbar.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "map/grid.h"
#include "scenario/invasion.h"
#include "translation/translation.h"
#include "window/city.h"

#define ADVISOR_HEIGHT 27

#define MAX_BUTTONS 18

#define MAX_VISIBLE_LEGIONS 6

static void button_go_to_legion(const generic_button *button);
static void button_return_to_fort(const generic_button *button);
static void button_return_all_or_call_all(const generic_button *button);
static void button_empire_service(const generic_button *button);
static void button_return_all_to_fort(const generic_button *button);
static void on_scroll(void);
static unsigned int get_num_legions_not_at_fort(void);
static unsigned int get_num_legions_with_saved_position(void);

static scrollbar_type scrollbar = { 592, 70, 272, 576, MAX_VISIBLE_LEGIONS, on_scroll };

static generic_button fort_buttons[] = {
    {384, 83, 30, 30, button_go_to_legion, 0, 1},
    {464, 83, 30, 30, button_return_to_fort, 0, 1},
    {544, 83, 30, 30, button_empire_service, 0, 1},
    {384, 127, 30, 30, button_go_to_legion, 0, 2},
    {464, 127, 30, 30, button_return_to_fort, 0, 2},
    {544, 127, 30, 30, button_empire_service, 0, 2},
    {384, 171, 30, 30, button_go_to_legion, 0, 3},
    {464, 171, 30, 30, button_return_to_fort, 0, 3},
    {544, 171, 30, 30, button_empire_service, 0, 3},
    {384, 215, 30, 30, button_go_to_legion, 0, 4},
    {464, 215, 30, 30, button_return_to_fort, 0, 4},
    {544, 215, 30, 30, button_empire_service, 0, 4},
    {384, 259, 30, 30, button_go_to_legion, 0, 5},
    {464, 259, 30, 30, button_return_to_fort, 0, 5},
    {544, 259, 30, 30, button_empire_service, 0, 5},
    {384, 303, 30, 30, button_go_to_legion, 0, 6},
    {464, 303, 30, 30, button_return_to_fort, 0, 6},
    {544, 303, 30, 30, button_empire_service, 0, 6},
};

static generic_button additional_buttons[] = {
    {443, 27, 70, 40, button_return_all_or_call_all}
};

static unsigned int focus_button_id;
static unsigned int focus_additional_button_id;
static unsigned int num_legions;
static unsigned int focus_button_legion_id;

static void init(void)
{
    num_legions = formation_get_num_legions();
    scrollbar_init(&scrollbar, 0, num_legions);
}

static int draw_background(void)
{
    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    image_draw(image_group(GROUP_ADVISOR_ICONS) + 1, 10, 10, COLOR_MASK_NONE, SCALE_NONE);
    lang_text_draw(51, 0, 60, 12, FONT_LARGE_BLACK);                  // Legion status

    lang_text_draw_centered(138, 36, 224, 50, 150, FONT_SMALL_PLAIN); // Morale
    lang_text_draw_centered(51, 1, 370, 35, 60, FONT_SMALL_PLAIN);    // Go to
    lang_text_draw_centered(51, 2, 370, 50, 60, FONT_SMALL_PLAIN);    // legion
    if (get_num_legions_not_at_fort() > 0) {
        text_draw_centered(translation_for(TR_RETURN_ALL_TO_FORT), 443, 35, 70, FONT_SMALL_PLAIN, 0);
    } else if (get_num_legions_with_saved_position() > 0) {
        text_draw_multiline(translation_for(TR_CALL_ALL_LEGIONS), 443, 30, 70, 1, FONT_SMALL_PLAIN, 0);
    } else {
        lang_text_draw_centered(51, 3, 444, 35, 70, FONT_SMALL_PLAIN);    // Return
        lang_text_draw_centered(51, 4, 444, 50, 70, FONT_SMALL_PLAIN);    // to fort
    }
    lang_text_draw_centered(51, 5, 524, 35, 70, FONT_SMALL_PLAIN);    // Empire
    lang_text_draw_centered(51, 6, 524, 50, 70, FONT_SMALL_PLAIN);    // service

    int enemy_text_id;
    if (city_figures_enemies()) {
        enemy_text_id = 10;
    } else if (city_figures_imperial_soldiers()) {
        enemy_text_id = 11;
    } else if (scenario_invasion_exists_upcoming()) {
        enemy_text_id = 9;
    } else {
        enemy_text_id = 8;
    }
    int distant_battle_text_id;
    if (city_military_distant_battle_roman_army_is_traveling_back()) {
        distant_battle_text_id = 15;
    } else if (city_military_distant_battle_roman_army_is_traveling_forth()) {
        distant_battle_text_id = 14;
    } else if (city_military_months_until_distant_battle() > 0) {
        distant_battle_text_id = 13;
    } else {
        distant_battle_text_id = 12;
    }
    int bullet_x = 60;
    int text_x = 80;
    int food_text;
    int food_stress = city_data.mess_hall.food_stress_cumulative;

    if (food_stress > 50 && !city_mess_hall_months_food_stored()) {
        food_text = TR_ADVISOR_LEGION_FOOD_CRITICAL;
    } else if (food_stress > 20 && !city_mess_hall_months_food_stored()) {
        food_text = TR_ADVISOR_LEGION_FOOD_NEEDED;
    } else {
        food_text = TR_ADVISOR_LEGION_MONTHS_FOOD_STORED;
    }

    if (num_legions <= 0) {
        image_draw(image_group(GROUP_BULLET), bullet_x, 359, COLOR_MASK_NONE, SCALE_NONE);
        lang_text_draw(51, enemy_text_id, text_x, 358, FONT_NORMAL_BLACK);

        image_draw(image_group(GROUP_BULLET), bullet_x, 379, COLOR_MASK_NONE, SCALE_NONE);
        lang_text_draw(51, distant_battle_text_id, text_x, 378, FONT_NORMAL_BLACK);
    } else {
        // has forts
        image_draw(image_group(GROUP_BULLET), bullet_x, 349, COLOR_MASK_NONE, SCALE_NONE);
        int width = lang_text_draw_amount(8, 46, city_military_total_soldiers(), text_x - 5, 348, FONT_NORMAL_BLACK);
        width += lang_text_draw(51, 7, text_x + width, 348, FONT_NORMAL_BLACK);
        lang_text_draw_amount(8, 48, city_military_total_legions(), text_x + width, 348, FONT_NORMAL_BLACK);

        image_draw(image_group(GROUP_BULLET), bullet_x, 369, COLOR_MASK_NONE, SCALE_NONE);
        lang_text_draw(51, enemy_text_id, text_x, 368, FONT_NORMAL_BLACK);

        image_draw(image_group(GROUP_BULLET), bullet_x, 389, COLOR_MASK_NONE, SCALE_NONE);
        lang_text_draw(51, distant_battle_text_id, text_x, 388, FONT_NORMAL_BLACK);

        image_draw(image_group(GROUP_BULLET), bullet_x, 409, COLOR_MASK_NONE, SCALE_NONE);
        width = text_draw(translation_for(food_text), text_x, 409, FONT_NORMAL_BLACK, 0);
        if (food_text == TR_ADVISOR_LEGION_MONTHS_FOOD_STORED) {
            text_draw_number(city_mess_hall_months_food_stored(), '@', " ", text_x + width, 409, FONT_NORMAL_BLACK, 0);
        }
    }

    inner_panel_draw(16, 70, 36, 17);
    if (num_legions <= 0) {
        lang_text_draw_multiline(51, 16, 64, 200, 496, FONT_NORMAL_GREEN);
        return ADVISOR_HEIGHT;
    }
    for (unsigned int i = 0; i < 6 && i < num_legions; i++) {
        const formation *m = formation_get(formation_for_legion(i + 1 + scrollbar.scroll_position));
        button_border_draw(22, 77 + 44 * i, 560, 40, 0);
        image_draw(m->legion_flag_id, 32, 82 + 44 * i,
            COLOR_MASK_NONE, SCALE_NONE);

        lang_text_draw(m->legion_name_group, m->legion_name_id, 84, 83 + 44 * i, FONT_NORMAL_WHITE);
        int width = text_draw_number(formation_legion_count_alive_soldiers(m->id), '@', " ", 84, 100 + 44 * i, FONT_NORMAL_GREEN, 0);
        switch (m->figure_type) {
            case FIGURE_FORT_LEGIONARY:
                text_draw(translation_for(TR_WINDOW_ADVISOR_LEGIONARIES), 84 + width, 100 + 44 * i, FONT_NORMAL_GREEN, 0);
                break;
            case FIGURE_FORT_MOUNTED:
                text_draw(translation_for(TR_WINDOW_ADVISOR_MOUNTED), 84 + width, 100 + 44 * i, FONT_NORMAL_GREEN, 0);
                break;
            case FIGURE_FORT_JAVELIN:
                text_draw(translation_for(TR_WINDOW_ADVISOR_JAVELIN), 84 + width, 100 + 44 * i, FONT_NORMAL_GREEN, 0);
                break;
            case FIGURE_FORT_INFANTRY:
                text_draw(translation_for(TR_WINDOW_ADVISOR_MILITARY_INFANTRY), 84 + width, 100 + 44 * i, FONT_NORMAL_GREEN, 0);
                break;
            case FIGURE_FORT_ARCHER:
                text_draw(translation_for(TR_WINDOW_ADVISOR_MILITARY_ARCHER), 84 + width, 100 + 44 * i, FONT_NORMAL_GREEN, 0);
        }
        int morale_offset = m->morale / 5;
        if (morale_offset > 20) {
            morale_offset = 20;
        }
        lang_text_draw_centered(138, 37 + morale_offset, 224, 91 + 44 * i, 150, FONT_NORMAL_GREEN);

        int image_id = image_group(GROUP_FORT_ICONS);
        button_border_draw(384, 83 + 44 * i, 30, 30, 0);
        image_draw(image_id, 387, 86 + 44 * i, COLOR_MASK_NONE, SCALE_NONE);

        button_border_draw(464, 83 + 44 * i, 30, 30, 0);
        if (m->is_at_fort) {
            image_draw(image_id + (formation_legion_can_return_to_position(m) ? 1 : 2), 467, 86 + 44 * i,
                COLOR_MASK_NONE, SCALE_NONE);
        } else if (m->in_distant_battle) {
            image_draw(image_id + 2, 467, 86 + 44 * i, COLOR_MASK_NONE, SCALE_NONE);
        } else {
            image_draw(image_id + 1, 467, 86 + 44 * i, COLOR_MASK_NONE, SCALE_NONE);
        }

        button_border_draw(544, 83 + 44 * i, 30, 30, 0);
        if (m->empire_service) {
            image_draw(image_id + 3, 547, 86 + 44 * i, COLOR_MASK_NONE, SCALE_NONE);
        } else {
            image_draw(image_id + 4, 547, 86 + 44 * i, COLOR_MASK_NONE, SCALE_NONE);
        }
    }

    return ADVISOR_HEIGHT;
}

static unsigned int get_num_legions_not_at_fort(void)
{
    unsigned int num_legions_not_at_fort = 0;
    for (unsigned int i = 0; i < num_legions; i++) {
        const formation *m = formation_get(formation_for_legion(i + 1));
        if (!m->in_distant_battle && !m->is_at_fort) {
            num_legions_not_at_fort++;
        }
    }
    return num_legions_not_at_fort;
}

static unsigned int get_num_legions_with_saved_position(void)
{
    unsigned int count = 0;
    for (unsigned int i = 0; i < num_legions; i++) {
        if (formation_legion_can_return_to_position(formation_get(formation_for_legion(i + 1)))) {
            count++;
        }
    }
    return count;
}

static void draw_foreground(void)
{
    scrollbar_draw(&scrollbar);
    num_legions = formation_get_num_legions();
    for (unsigned int i = 0; i < 6 && i < num_legions; i++) {
        button_border_draw(384, 83 + 44 * i, 30, 30, focus_button_id == 3 * i + 1);
        button_border_draw(464, 83 + 44 * i, 30, 30, focus_button_id == 3 * i + 2);
        button_border_draw(544, 83 + 44 * i, 30, 30, focus_button_id == 3 * i + 3);
    }

    if (get_num_legions_not_at_fort() > 0 || get_num_legions_with_saved_position() > 0) {
        button_border_draw(443, 27, 70, 40, focus_additional_button_id == 1);
    }
}

static int handle_mouse(const mouse *m)
{
    focus_additional_button_id = 0;
    if (scrollbar_handle_mouse(&scrollbar, m, 1)) {
        focus_button_id = 0;
        return 1;
    }
    unsigned int buttons = 3 * num_legions;
    if (buttons > MAX_BUTTONS) {
        buttons = MAX_BUTTONS;
    }
    int result = generic_buttons_handle_mouse(m, 0, 0, fort_buttons, buttons, &focus_button_id);
    focus_button_legion_id = focus_button_id ? (focus_button_id + 2) / 3 + scrollbar.scroll_position : 0;
    if (result == 0 && (get_num_legions_not_at_fort() > 0 || get_num_legions_with_saved_position() > 0)) {
        result = generic_buttons_handle_mouse(m, 0, 0, additional_buttons, 1, &focus_additional_button_id);
    } else if (result != 0) {
        focus_additional_button_id = 0;
    }
    return result;
}

static void button_go_to_legion(const generic_button *button)
{
    int legion_id = button->parameter1;
    const formation *m = formation_get(formation_for_legion(legion_id + scrollbar.scroll_position));
    city_view_go_to_grid_offset(map_grid_offset(m->x_home, m->y_home));
    window_city_show();
}

static void return_legion_to_fort(int legion_id)
{
    formation *m = formation_get(formation_for_legion(legion_id));
    if (!m->in_distant_battle && !m->is_at_fort) {
        formation_legion_return_home(m);
        window_invalidate();
    }
}

static void button_return_to_fort(const generic_button *button)
{
    int legion_id = button->parameter1;
    int formation_id = formation_for_legion(legion_id + scrollbar.scroll_position);
    formation *m = formation_get(formation_id);
    if (m->is_at_fort) {
        formation_legion_return_to_position(m);
        window_invalidate();
    } else {
        return_legion_to_fort(legion_id + scrollbar.scroll_position);
    }
}

static void button_empire_service(const generic_button *button)
{
    int legion_id = button->parameter1;
    int formation_id = formation_for_legion(legion_id + scrollbar.scroll_position);
    formation_toggle_empire_service(formation_id);
    formation_calculate_figures();
    window_invalidate();
}

static void button_return_all_or_call_all(const generic_button *button)
{
    if (get_num_legions_not_at_fort() > 0) {
        for (unsigned int i = 0; i < num_legions; i++) {
            return_legion_to_fort(i + 1);
        }
    } else {
    formation_legion_return_to_position_all();
    }
    window_invalidate();
}

static void on_scroll(void)
{
    window_invalidate();
}

static void get_tooltip_text(advisor_tooltip_result *r)
{
    if (focus_button_legion_id && focus_button_id % 3 == 2) {
        const formation *m = formation_get(formation_for_legion(focus_button_legion_id));
        if (m->is_at_fort) {
            if (formation_legion_can_return_to_position(m)) {
                r->translation_key = TR_RETURN_TO_POSITION;
            }
        } else {
            r->translation_key = TR_RETURN_TO_FORT;
        }
    } else if (focus_additional_button_id == 1) {
        r->translation_key = get_num_legions_not_at_fort() > 0 ?
            TR_RETURN_ALL_TO_FORT : TR_CALL_ALL_LEGIONS;
    }
}

const advisor_window_type *window_advisor_military(void)
{
    static const advisor_window_type window = {
        draw_background,
        draw_foreground,
        handle_mouse,
        get_tooltip_text
    };
    init();
    return &window;
}
