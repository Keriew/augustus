#include "top_menu.h"

#include "assets/assets.h"
#include "building/construction.h"
#include "city/constants.h"
#include "city/emperor.h"
#include "city/finance.h"
#include "city/health.h"
#include "city/population.h"
#include "city/ratings.h"
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
#include "scenario/criteria.h"
#include "scenario/property.h"
#include "widget/city.h"
#include "window/advisors.h"
#include "window/advisor/health.h"
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
    INFO_RATINGS = 9,
    INFO_HEALTH = 10 //health last since doesnt count towards goals
};
typedef enum {
    WIDGET_LAYOUT_NONE = 0,   // fits nothing
    WIDGET_LAYOUT_BASIC = 1,   // treasury, population, date
    WIDGET_LAYOUT_FULL = 2    // + ratings and savings
} widget_layout_case_t;

#define BLACK_PANEL_BLOCK_WIDTH 20
#define BLACK_PANEL_MIDDLE_BLOCKS 4
#define BLACK_PANEL_TOTAL_BLOCKS 6

#define PANEL_MARGIN 10
#define DATE_FIELD_WIDTH 140

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
static void ratings_advisors_go_to(int advisor);

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
    top_menu_tooltip_range ratings;
    top_menu_tooltip_range health;
    unsigned char savings_on_right;
    int menu_end;
    int open_sub_menu;
    int focus_menu_id;
    int focus_sub_menu_id;
    int extra_space;
    int basic_margin;
} data;

static struct {
    int population;
    int treasury;
    signed char day;
    int personal;
    int culture;
    int prosperity;
    int peace;
    int favor;
    int health;
    int s_width;
    widget_layout_case_t current_layout;
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
    data.savings_on_right = config_get(CONFIG_UI_MOVE_SAVINGS_TO_RIGHT);
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
    if (width < BLACK_PANEL_BLOCK_WIDTH * BLACK_PANEL_TOTAL_BLOCKS) {
        width = BLACK_PANEL_BLOCK_WIDTH * BLACK_PANEL_TOTAL_BLOCKS;  // enforce minimum panel size
    }

    int blocks = ((width + BLACK_PANEL_BLOCK_WIDTH - 1) / BLACK_PANEL_BLOCK_WIDTH) - 2;
    int actual_width = (blocks + 2) * BLACK_PANEL_BLOCK_WIDTH;

    // Step 1: Draw start cap
    image_draw(image_group(GROUP_TOP_MENU) + 14, x, y, COLOR_MASK_NONE, SCALE_NONE);
    x += BLACK_PANEL_BLOCK_WIDTH;

    // Step 2: Load base panel images
    static int black_panel_base_id;
    if (!black_panel_base_id) {
        black_panel_base_id = assets_get_image_id("UI", "Top_UI_Panel");
    }

    // Step 3: Draw middle blocks
    for (int i = 0; i < blocks; i++) {
        image_draw(black_panel_base_id + (i % BLACK_PANEL_MIDDLE_BLOCKS) + 1, x, y,
            COLOR_MASK_NONE, SCALE_NONE);
        x += BLACK_PANEL_BLOCK_WIDTH;
    }

    // Step 4: Draw end cap
    image_draw(black_panel_base_id + 5, x, y, COLOR_MASK_NONE, SCALE_NONE);

    return actual_width;
}


static int get_black_panel_actual_width(int desired_width)
{
    int blocks = (desired_width / BLACK_PANEL_BLOCK_WIDTH) - 1;
    if ((desired_width % BLACK_PANEL_BLOCK_WIDTH) > 0) {
        blocks++;
    }

    if (blocks < BLACK_PANEL_MIDDLE_BLOCKS) {
        blocks = BLACK_PANEL_MIDDLE_BLOCKS; // ensure at least minimal middle blocks
    }
    return (blocks + 2) * BLACK_PANEL_BLOCK_WIDTH;
}

static int get_black_panel_total_width_for_text_id(int group, int id, int number, font_t font)
{
    int label_width = lang_text_get_width(group, id, font);
    int number_width = text_get_number_width(number, '@', " ", font);
    int text_width = label_width + number_width; // add padding
    int total_width = get_black_panel_actual_width(text_width);

    return total_width;
}

static int detect_layout_change(void)
{
    unsigned char new_savings_on_right = config_get(CONFIG_UI_MOVE_SAVINGS_TO_RIGHT);
    signed char screen_width_unchanged = (drawn.s_width == screen_width());

    if (data.savings_on_right == new_savings_on_right && screen_width_unchanged) {
        return 0; // no layout change
    }

    data.savings_on_right = new_savings_on_right;
    drawn.s_width = screen_width();
    return 1;
}

