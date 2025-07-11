#include "top_menu.h"

#include "city/emperor.h"
#include "city/health.h"
#include "city/ratings.h"
#include "scenario/criteria.h"

#include "assets/assets.h"
#include "building/construction.h"
#include "city/constants.h"
#include "city/finance.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/lang.h"
#include "game/campaign.h"
#include "game/file.h"
#include "game/settings.h"
#include "game/state.h"
#include "game/system.h"
#include "game/time.h"
#include "game/undo.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/menu.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "scenario/property.h"
#include "widget/city.h"
#include "window/advisors.h"
#include "window/city.h"
#include "window/config.h"
#include "window/file_dialog.h"
#include "window/hotkey_config.h"
#include "window/main_menu.h"
#include "window/message_dialog.h"
#include "window/mission_selection.h"
#include "window/plain_message_dialog.h"
#include "window/popup_dialog.h"

enum {
    INFO_NONE = 0,
    INFO_FUNDS = 1,
    INFO_POPULATION = 2,
    INFO_DATE = 3,
    INFO_PERSONAL = 4,
    INFO_CULTURE = 5,
    INFO_PROSPERITY = 6,
    INFO_PEACE = 7,
    INFO_FAVOR = 8,
    INFO_RATINGS = 9
};
typedef enum {
    WIDGET_LAYOUT_NONE = 0,   // fits nothing (unlikely)
    WIDGET_LAYOUT_BASIC = 1,   // treasury, population, date
    WIDGET_LAYOUT_BASIC_SAV = 2,   // + savings
    WIDGET_LAYOUT_FULL = 3    // + ratings
} widget_layout_case_t;

#define BLACK_PANEL_BLOCK_WIDTH 20
#define BLACK_PANEL_MIDDLE_BLOCKS 4
#define BLACK_PANEL_TOTAL_BLOCKS 6

#define PANEL_MARGIN 10
#define START_MARGIN 10
#define MAX_SCREEN_WIDTH 1280

static void menu_file_replay_map(int param);
static void menu_file_load_game(int param);
static void menu_file_save_game(int param);
static void menu_file_delete_game(int param);
static void menu_file_exit_to_main_menu(int param);
static void menu_file_exit_game(int param);

static void menu_options_general(int param);
static void menu_options_user_interface(int param);
static void menu_options_gameplay(int param);
static void menu_options_city_management(int param);
static void menu_options_hotkeys(int param);
static void menu_options_monthly_autosave(int param);
static void menu_options_yearly_autosave(int param);

static void menu_help_help(int param);
static void menu_help_mouse_help(int param);
static void menu_help_warnings(int param);
static void menu_help_about(int param);

static void menu_advisors_go_to(int advisor);

static menu_item menu_file[] = {
    {1, 2, menu_file_replay_map, 0},
    {1, 3, menu_file_load_game, 0},
    {1, 4, menu_file_save_game, 0},
    {1, 6, menu_file_delete_game, 0},
    {CUSTOM_TRANSLATION, TR_BUTTON_BACK_TO_MAIN_MENU, menu_file_exit_to_main_menu, 0},
    {1, 5, menu_file_exit_game, 0},
};

static menu_item menu_options[] = {
    {CUSTOM_TRANSLATION, TR_CONFIG_HEADER_GENERAL, menu_options_general, 0},
    {CUSTOM_TRANSLATION, TR_CONFIG_HEADER_UI_CHANGES, menu_options_user_interface, 0},
    {CUSTOM_TRANSLATION, TR_CONFIG_HEADER_GAMEPLAY_CHANGES, menu_options_gameplay, 0},
    {CUSTOM_TRANSLATION, TR_CONFIG_HEADER_CITY_MANAGEMENT_CHANGES, menu_options_city_management, 0},
    {CUSTOM_TRANSLATION, TR_BUTTON_CONFIGURE_HOTKEYS, menu_options_hotkeys, 0},
    {19, 51, menu_options_monthly_autosave, 0},
    {CUSTOM_TRANSLATION, TR_BUTTON_YEARLY_AUTOSAVE_OFF, menu_options_yearly_autosave, 0},
};

