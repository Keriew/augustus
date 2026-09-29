#include "financial.h"

#include "city/data_private.h"
#include "city/finance.h"
#include "core/calc.h"
#include "core/lang.h"
#include "graphics/arrow_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "translation/translation.h"
#include "widget/dropdown_button.h"

#define ADVISOR_HEIGHT 27
#define FINANCE_YEAR_DROPDOWN_COUNT 9
#define FINANCE_YEAR_DROPDOWN_HEIGHT 20
#define FINANCE_YEAR_DROPDOWN_LEFT_CENTER 300
#define FINANCE_YEAR_DROPDOWN_RIGHT_CENTER 440

static void button_change_taxes(int is_down, int param2);
static void setup_year_dropdowns(void);
static void update_year_dropdowns(void);
static void year_dropdown_selected(dropdown_button *dd);
static int dropdown_to_years_ago(int selected_index);
static int overview_misc_income(const finance_overview *overview, int years_ago);

static arrow_button arrow_buttons_taxes[] = {
    {180, 75, 17, 24, button_change_taxes, 1, 0},
    {204, 75, 15, 24, button_change_taxes, 0, 0}
};

static unsigned int arrow_button_focus;
static dropdown_button year_dropdowns[2];
static int year_dropdowns_initialized;
static int left_years_ago = 1;
static int right_years_ago = 0;

static void draw_row(int group, int number, int y, int value_left, int value_right)
{
    lang_text_draw(group, number, 80, y, FONT_NORMAL_BLACK);
    text_draw_number_finances(value_left, 350, y, FONT_NORMAL_BLACK, 0);
    text_draw_number_finances(value_right, 490, y, FONT_NORMAL_BLACK, 0);
}

static void draw_tr_row(int tr_string, int y, int value_left, int value_right)
{
    text_draw(translation_for(tr_string), 80, y, FONT_NORMAL_BLACK, 0);
    text_draw_number_finances(value_left, 350, y, FONT_NORMAL_BLACK, 0);
    text_draw_number_finances(value_right, 490, y, FONT_NORMAL_BLACK, 0);
}

static int draw_background(void)
{
    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    image_draw(image_group(GROUP_ADVISOR_ICONS) + 10, 10, 10, COLOR_MASK_NONE, SCALE_NONE);

    lang_text_draw(60, 0, 60, 12, FONT_LARGE_BLACK);
    inner_panel_draw(64, 48, 34, 5);

    int treasury = city_finance_treasury();
    const finance_overview *left_year = city_finance_overview_for_year(left_years_ago);
    const finance_overview *right_year = city_finance_overview_for_year(right_years_ago);
    if (!left_year) {
        left_year = city_finance_overview_last_year();
    }
    if (!right_year) {
        right_year = city_finance_overview_this_year();
    }

    int width;
    if (treasury < 0) {
        width = lang_text_draw(60, 3, 70, 58, FONT_NORMAL_RED);
        lang_text_draw_amount(8, 0, -treasury, 72 + width, 58, FONT_NORMAL_RED);
    } else {
        width = lang_text_draw(60, 2, 70, 58, FONT_NORMAL_WHITE);
        lang_text_draw_amount(8, 0, treasury, 72 + width, 58, FONT_NORMAL_WHITE);
    }

    // tax percentage and estimated income
    lang_text_draw(60, 1, 70, 81, FONT_NORMAL_WHITE);
    width = text_draw_percentage(city_finance_tax_percentage(), 240, 81, FONT_NORMAL_WHITE);
    width += lang_text_draw(60, 4, 240 + width, 81, FONT_NORMAL_WHITE);
    lang_text_draw_amount(8, 0, city_finance_estimated_tax_income(), 240 + width, 81, FONT_NORMAL_WHITE);

    // percentage taxpayers
    width = text_draw_percentage(city_finance_percentage_taxed_people(), 70, 103, FONT_NORMAL_WHITE);
    lang_text_draw(60, 5, 70 + width, 103, FONT_NORMAL_WHITE);

    // income
    draw_row(60, 8, 155, left_year->income.taxes, right_year->income.taxes);
    draw_row(60, 9, 170, left_year->income.exports, right_year->income.exports);
    draw_tr_row(TR_WINDOW_ADVISOR_TOURISM, 185,
        overview_misc_income(left_year, left_years_ago), overview_misc_income(right_year, right_years_ago));
    draw_row(60, 20, 200, left_year->income.donated, right_year->income.donated);

    graphics_draw_line(280, 350, 213, 213, COLOR_BLACK);
    graphics_draw_line(420, 490, 213, 213, COLOR_BLACK);

    draw_row(60, 10, 218, left_year->income.total, right_year->income.total);

    // expenses

    draw_row(60, 11, 242, left_year->expenses.imports, right_year->expenses.imports);
    draw_row(60, 12, 257, left_year->expenses.wages, right_year->expenses.wages);
    draw_row(60, 13, 272, left_year->expenses.construction, right_year->expenses.construction);
    draw_tr_row(TR_ADVISOR_FINANCE_LEVIES, 287, left_year->expenses.levies, right_year->expenses.levies);

    draw_row(60, 15, 302, left_year->expenses.salary, right_year->expenses.salary);
    draw_row(60, 16, 317, left_year->expenses.sundries, right_year->expenses.sundries);
    draw_tr_row(TR_WINDOW_ADVISOR_FINANCE_INTEREST_TRIBUTE, 332,
        left_year->expenses.tribute + left_year->expenses.interest,
        right_year->expenses.tribute + right_year->expenses.interest);

    graphics_draw_line(280, 350, 345, 345, COLOR_BLACK);
    graphics_draw_line(420, 490, 345, 345, COLOR_BLACK);

    draw_row(60, 17, 350, left_year->expenses.total, right_year->expenses.total);
    draw_row(60, 18, 373, left_year->net_in_out, right_year->net_in_out);
    draw_row(60, 19, 396, left_year->balance, right_year->balance);

    return ADVISOR_HEIGHT;
}