static void reset_data_states(void)
{
    top_menu_tooltip_range *ranges[] = {
        &data.funds, &data.population, &data.date,
        &data.personal, &data.culture, &data.prosperity,
        &data.peace, &data.favor, &data.ratings, &data.health
    };

    for (size_t i = 0; i < sizeof(ranges) / sizeof(ranges[0]); ++i) {
        ranges[i]->start = 0;
        ranges[i]->end = 0;
    }
    data.basic_margin = PANEL_MARGIN;
    data.extra_space = 0;
}

static widget_layout_case_t widget_top_menu_measure_layout(int available_width, font_t font)
{
    char tmp[32];
    sprintf(tmp, "%d(%d)", 999, 999); // max rating string
    int rating_one_block_w = text_get_width((const uint8_t *) tmp, font);
    int w_funds = get_black_panel_total_width_for_text_id(6, 0, 99999, font); //5 digit city treasury as base
    int w_savings = get_black_panel_total_width_for_text_id(6, 0, 9999, font); //4 digit city treasury as base
    int w_population = get_black_panel_total_width_for_text_id(6, 1, 99999, font); //5 digit city pop as base
    int w_date = DATE_FIELD_WIDTH + BLACK_PANEL_BLOCK_WIDTH; // returned block is longer
    int w_rating = get_black_panel_actual_width(rating_one_block_w * 4.5f);
    // half block for health, one extra block to ensure everything fits in edge cases

    int min_basic = w_funds + w_population + w_date;
    int min_full = w_funds + w_savings + w_population + DATE_FIELD_WIDTH + w_rating + BLACK_PANEL_BLOCK_WIDTH;
    // decide BASIC vs FULL, no margins for minimum size

    widget_layout_case_t layout;
    data.basic_margin = PANEL_MARGIN;
    data.extra_space = 0;

    reset_data_states();
    if (available_width >= min_full) {
        layout = WIDGET_LAYOUT_FULL;
        data.extra_space = (available_width >= min_full * 1.2f) ? BLACK_PANEL_BLOCK_WIDTH : 0;
        data.basic_margin = (data.extra_space == 0) ? 0 : data.basic_margin;
    } else if (available_width >= min_basic) {
        layout = WIDGET_LAYOUT_BASIC;
        data.extra_space = (available_width >= min_basic * 1.2f) ? BLACK_PANEL_BLOCK_WIDTH : 0;
        data.basic_margin = (data.extra_space == 0) ? 0 : data.basic_margin;
    } else {
        layout = WIDGET_LAYOUT_NONE;
    }

    // GROUP 1:
    int current_x = data.menu_end + data.basic_margin;
    data.funds.start = current_x;
    data.funds.end = current_x + w_funds + data.extra_space;
    current_x += w_funds + data.basic_margin + data.extra_space;

    if (layout == WIDGET_LAYOUT_FULL && !data.savings_on_right) {
        data.personal.start = current_x;
        data.personal.end = current_x + w_savings + data.extra_space;
        current_x += w_savings + data.basic_margin + data.extra_space;
    }

    data.population.start = current_x;
    data.population.end = current_x + w_population + data.extra_space;
    current_x += w_population + data.basic_margin + data.extra_space;
    int group1_end_x = current_x;

    // precompute some values
    float avail_w = (float) available_width;
    int group1_span = group1_end_x - data.menu_end;
    int group3_min_w = w_rating + (data.savings_on_right ? (data.basic_margin + w_savings + data.extra_space) : data.basic_margin);
    int bar_right_edge = data.menu_end + available_width - data.extra_space;


    // GROUP 2: date and  45% / 80% checks + OOB guard
    int date_start_x;
    if (layout == WIDGET_LAYOUT_FULL) {
        unsigned char g1_too_big = (group1_span >= 0.45f * avail_w);
        unsigned char g1g3_too_big = (group1_span + group3_min_w >= 0.80f * avail_w);

        int center_pos_x = data.menu_end + (available_width - w_date) / 2;
        unsigned char center_breaks_g3 =
            (center_pos_x + w_date + data.basic_margin + group3_min_w) > bar_right_edge;
        unsigned char center_overlap = center_pos_x < group1_end_x;
        if (g1_too_big || g1g3_too_big || center_breaks_g3 || center_overlap) {
            date_start_x = group1_end_x;
        } else {
            date_start_x = center_pos_x;
        }
    } else {
        date_start_x = group1_end_x;
    }
    data.date.start = date_start_x;
    data.date.end = date_start_x + w_date + data.extra_space;


    // GROUP 3
    if (layout == WIDGET_LAYOUT_FULL) {
        int group3_start_x;
        unsigned char force_sequence = (group1_span + group3_min_w + w_date >= 0.90f * avail_w);
        // if >80%, force sequentional drawing
        unsigned char too_big_overall =
            (group3_min_w > 0.45f * avail_w)
            || (group3_min_w > 0.90f * (available_width - group1_span - w_date))
            || (group3_min_w > (0.5f * avail_w - w_date));

        if (force_sequence || too_big_overall) {
            group3_start_x = data.date.end + PANEL_MARGIN; // force Panel margin here for visual consistency
        } else {
            // anchor to right edge
            group3_start_x = bar_right_edge - w_rating - PANEL_MARGIN - data.extra_space;
            if (data.savings_on_right) {
                group3_start_x -= (data.basic_margin + w_savings);
            }
        }
        int x3 = group3_start_x;
        if (data.savings_on_right) {
            data.personal.start = x3;
            data.personal.end = x3 + w_savings + data.extra_space;
            x3 += w_savings + data.basic_margin + data.extra_space;
        }
        data.ratings.start = x3;
        data.ratings.end = x3 + w_rating + data.extra_space;
    }
    drawn.current_layout = layout;
    return layout;
}