static menu_item menu_help[] = {
    {3, 1, menu_help_help, 0},
    {3, 2, menu_help_mouse_help, 0},
    {3, 5, menu_help_warnings, 0},
    {3, 7, menu_help_about, 0},
};

static menu_item menu_advisors[] = {
    {4, 1, menu_advisors_go_to, ADVISOR_LABOR},
    {4, 2, menu_advisors_go_to, ADVISOR_MILITARY},
    {4, 3, menu_advisors_go_to, ADVISOR_IMPERIAL},
    {4, 4, menu_advisors_go_to, ADVISOR_RATINGS},
    {4, 5, menu_advisors_go_to, ADVISOR_TRADE},
    {4, 6, menu_advisors_go_to, ADVISOR_POPULATION},
    {CUSTOM_TRANSLATION, TR_HEADER_HOUSING, menu_advisors_go_to, ADVISOR_HOUSING},
    {4, 7, menu_advisors_go_to, ADVISOR_HEALTH},
    {4, 8, menu_advisors_go_to, ADVISOR_EDUCATION},
    {4, 9, menu_advisors_go_to, ADVISOR_ENTERTAINMENT},
    {4, 10, menu_advisors_go_to, ADVISOR_RELIGION},
    {4, 11, menu_advisors_go_to, ADVISOR_FINANCIAL},
    {4, 12, menu_advisors_go_to, ADVISOR_CHIEF},
};

static menu_bar_item menu[] = {
    {1, menu_file, 6},
    {2, menu_options, 7},
    {3, menu_help, 4},
    {4, menu_advisors, 13},
};

static const int INDEX_OPTIONS = 1;
static const int INDEX_HELP = 2;
typedef struct {
    int start;
    int end;
}top_menu_tooltip_range;
static struct {
    top_menu_tooltip_range funds;
    top_menu_tooltip_range population;
    top_menu_tooltip_range date;
    top_menu_tooltip_range personal;
    top_menu_tooltip_range culture;
    top_menu_tooltip_range prosperity;
    top_menu_tooltip_range peace;
    top_menu_tooltip_range favor;
    unsigned char hovered_widget; // group var for all ratings
    int open_sub_menu;
    int focus_menu_id;
    int focus_sub_menu_id;
} data;

static struct {
    int population;
    int treasury;
    int month;
    int personal;
    int culture;
    int prosperity;
    int peace;
    int favor;
} drawn;

static void clear_state(void)
{
    data.open_sub_menu = 0;
    data.focus_menu_id = 0;
    data.focus_sub_menu_id = 0;
}

static void set_text_for_monthly_autosave(void)
{
    menu_update_text(&menu[INDEX_OPTIONS], 5, setting_monthly_autosave() ? 51 : 52);
}

static void set_text_for_yearly_autosave(void)
{
    menu_update_text(&menu[INDEX_OPTIONS], 6,
        config_get(CONFIG_GP_CH_YEARLY_AUTOSAVE) ? TR_BUTTON_YEARLY_AUTOSAVE_ON : TR_BUTTON_YEARLY_AUTOSAVE_OFF);
}


static void set_text_for_tooltips(void)
{
    int new_text;
    switch (setting_tooltips()) {
        case TOOLTIPS_NONE:
            new_text = 2;
            break;
        case TOOLTIPS_SOME:
            new_text = 3;
            break;
        case TOOLTIPS_FULL:
            new_text = 4;
            break;
        default:
            return;
    }
    menu_update_text(&menu[INDEX_HELP], 1, new_text);
}

static void set_text_for_warnings(void)
{
    menu_update_text(&menu[INDEX_HELP], 2, setting_warnings() ? 6 : 5);
}

static void init(void)
{
    set_text_for_monthly_autosave();
    set_text_for_yearly_autosave();
    set_text_for_tooltips();
    set_text_for_warnings();
}

static void draw_background(void)
{
    window_city_draw_panels();
    window_city_draw();
}

static void draw_foreground(void)
{
    if (data.open_sub_menu) {
        menu_draw(&menu[data.open_sub_menu - 1], data.focus_sub_menu_id);
    }
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    widget_top_menu_handle_input(m, h);
}

static void top_menu_window_show(void)
{
    window_type window = {
        WINDOW_TOP_MENU,
        draw_background,
        draw_foreground,
        handle_input
    };
    init();
    window_show(&window);
}