static void draw_foreground(void)
{
    setup_year_dropdowns();
    update_year_dropdowns();
    arrow_buttons_draw(0, 0, arrow_buttons_taxes, 2);
    dropdown_button_draw_array(year_dropdowns, 2);
}

static int handle_mouse(const mouse *m)
{
    setup_year_dropdowns();
    update_year_dropdowns();
    if (dropdown_button_handle_mouse_array(year_dropdowns, m, 2)) {
        return 1;
    }
    return arrow_buttons_handle_mouse(m, 0, 0, arrow_buttons_taxes, 2, &arrow_button_focus);
}

static void button_change_taxes(int is_down, int param2)
{
    city_finance_change_tax_percentage(is_down ? -1 : 1);
    city_finance_estimate_taxes();
    city_finance_calculate_totals();
    window_invalidate();
}

static void setup_year_dropdowns(void)
{
    if (year_dropdowns_initialized) {
        return;
    }

    static lang_fragment year_fragments[FINANCE_YEAR_DROPDOWN_COUNT] = { 0 };

    for (int i = 0; i < 3; i++) {
        year_fragments[i].type = LANG_FRAG_LABEL;
        year_fragments[i].text_group = CUSTOM_TRANSLATION;
    }
    year_fragments[0].text_id = TR_UI_SELECT_TRADE_LEDGER_YEAR;
    year_fragments[1].text_id = TR_UI_CURRENT_YEAR;
    year_fragments[2].text_id = TR_UI_LAST_YEAR;

    for (int i = 3; i < FINANCE_YEAR_DROPDOWN_COUNT; i++) {
        year_fragments[i].type = LANG_FRAG_AMOUNT;
        year_fragments[i].text_group = CUSTOM_TRANSLATION;
        year_fragments[i].text_id = TR_UI_YEAR_AGO;
        year_fragments[i].number = i - 1;
    }

    dropdown_button_init_simple(0, 0, 0, FINANCE_YEAR_DROPDOWN_HEIGHT,
        year_fragments, FINANCE_YEAR_DROPDOWN_COUNT, &year_dropdowns[0], DD_BUTTON_STYLE_DEFAULT, 0);
    dropdown_button_init_simple(0, 0, 0, FINANCE_YEAR_DROPDOWN_HEIGHT,
        year_fragments, FINANCE_YEAR_DROPDOWN_COUNT, &year_dropdowns[1], DD_BUTTON_STYLE_DEFAULT, 0);

    year_dropdowns[0].show_origin = 1;
    year_dropdowns[1].show_origin = 1;
    year_dropdowns[0].selected_index = 2;
    year_dropdowns[1].selected_index = 1;
    year_dropdowns[0].selected_callback = year_dropdown_selected;
    year_dropdowns[1].selected_callback = year_dropdown_selected;
    year_dropdowns_initialized = 1;
}

static void update_year_dropdowns(void)
{
    int available_years = city_finance_overview_years_stored() + 2;
    if (available_years > FINANCE_YEAR_DROPDOWN_COUNT - 1) {
        available_years = FINANCE_YEAR_DROPDOWN_COUNT - 1;
    }

    for (int dd = 0; dd < 2; dd++) {
        if (year_dropdowns[dd].selected_index > available_years) {
            year_dropdowns[dd].selected_index = available_years;
        }
        for (int i = 1; i < FINANCE_YEAR_DROPDOWN_COUNT; i++) {
            int unavailable = i > available_years;
            year_dropdowns[dd].buttons[i].is_disabled = unavailable;
            year_dropdowns[dd].buttons[i].is_hidden = unavailable;
        }
    }

    left_years_ago = dropdown_to_years_ago(year_dropdowns[0].selected_index);
    right_years_ago = dropdown_to_years_ago(year_dropdowns[1].selected_index);

    dropdown_button_update_dimensions(
        FINANCE_YEAR_DROPDOWN_LEFT_CENTER - dropdown_button_get_width(&year_dropdowns[0]) / 2,
        130, 0, FINANCE_YEAR_DROPDOWN_HEIGHT, &year_dropdowns[0]);
    dropdown_button_update_dimensions(
        FINANCE_YEAR_DROPDOWN_RIGHT_CENTER - dropdown_button_get_width(&year_dropdowns[1]) / 2,
        130, 0, FINANCE_YEAR_DROPDOWN_HEIGHT, &year_dropdowns[1]);
}

static void year_dropdown_selected(dropdown_button *dd)
{
    if (dd == &year_dropdowns[0]) {
        left_years_ago = dropdown_to_years_ago(dd->selected_index);
    } else if (dd == &year_dropdowns[1]) {
        right_years_ago = dropdown_to_years_ago(dd->selected_index);
    }
    window_invalidate();
}

static int dropdown_to_years_ago(int selected_index)
{
    if (selected_index <= 1) {
        return 0;
    }
    return selected_index - 1;
}

static int overview_misc_income(const finance_overview *overview, int years_ago)
{
    if (years_ago <= 0) {
        return city_data.finance.misc_this_year;
    }
    if (years_ago == 1) {
        return city_data.finance.misc_last_year;
    }
    return overview->income.total - overview->income.taxes - overview->income.exports - overview->income.donated;
}

static void get_tooltip_text(advisor_tooltip_result *r)
{
    if (arrow_button_focus) {
        r->text_id = 120;
    }
}

const advisor_window_type *window_advisor_financial(void)
{
    static const advisor_window_type window = {
        draw_background,
        draw_foreground,
        handle_mouse,
        get_tooltip_text
    };
    return &window;
}