static int draw_panel_with_text_and_number(int offset, int lang_section, int lang_index,
    int number, int margin, int fixed_width, font_t font, color_t label_color, color_t num_color)
{
    int label_width = lang_text_get_width(lang_section, lang_index, font);
    int number_width = text_get_number_width(number, '@', " ", font);
    int text_width = label_width + number_width + 2 * margin;

    // Compute required usable width + total panel width (adds end caps)
    int black_panel_width = (fixed_width > 0) ? fixed_width : text_width;

    int panel_width = draw_black_panel(offset, 0, black_panel_width);
    int end_of_panel = offset + panel_width;
    int usable_width = end_of_panel - offset - 2 * BLACK_PANEL_BLOCK_WIDTH;
    int draw_x = offset + BLACK_PANEL_BLOCK_WIDTH + (usable_width / 2) - text_width / 2;
    // Draw label
    lang_text_draw_colored(lang_section, lang_index, draw_x, 5, font, label_color);
    // Draw number right after label
    text_draw_number(number, '@', "\0", draw_x + label_width, 5, font, num_color);

    return end_of_panel - offset;
}

static int draw_rating_panel(int offset, int info_id, int box_width, int gap_length,
                             font_t font, color_t goal_color)
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

    int value_width = text_get_number_width(value, '@', " ", font);
    int goal_width = text_get_number_width(goal, '(', ")", font);
    int total_width = value_width + gap_length + goal_width;

    int x = offset + (box_width - total_width) / 2;
    color_t val_color = (value >= goal && goal > 0) ? COLOR_FONT_GREEN : COLOR_WHITE;
    text_draw_number(value, '@', " ", x, 5, font, val_color);
    text_draw_number(goal, '(', ")", x + value_width, 5, font, goal_color);
    return box_width;
}

static int draw_health_panel(int offset, int box_width, font_t font)
{
    int health = city_health();

    // choose color based on health level
    color_t color;
    if (health > 75)
        color = COLOR_FONT_GREEN;
    else if (health >= 25)
        color = COLOR_WHITE;
    else
        color = COLOR_FONT_RED;

    int health_w = text_get_number_width(health, ' ', "", font);

    // center it in the box
    int x = offset + (box_width - health_w) / 2;
    text_draw_number(health, ' ', "", x + 10, 5, font, color);

    return box_width;
}

static color_t get_savings_color_mask(void)
{
    city_emperor_calculate_gift_costs(); //update gift costs before checking affordability
    if (city_emperor_can_send_gift(GIFT_LAVISH)) {
        return COLOR_FONT_GREEN;
    }
    if (city_emperor_can_send_gift(GIFT_GENEROUS)) {
        return COLOR_WHITE;
    }
    if (city_emperor_can_send_gift(GIFT_MODEST)) {
        return COLOR_FONT_ORANGE;
    }
    // cant afford even the modest gift -> red
    return COLOR_FONT_RED;
}