static void refresh_background(void)
{
    int block_width = 24;
    int image_base = image_group(GROUP_TOP_MENU);
    int s_width = screen_width();
    for (int i = 0; i * block_width < s_width; i++) {
        image_draw(image_base + i % 8, i * block_width, 0, COLOR_MASK_NONE, SCALE_NONE);
    }
}

static int draw_black_panel(int x, int y, int width)
{
    int blocks = (width / BLACK_PANEL_BLOCK_WIDTH) - 1;
    if ((width % BLACK_PANEL_BLOCK_WIDTH) > 0) {
        blocks++;
    }

    image_draw(image_group(GROUP_TOP_MENU) + 14, x, y, COLOR_MASK_NONE, SCALE_NONE);
    if (blocks <= BLACK_PANEL_MIDDLE_BLOCKS) {
        return BLACK_PANEL_TOTAL_BLOCKS * BLACK_PANEL_BLOCK_WIDTH;
    }
    static int black_panel_base_id;
    if (!black_panel_base_id) {
        black_panel_base_id = assets_get_image_id("UI", "Top_UI_Panel");
    }
    int x_offset = BLACK_PANEL_BLOCK_WIDTH * (BLACK_PANEL_MIDDLE_BLOCKS + 1);
    blocks -= BLACK_PANEL_MIDDLE_BLOCKS;

    for (int i = 0; i < blocks; i++) {
        image_draw(black_panel_base_id + (i % BLACK_PANEL_MIDDLE_BLOCKS) + 1, x + x_offset, y,
            COLOR_MASK_NONE, SCALE_NONE);
        x_offset += BLACK_PANEL_BLOCK_WIDTH;
    }

    image_draw(black_panel_base_id + 5, x + x_offset, y, COLOR_MASK_NONE, SCALE_NONE);

    return x_offset + BLACK_PANEL_BLOCK_WIDTH;
}

static int get_black_panel_usable_width(int desired_width)
{
    int blocks = desired_width / BLACK_PANEL_BLOCK_WIDTH;
    if (desired_width % BLACK_PANEL_BLOCK_WIDTH > 0) {
        blocks++;
    }
    int usable_blocks = (blocks - 2) < 0 ? 0 : (blocks - 2); // subtract left + right caps, no negatives
    return usable_blocks * BLACK_PANEL_BLOCK_WIDTH;
}

int get_black_panel_total_width_for_text_id(int group, int id, int number, font_t font)
{
    int label_width = lang_text_get_width(group, id, font);
    int number_width = text_get_number_width(number, '@', " ", font);
    int text_width = label_width + number_width;
    int usable_width = get_black_panel_usable_width(text_width);
    return usable_width + 2 * BLACK_PANEL_BLOCK_WIDTH;
}

static widget_layout_case_t widget_top_menu_measure_layout(int available_width, font_t font)
{
    char dummy_rating[20];
    sprintf(dummy_rating, "%d (%d)", 999, 999);

    int treasury_w = get_black_panel_total_width_for_text_id(6, 0, city_finance_treasury(), font);
    int savings_w = get_black_panel_total_width_for_text_id(52, 1, city_emperor_personal_savings(), font);
    int population_w = get_black_panel_total_width_for_text_id(6, 1, city_population(), font);
    int date_w = 100;  // fixed width
    int rating_w = text_get_width((const uint8_t *) dummy_rating, font) * 4 + BLACK_PANEL_BLOCK_WIDTH;

    int group1 = treasury_w + PANEL_MARGIN + population_w + PANEL_MARGIN + date_w;
    int group2 = savings_w + PANEL_MARGIN;
    int group3 = rating_w;

    if (group1 > available_width)
        return WIDGET_LAYOUT_NONE;
    if (group1 + group2 > available_width)
        return WIDGET_LAYOUT_BASIC;
    if (group1 + group2 + group3 > available_width)
        return WIDGET_LAYOUT_BASIC_SAV;
    return WIDGET_LAYOUT_FULL;
}

