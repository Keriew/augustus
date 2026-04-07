#include "graphics/advisor_card_button_widget.h"
#include "graphics/ui_runtime.h"

extern "C" {
#include "ratings.h"

#include "city/ratings.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/lang.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "scenario/criteria.h"
#include "scenario/property.h"
}

#define ADVISOR_HEIGHT 27

static void button_rating(const generic_button *button);
static void draw_rating_widgets(void);

static generic_button rating_buttons[] = {
    { 80, 286, 110, 66, button_rating, 0, SELECTED_RATING_CULTURE},
    {200, 286, 110, 66, button_rating, 0, SELECTED_RATING_PROSPERITY},
    {320, 286, 110, 66, button_rating, 0, SELECTED_RATING_PEACE},
    {440, 286, 110, 66, button_rating, 0, SELECTED_RATING_FAVOR},
};

static unsigned int focus_button_id;

void draw_rating_column(int x_offset, int y_offset, int value, int has_reached)
{
    int image_base = image_group(GROUP_RATINGS_COLUMN);
    int y = y_offset - image_get(image_base)->height;
    int value_to_draw = value;
    if (has_reached && value < 25) {
        value_to_draw = 25;
    }

    image_draw(image_base, x_offset, y, COLOR_MASK_NONE, SCALE_NONE);
    for (int i = 0; i < 2 * value_to_draw; i++) {
        image_draw(image_base + 1, x_offset + 11, --y, COLOR_MASK_NONE, SCALE_NONE);
    }
    if (has_reached) {
        image_draw(image_base + 2, x_offset - 6, y, COLOR_MASK_NONE, SCALE_NONE);
    }
}

static int draw_background(void)
{
    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    image_draw(image_group(GROUP_ADVISOR_ICONS) + 3, 10, 10, COLOR_MASK_NONE, SCALE_NONE);
    int width = lang_text_draw(53, 0, 60, 12, FONT_LARGE_BLACK);
    if (!scenario_criteria_population_enabled() || scenario_is_open_play()) {
        lang_text_draw(53, 7, 80 + width, 17, FONT_NORMAL_BLACK);
    } else {
        width += lang_text_draw(53, 6, 80 + width, 17, FONT_NORMAL_BLACK);
        text_draw_number(scenario_criteria_population(), '@', ")", 80 + width, 17, FONT_NORMAL_BLACK, 0);
    }

    image_draw(image_group(GROUP_RATINGS_BACKGROUND), 60, 48, COLOR_MASK_NONE, SCALE_NONE);

    int open_play = scenario_is_open_play();

    // culture
    int culture = city_rating_culture();
    int has_culture_goal = !open_play && scenario_criteria_culture_enabled();
    draw_rating_column(110, 274, culture, !has_culture_goal || culture >= scenario_criteria_culture());
    // prosperity
    int prosperity = city_rating_prosperity();
    int has_prosperity_goal = !open_play && scenario_criteria_prosperity_enabled();
    draw_rating_column(230, 274, prosperity, !has_prosperity_goal || prosperity >= scenario_criteria_prosperity());

    // peace
    int peace = city_rating_peace();
    int has_peace_goal = !open_play && scenario_criteria_peace_enabled();
    draw_rating_column(350, 274, peace, !has_peace_goal || peace >= scenario_criteria_peace());

    // favor
    int favor = city_rating_favor();
    int has_favor_goal = !open_play && scenario_criteria_favor_enabled();
    draw_rating_column(470, 274, favor, !has_favor_goal || favor >= scenario_criteria_favor());

    // bottom info box
    inner_panel_draw(64, 356, 32, 4);
    switch (city_rating_selected()) {
        case SELECTED_RATING_CULTURE:
            lang_text_draw(53, 1, 72, 359, FONT_NORMAL_WHITE);
            if (culture <= 90) {
                lang_text_draw_multiline(53, 9 + city_rating_explanation_for(SELECTED_RATING_CULTURE),
                    72, 374, 496, FONT_NORMAL_WHITE);
            } else {
                lang_text_draw_multiline(53, 50, 72, 374, 496, FONT_NORMAL_WHITE);
            }
            break;
        case SELECTED_RATING_PROSPERITY:
        {
            int line_width;
            lang_text_draw(53, 2, 72, 359, FONT_NORMAL_WHITE);
            if (prosperity <= 90) {
                line_width = lang_text_draw_multiline(53, 16 + city_rating_explanation_for(SELECTED_RATING_PROSPERITY),
                    72, 374, 496, FONT_NORMAL_WHITE);
            } else {
                line_width = lang_text_draw_multiline(53, 51, 72, 374, 496, FONT_NORMAL_WHITE);
            }
            if (config_get(CONFIG_UI_SHOW_MAX_PROSPERITY)) {
                int max = calc_bound(city_ratings_prosperity_max(), 0, 100);
                if (prosperity < max) {
                    width = lang_text_draw(CUSTOM_TRANSLATION, TR_ADVISOR_MAX_ATTAINABLE_PROSPERITY_IS, 72, 374 + line_width, FONT_NORMAL_WHITE);
                    text_draw_number(max, 0, ".", 72 + width, 374 + line_width, FONT_NORMAL_WHITE, 0);
                }
            }
            break;
        }
        case SELECTED_RATING_PEACE:
            lang_text_draw(53, 3, 72, 359, FONT_NORMAL_WHITE);
            if (peace <= 90) {
                lang_text_draw_multiline(53, 41 + city_rating_explanation_for(SELECTED_RATING_PEACE),
                    72, 374, 496, FONT_NORMAL_WHITE);
            } else {
                lang_text_draw_multiline(53, 52, 72, 374, 496, FONT_NORMAL_WHITE);
            }
            break;
        case SELECTED_RATING_FAVOR:
            lang_text_draw(53, 4, 72, 359, FONT_NORMAL_WHITE);
            if (favor <= 90) {
                lang_text_draw_multiline(53, 27 + city_rating_explanation_for(SELECTED_RATING_FAVOR),
                    72, 374, 496, FONT_NORMAL_WHITE);
            } else {
                lang_text_draw_multiline(53, 53, 72, 374, 496, FONT_NORMAL_WHITE);
            }
            break;
        default:
            lang_text_draw_centered(53, 8, 72, 380, 496, FONT_NORMAL_WHITE);
            break;
    }

    return ADVISOR_HEIGHT;
}