static char get_cosmetic_day_of_month(void)
{
    static const char days_in_month[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    int year = game_time_year();
    int month = game_time_month();
    int day = game_time_day();
    int tick = game_time_tick();

    int is_leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    //extra points for leap years
    int days_this_month = days_in_month[month];
    if (month == 1 && is_leap) {
        days_this_month = 29;
    }

    // Total ticks into the current in-game month (0 to 799)
    int total_ticks = day * 50 + tick;
    // Scale to real calendar day, add 1 to offset 0-based indexing
    char cosmetic_day = (total_ticks * days_this_month) / 800 + 1;

    if (cosmetic_day > days_this_month) {
        cosmetic_day = days_this_month;
    }

    return cosmetic_day;
}

void widget_top_menu_draw(int force)
{
    // Skip redraw if nothing changed
    if (!force &&
        drawn.treasury == city_finance_treasury() &&
        drawn.population == city_population() &&
        drawn.day == get_cosmetic_day_of_month() &&
        drawn.personal == city_emperor_personal_savings() &&
        drawn.culture == city_rating_culture() &&
        drawn.prosperity == city_rating_prosperity() &&
        drawn.peace == city_rating_peace() &&
        drawn.favor == city_rating_favor() &&
        drawn.health == city_health() &&
        !detect_layout_change()) {
        return;
    }

    // Layout settings
    int s_width = screen_width();


    refresh_background();
    data.menu_end = menu_bar_draw(menu, 4, 340);
    //calculate layout
    widget_layout_case_t layout = widget_top_menu_measure_layout(s_width - data.menu_end, FONT_NORMAL_PLAIN);

    font_t font = (layout == WIDGET_LAYOUT_NONE) ? FONT_NORMAL_GREEN : FONT_NORMAL_PLAIN;
    color_t pop_color = (layout == WIDGET_LAYOUT_NONE) ? COLOR_MASK_NONE : COLOR_WHITE;
    color_t date_color = (layout == WIDGET_LAYOUT_NONE) ? COLOR_MASK_NONE : COLOR_FONT_YELLOW;
    int treasury = city_finance_treasury();
    color_t treasury_color = (treasury < 0) ? COLOR_FONT_RED : COLOR_WHITE;


    // Minimal layout: no panels, just numbers
    if (layout == WIDGET_LAYOUT_NONE) {
        int current_x = data.menu_end;
        data.funds.start = current_x;
        current_x += lang_text_draw_colored(6, 0, current_x, 5, font, treasury_color);
        // Draw number right after label
        current_x += text_draw_number(treasury, '@', "\0", current_x, 5, font, treasury_color);
        data.funds.end = current_x;
        current_x += PANEL_MARGIN;
        data.population.start = current_x;
        current_x += lang_text_draw_colored(6, 1, current_x, 5, font, pop_color);
        current_x += text_draw_number(city_population(), '@', "\0", current_x, 5, font, pop_color);
        data.population.end = current_x;
        current_x += PANEL_MARGIN;
        data.date.start = current_x;
        lang_text_draw_month_year_max_width(game_time_month(), game_time_year(),
                current_x, 5, DATE_FIELD_WIDTH - BLACK_PANEL_BLOCK_WIDTH, font, date_color);
        data.date.end = current_x + DATE_FIELD_WIDTH - BLACK_PANEL_BLOCK_WIDTH;
    }


    if (layout >= WIDGET_LAYOUT_BASIC) {
        // --- Draw Treasury ---
        int treasury_w = data.funds.end - data.funds.start;
        draw_panel_with_text_and_number(data.funds.start, 6, 0, treasury, 3, treasury_w, font, treasury_color,
            treasury_color);
        // --- Draw Population ---
        int population_w = data.population.end - data.population.start;

        draw_panel_with_text_and_number(data.population.start, 6, 1, city_population(), 3, population_w, font,
         pop_color, pop_color);
        // --- Draw Date ---
        int date_x = data.date.start;
        draw_black_panel(date_x, 0, DATE_FIELD_WIDTH + data.extra_space);
        int month_offset = date_x + data.extra_space / 2 + BLACK_PANEL_BLOCK_WIDTH + 14; // 14px is enough for day
        text_draw_number(get_cosmetic_day_of_month(), 0, "", date_x + PANEL_MARGIN + data.extra_space / 2, 5, font,
         date_color);
        lang_text_draw_month_year_max_width(game_time_month(), game_time_year(),
         month_offset, 5, DATE_FIELD_WIDTH - BLACK_PANEL_BLOCK_WIDTH - 14, font, date_color);
    }

    // --- Group 2: Ratings (if enabled and space allows) ---
    if (layout == WIDGET_LAYOUT_FULL) {
        // Draw Savings 
        color_t savings_color = get_savings_color_mask();
        int savings_w = data.personal.end - data.personal.start;

        draw_panel_with_text_and_number(data.personal.start, 6, 0, city_emperor_personal_savings(), 3, savings_w,
        font, savings_color, savings_color);

        char rating_buf[20];
        sprintf(rating_buf, "%d(%d)", 999, 999);
        int label_w = text_get_width((const uint8_t *) rating_buf, font);
        int block_w = data.ratings.end - data.ratings.start; //half for health rating
        int slot_w = (block_w - data.extra_space - (label_w / 2)) / 4;
        int x = data.ratings.start;


        draw_black_panel(x, 0, block_w);
        x += data.extra_space / 2;
        const int rating_ids[] = { INFO_CULTURE, INFO_PROSPERITY, INFO_PEACE, INFO_FAVOR };
        top_menu_tooltip_range *targets[] = { &data.culture, &data.prosperity, &data.peace, &data.favor };
        int gap_length = 2;
        for (int i = 0; i < 4; ++i) {
            int x_offset = x + slot_w * i;
            targets[i]->start = x_offset;
            targets[i]->end = x_offset + draw_rating_panel(x_offset, rating_ids[i], slot_w, gap_length, font, date_color);
        }
        data.health.start = targets[3]->end;
        data.health.end = draw_health_panel(targets[3]->end - gap_length * 2, slot_w / 2, font) + targets[3]->end;

    }

    // --- Cache current state ---
    drawn.treasury = treasury;
    drawn.population = city_population();
    drawn.day = get_cosmetic_day_of_month();
    drawn.personal = city_emperor_personal_savings();
    drawn.culture = city_rating_culture();
    drawn.prosperity = city_rating_prosperity();
    drawn.peace = city_rating_peace();
    drawn.favor = city_rating_favor();
    drawn.health = city_health();
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
    if (mouse_x > data.health.start && mouse_x < data.health.end) {
        return INFO_HEALTH;
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

    switch (top_menu_widget) {
        case INFO_FUNDS:
            if (m->left.went_up) {
                ratings_advisors_go_to(ADVISOR_FINANCIAL);
            }
        case INFO_PERSONAL:
            if (m->left.went_up) {
                ratings_advisors_go_to(ADVISOR_IMPERIAL);
            }
        case INFO_POPULATION:
            if (m->left.went_up) {
                ratings_advisors_go_to(ADVISOR_POPULATION);
            }
        case INFO_CULTURE:
        case INFO_PROSPERITY:
        case INFO_PEACE:
        case INFO_FAVOR:
            if (m->left.went_up) {
                ratings_advisors_go_to(ADVISOR_RATINGS);
            }
        case INFO_HEALTH:
            if (m->left.went_up) {
                ratings_advisors_go_to(ADVISOR_HEALTH);
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
        if (button_id == INFO_POPULATION) {
            if (scenario_criteria_population_enabled()) {
                const uint8_t *original_tooltip = lang_get_string(68, 59 + INFO_POPULATION);
                const uint8_t *precomposed_text = lang_get_string(CUSTOM_TRANSLATION, TR_TOOLTIP_POPULATION_GOAL);
                int value = scenario_criteria_population();
                static char formatted_text[128];
                snprintf(formatted_text, sizeof(formatted_text), "%s\n%s %d", original_tooltip, precomposed_text, value);
                const uint8_t *final_text = (const uint8_t *) formatted_text;
                c->precomposed_text = final_text;
            }
        }
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
                    c->extra_text_ids[0] = (city_rating_culture() <= 90)
                        ? 9 + city_rating_explanation_for(SELECTED_RATING_CULTURE) : 50;
                    return 1;
                case INFO_PROSPERITY:
                    c->extra_text_ids[0] = (city_rating_prosperity() <= 90)
                        ? 16 + city_rating_explanation_for(SELECTED_RATING_PROSPERITY) : 51;
                    return 2;
                case INFO_PEACE:
                    c->extra_text_ids[0] = (city_rating_peace() <= 90)
                        ? 41 + city_rating_explanation_for(SELECTED_RATING_PEACE) : 52;
                    return 3;
                case INFO_FAVOR:
                    c->extra_text_ids[0] = (city_rating_favor() <= 90)
                        ? 27 + city_rating_explanation_for(SELECTED_RATING_FAVOR) : 53;
                    return 4;
                case INFO_HEALTH:
                    c->text_group = CUSTOM_TRANSLATION;
                    c->extra_text_groups[0] = 56;
                    c->extra_text_ids[0] = window_advisor_health_get_rating_text_id();
                    return TR_CONDITION_TYPE_STATS_CITY_HEALTH;
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
static void ratings_advisors_go_to(int advisor)
{
    clear_state();
    window_advisors_show_advisor(advisor);
}