static int draw_panel_with_text_and_number(int offset, int lang_section, int lang_index,
                                           int number, int margin, font_t font, color_t label_color, color_t num_color)
{
    int label_width = lang_text_get_width(lang_section, lang_index, font);
    int number_width = text_get_number_width(number, '@', " ", font);
    int text_width = label_width + number_width + 2 * margin;

    // Compute required usable width + total panel width (adds end caps)
    int panel_width = draw_black_panel(offset, 0, text_width);
    int end_of_panel = offset + panel_width;
    int usable_width = end_of_panel - offset - 2 * BLACK_PANEL_BLOCK_WIDTH;
    int draw_x = offset + BLACK_PANEL_BLOCK_WIDTH + (usable_width / 2) - text_width / 2;


    // Draw label
    lang_text_draw_colored(lang_section, lang_index, draw_x, 5, font, label_color);

    // Draw number right after label
    text_draw_number(number, '@', " ", draw_x + label_width, 5, font, num_color);

    return end_of_panel - offset;
}

static int draw_rating_panel(int offset, int info_id, int box_width,
                             font_t font, color_t val_color, color_t goal_color)
{
    int value = 0, goal = 0;

    switch (info_id) {
        case INFO_CULTURE:
            value = city_rating_culture();
            goal = scenario_criteria_culture_enabled() ? scenario_criteria_culture() : 0;
            break;
        case INFO_PROSPERITY:
            value = city_rating_prosperity();
            goal = scenario_criteria_prosperity_enabled() ? scenario_criteria_prosperity() : 0;
            break;
        case INFO_PEACE:
            value = city_rating_peace();
            goal = scenario_criteria_peace_enabled() ? scenario_criteria_peace() : 0;
            break;
        case INFO_FAVOR:
            value = city_rating_favor();
            goal = scenario_criteria_favor_enabled() ? scenario_criteria_favor() : 0;
            break;
        default:
            return 0;
    }

    int gap_length = 2;
    int value_width = text_get_number_width(value, '@', " ", font);
    int goal_width = text_get_number_width(goal, '(', ")", font);
    int total_width = value_width + gap_length + goal_width;

    int x = offset + (box_width - total_width) / 2;
    text_draw_number(value, '@', " ", x, 5, font, val_color);
    text_draw_number(goal, '(', ")", x + value_width + gap_length, 5, font, goal_color);

    return box_width;
}