static void draw_foreground(void)
{
    draw_rating_widgets();
}

static void draw_rating_widgets(void)
{
    UiPrimitives &primitives = shared_ui_runtime().primitives();
    const int open_play = scenario_is_open_play();

    const struct {
        int x;
        int title_id;
        int value;
        int goal_enabled;
        int goal_value;
        int focus_id;
    } cards[] = {
        {80, 1, city_rating_culture(), !open_play && scenario_criteria_culture_enabled(), scenario_criteria_culture(), SELECTED_RATING_CULTURE},
        {200, 2, city_rating_prosperity(), !open_play && scenario_criteria_prosperity_enabled(), scenario_criteria_prosperity(), SELECTED_RATING_PROSPERITY},
        {320, 3, city_rating_peace(), !open_play && scenario_criteria_peace_enabled(), scenario_criteria_peace(), SELECTED_RATING_PEACE},
        {440, 4, city_rating_favor(), !open_play && scenario_criteria_favor_enabled(), scenario_criteria_favor(), SELECTED_RATING_FAVOR},
    };

    for (const auto &card : cards) {
        UiTextSpec texts[3] = {};

        texts[0].content_type = UiTextContentType::Language;
        texts[0].alignment = UiTextAlignment::Center;
        texts[0].text_group = 53;
        texts[0].text_id = card.title_id;
        texts[0].x = card.x;
        texts[0].y = 294;
        texts[0].box_width = 110;
        texts[0].font = FONT_NORMAL_BLACK;

        texts[1].content_type = UiTextContentType::Number;
        texts[1].alignment = UiTextAlignment::Center;
        texts[1].value = card.value;
        texts[1].x = card.x;
        texts[1].y = 309;
        texts[1].box_width = 100;
        texts[1].font = FONT_LARGE_BLACK;

        texts[2].content_type = UiTextContentType::Language;
        texts[2].alignment = UiTextAlignment::Left;
        texts[2].text_group = 53;
        texts[2].text_id = 5;
        texts[2].x = card.x + 5 + text_get_number_width(card.goal_enabled ? card.goal_value : 0, '@', " ", FONT_NORMAL_BLACK);
        texts[2].y = 334;
        texts[2].font = FONT_NORMAL_BLACK;

        AdvisorCardButtonWidget(
            primitives,
            card.x,
            286,
            110,
            66,
            focus_button_id == static_cast<unsigned int>(card.focus_id),
            texts,
            3)
            .draw();

        UiTextSpec goal_number = {};
        goal_number.content_type = UiTextContentType::Number;
        goal_number.alignment = UiTextAlignment::Left;
        goal_number.value = card.goal_enabled ? card.goal_value : 0;
        goal_number.prefix = '@';
        goal_number.postfix = " ";
        goal_number.x = card.x + 5;
        goal_number.y = 334;
        goal_number.font = FONT_NORMAL_BLACK;
        UiTextPrimitive(primitives, goal_number).draw();
    }
}

static int handle_mouse(const mouse *m)
{
    return generic_buttons_handle_mouse(m, 0, 0, rating_buttons, 4, &focus_button_id);
}

static void button_rating(const generic_button *button)
{
    const selected_rating rating = static_cast<selected_rating>(button->parameter1);
    city_rating_select(rating);
    window_invalidate();
}

static void get_tooltip_text(advisor_tooltip_result *r)
{
    switch (focus_button_id) {
        case SELECTED_RATING_CULTURE:
            r->text_id = 102;
            break;
        case SELECTED_RATING_PROSPERITY:
            r->text_id = 103;
            break;
        case SELECTED_RATING_PEACE:
            r->text_id = 104;
            break;
        case SELECTED_RATING_FAVOR:
            r->text_id = 105;
            break;
    }
}

const advisor_window_type *window_advisor_ratings(void)
{
    static const advisor_window_type window = {
        draw_background,
        draw_foreground,
        handle_mouse,
        get_tooltip_text
    };
    return &window;
}