static int draw_rating_with_goal(int offset, int value, int goal,
                                 int box_width, font_t font, color_t val_color, color_t goal_color)
{
    // Measure total width of both parts
    int gap_length = 2;
    int value_width = text_get_number_width(value, '@', " ", font);
    int goal_width = text_get_number_width(goal, '(', ")", font);
    int total_width = value_width + goal_width;

    // Center inside box
    int x = offset + (box_width - total_width) / 2;
    // Draw value
    text_draw_number(value, '@', " ", x, 5, font, val_color);
    // Draw goal next to value
    text_draw_number(goal, '(', ")", x + value_width + gap_length, 5, font, goal_color);

    return box_width;
}
void widget_top_menu_draw(int force)
{
    // Skip redraw if nothing changed
    if (!force &&
        drawn.treasury == city_finance_treasury() &&
        drawn.population == city_population() &&
        drawn.month == game_time_month() &&
        drawn.personal == city_emperor_personal_savings() &&
        drawn.culture == city_rating_culture() &&
        drawn.prosperity == city_rating_prosperity() &&
        drawn.peace == city_rating_peace() &&
        drawn.favor == city_rating_favor()) {
        return;
    }

    // Layout settings
    int s_width = screen_width();
    font_t font = (s_width < 800) ? FONT_NORMAL_GREEN : FONT_NORMAL_PLAIN;
    color_t pop_color = (s_width < 800) ? COLOR_MASK_NONE : COLOR_WHITE;
    color_t date_color = (s_width < 800) ? COLOR_MASK_NONE : COLOR_FONT_YELLOW;

    refresh_background();
    int menu_end = menu_bar_draw(menu, 4, s_width < 1024 ? 338 : 493);
    int available_width = s_width - menu_end;
    int center_x = menu_end + (available_width * 0.50);
    int right_anchor = s_width - (available_width * 0.05);
    int base_x = menu_end + (available_width * 0.05);

    widget_layout_case_t layout = widget_top_menu_measure_layout(available_width, font);

    // --- Group 1: Treasury + Population + Date ---
    if (layout >= WIDGET_LAYOUT_BASIC) {
        int treasury = city_finance_treasury();
        color_t t_color = (treasury < 0) ? COLOR_FONT_RED : COLOR_WHITE;
        data.funds.start = base_x;
        base_x += draw_panel_with_text_and_number(base_x, 6, 0, treasury, 3, font, t_color, t_color);
        data.funds.end = base_x;
        base_x += PANEL_MARGIN;

        if (layout >= WIDGET_LAYOUT_BASIC_SAV) {
            data.personal.start = base_x;
            base_x += draw_panel_with_text_and_number(base_x, 6, 0, city_emperor_personal_savings(), 3, font, date_color, date_color);
            data.personal.end = base_x;
            base_x += PANEL_MARGIN * 3;

        }
        data.population.start = base_x;
        base_x += draw_panel_with_text_and_number(base_x, 6, 1, city_population(), 3, font, pop_color, pop_color);
        data.population.end = base_x;

        int date_w = 100;
        int date_x = center_x - (date_w / 2);
        draw_black_panel(date_x, 0, date_w);
        data.date.start = date_x;
        data.date.end = date_x + date_w;
        lang_text_draw_month_year_max_width(game_time_month(), game_time_year(), date_x + BLACK_PANEL_BLOCK_WIDTH, 5,
                                            date_w, font, date_color);

    }

    // --- Group 2: Ratings (if enabled and space allows) ---
    if (layout == WIDGET_LAYOUT_FULL && s_width >= 1024) {
        char rating_buf[20];
        sprintf(rating_buf, "%d (%d)", 999, 999);
        int label_w = text_get_width((const uint8_t *) rating_buf, font);
        int block_w = label_w * 4;
        int slot_w = block_w / 4;
        int x = right_anchor - block_w;


        draw_black_panel(x, 0, block_w);
        x += BLACK_PANEL_BLOCK_WIDTH / 2;

        const int rating_ids[] = { INFO_CULTURE, INFO_PROSPERITY, INFO_PEACE, INFO_FAVOR };
        top_menu_tooltip_range *targets[] = { &data.culture, &data.prosperity, &data.peace, &data.favor };
        for (int i = 0; i < 4; ++i) {
            int x_offset = x + slot_w * i;
            targets[i]->start = x_offset;
            targets[i]->end = x_offset + draw_rating_panel(x_offset, rating_ids[i], slot_w, font, pop_color, date_color);
        }

    }

    // --- Cache current state ---
    drawn.treasury = city_finance_treasury();
    drawn.population = city_population();
    drawn.month = game_time_month();
    drawn.personal = city_emperor_personal_savings();
    drawn.culture = city_rating_culture();
    drawn.prosperity = city_rating_prosperity();
    drawn.peace = city_rating_peace();
    drawn.favor = city_rating_favor();
}


static int handle_input_submenu(const mouse *m, const hotkeys *h)
{
    if (m->right.went_up || h->escape_pressed) {
        clear_state();
        window_go_back();
        return 1;
    }
    int menu_id = menu_bar_handle_mouse(m, menu, 4, &data.focus_menu_id);
    if (menu_id && menu_id != data.open_sub_menu) {
        window_request_refresh();
        data.open_sub_menu = menu_id;
    }
    if (!menu_handle_mouse(m, &menu[data.open_sub_menu - 1], &data.focus_sub_menu_id)) {
        if (m->left.went_up) {
            clear_state();
            window_go_back();
            return 1;
        }
    }
    return 0;
}

static int get_info_id(int mouse_x, int mouse_y)
{
    if (mouse_y < 4 || mouse_y >= 18) {
        return INFO_NONE;
    }
    if (mouse_x > data.funds.start && mouse_x < data.funds.end) {
        return INFO_FUNDS;
    }
    if (mouse_x > data.personal.start && mouse_x < data.personal.end) {
        return INFO_PERSONAL;
    }
    if (mouse_x > data.population.start && mouse_x < data.population.end) {
        return INFO_POPULATION;
    }
    if (mouse_x > data.date.start && mouse_x < data.date.end) {
        return INFO_DATE;
    }
    if (mouse_x > data.culture.start && mouse_x < data.culture.end) {
        return INFO_CULTURE;
    }
    if (mouse_x > data.prosperity.start && mouse_x < data.prosperity.end) {
        return INFO_PROSPERITY;
    }
    if (mouse_x > data.peace.start && mouse_x < data.peace.end) {
        return INFO_PEACE;
    }
    if (mouse_x > data.favor.start && mouse_x < data.favor.end) {
        return INFO_FAVOR;
    }
    return INFO_NONE;
}

static int handle_right_click(int type)
{
    if (type == INFO_NONE) {
        return 0;
    }
    if (type == INFO_FUNDS) {
        window_message_dialog_show(MESSAGE_DIALOG_TOP_FUNDS, window_city_draw_all);
    } else if (type == INFO_POPULATION) {
        window_message_dialog_show(MESSAGE_DIALOG_TOP_POPULATION, window_city_draw_all);
    } else if (type == INFO_DATE) {
        window_message_dialog_show(MESSAGE_DIALOG_TOP_DATE, window_city_draw_all);
    }
    return 1;
}

static int handle_mouse_menu(const mouse *m)
{
    int menu_id = menu_bar_handle_mouse(m, menu, 4, &data.focus_menu_id);
    int top_menu_widget = get_info_id(m->x, m->y); //hack to get mouse position over widget
    data.hovered_widget = INFO_NONE;
    switch (top_menu_widget) {
        case INFO_PERSONAL:
            data.hovered_widget = INFO_PERSONAL;
            if (m->left.went_up) {
                menu_advisors_go_to(ADVISOR_IMPERIAL);
            }
        case INFO_POPULATION:
            data.hovered_widget = INFO_POPULATION;
            if (m->left.went_up) {
                menu_advisors_go_to(ADVISOR_POPULATION);
            }
        case INFO_CULTURE:
        case INFO_PROSPERITY:
        case INFO_PEACE:
        case INFO_FAVOR:
            data.hovered_widget = INFO_RATINGS;
            if (m->left.went_up) {
                menu_advisors_go_to(ADVISOR_RATINGS);
            }
    }
    if (menu_id && m->left.went_up) {
        data.open_sub_menu = menu_id;
        top_menu_window_show();
        return 1;
    }
    if (m->right.went_up) {
        return handle_right_click(get_info_id(m->x, m->y));
    }

    return 0;
}

int widget_top_menu_handle_input(const mouse *m, const hotkeys *h)
{
    if (widget_city_has_input()) {
        return 0;
    }
    if (data.open_sub_menu) {
        return handle_input_submenu(m, h);
    } else {
        return handle_mouse_menu(m);
    }
}

int widget_top_menu_get_tooltip_text(tooltip_context *c)
{
    if (data.focus_menu_id) {
        return 49 + data.focus_menu_id;
    }
    int button_id = get_info_id(c->mouse_x, c->mouse_y);
    if (button_id) {
        if (button_id < 4) {
            return 59 + button_id;
        } else if (button_id == INFO_PERSONAL) {
            c->text_group = CUSTOM_TRANSLATION;
            return TR_TOOLTIP_PERSONAL_SAVINGS;
        } else {
            c->text_group = 53;
            c->num_extra_texts = 1;
            c->extra_text_groups[0] = 53;
            switch (button_id) {
                case INFO_CULTURE:
                    c->extra_text_ids[0] = (scenario_criteria_culture() <= 90)
                        ? 9 + city_rating_explanation_for(SELECTED_RATING_CULTURE) : 50;
                    return 1;

                case INFO_PROSPERITY:
                    c->extra_text_ids[0] = (scenario_criteria_prosperity() <= 90)
                        ? 16 + city_rating_explanation_for(SELECTED_RATING_PROSPERITY) : 51;
                    return 2;
                case INFO_PEACE:
                    c->extra_text_ids[0] = (scenario_criteria_peace() <= 90)
                        ? 41 + city_rating_explanation_for(SELECTED_RATING_PEACE) : 52;
                    return 3;
                case INFO_FAVOR:
                    c->extra_text_ids[0] = (scenario_criteria_favor() <= 90)
                        ? 27 + city_rating_explanation_for(SELECTED_RATING_FAVOR) : 53;
                    return 4;
                default:
                    return 0;
            }
        }

    }
    return 0;
}

static void replay_map_confirmed(int confirmed, int checked)
{
    if (!confirmed) {
        window_city_show();
        return;
    }
    if (!game_campaign_is_active()) {
        window_city_show();
        if (!game_file_start_scenario_by_name(scenario_name())) {
            window_plain_message_dialog_show_with_extra(TR_REPLAY_MAP_NOT_FOUND_TITLE,
                TR_REPLAY_MAP_NOT_FOUND_MESSAGE, 0, scenario_name());
        }
    } else {
        int mission_id = game_campaign_is_original() ? scenario_campaign_mission() : 0;
        setting_set_personal_savings_for_mission(mission_id, scenario_starting_personal_savings());
        scenario_save_campaign_player_name();
        window_mission_selection_show_again();
    }
}

static void menu_file_replay_map(int param)
{
    clear_state();
    building_construction_clear_type();
    window_popup_dialog_show_confirmation(lang_get_string(1, 2), 0, 0, replay_map_confirmed);
}

static void menu_file_load_game(int param)
{
    clear_state();
    building_construction_clear_type();
    window_go_back();
    window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_LOAD);
}

static void menu_file_save_game(int param)
{
    clear_state();
    window_go_back();
    window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_SAVE);
}

static void menu_file_delete_game(int param)
{
    clear_state();
    window_go_back();
    window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_DELETE);
}

static void menu_file_confirm_exit(int accepted, int checked)
{
    if (accepted) {
        system_exit();
    } else {
        window_city_return();
    }
}

static void main_menu_confirmed(int confirmed, int checked)
{
    if (!confirmed) {
        window_city_show();
        return;
    }
    building_construction_clear_type();
    game_undo_disable();
    game_state_reset_overlay();
    window_main_menu_show(1);
}

static void menu_file_exit_to_main_menu(int param)
{
    clear_state();
    window_popup_dialog_show_confirmation(translation_for(TR_BUTTON_BACK_TO_MAIN_MENU), 0, 0,
        main_menu_confirmed);
}

static void menu_file_exit_game(int param)
{
    clear_state();
    window_popup_dialog_show(POPUP_DIALOG_QUIT, menu_file_confirm_exit, 1);
}

static void menu_options_general(int param)
{
    clear_state();
    window_go_back();
    window_config_show(CONFIG_PAGE_GENERAL, 0);
}

static void menu_options_user_interface(int param)
{
    clear_state();
    window_go_back();
    window_config_show(CONFIG_PAGE_UI_CHANGES, 0);
}

static void menu_options_gameplay(int param)
{
    clear_state();
    window_go_back();
    window_config_show(CONFIG_PAGE_GAMEPLAY_CHANGES, 0);
}

static void menu_options_city_management(int param)
{
    clear_state();
    window_go_back();
    window_config_show(CONFIG_PAGE_CITY_MANAGEMENT_CHANGES, 0);
}

static void menu_options_hotkeys(int param)
{
    clear_state();
    window_go_back();
    window_hotkey_config_show();
}

static void menu_options_monthly_autosave(int param)
{
    setting_toggle_monthly_autosave();
    set_text_for_monthly_autosave();
}

static void menu_options_yearly_autosave(int param)
{
    config_set(CONFIG_GP_CH_YEARLY_AUTOSAVE, !config_get(CONFIG_GP_CH_YEARLY_AUTOSAVE));
    config_save();
    set_text_for_yearly_autosave();
}

static void menu_help_help(int param)
{
    clear_state();
    window_go_back();
    window_message_dialog_show(MESSAGE_DIALOG_HELP, window_city_draw_all);
}

static void menu_help_mouse_help(int param)
{
    setting_cycle_tooltips();
    set_text_for_tooltips();
}

static void menu_help_warnings(int param)
{
    setting_toggle_warnings();
    set_text_for_warnings();
}

static void menu_help_about(int param)
{
    clear_state();
    window_go_back();
    window_message_dialog_show(MESSAGE_DIALOG_ABOUT, window_city_draw_all);
}

static void menu_advisors_go_to(int advisor)
{
    clear_state();
    window_go_back();
    window_advisors_show_advisor(advisor);
}
