#include "health.h"

#include "building/count.h"
#include "city/culture.h"
#include "city/health.h"
#include "city/houses.h"
#include "city/population.h"
#include "core/calc.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h" 

#define ADVISOR_HEIGHT 26

static unsigned int focus_button_id;
static int display_water_coverage = 0;

static void button_water_buildings(const generic_button *button);

static generic_button generic_buttons[] = {
    {32, 104, 80, 20, button_water_buildings, 0, 0},
    {112, 104, 80, 20, button_water_buildings, 0, 1}
};

static int get_health_advice(void)
{
    house_demands *demands = city_houses_demands();
    switch (demands->health) {
        case 1:
            return demands->requiring.bathhouse ? 1 : 0;
        case 2:
            return demands->requiring.barber ? 3 : 2;
        case 3:
            return demands->requiring.clinic ? 5 : 4;
        case 4:
            return 6;
        default:
            return 7;
    }
}

static void print_water_building_info(int y_offset, building_type type, int population_served)
{
    // total amount & building name
    int total_count = building_count_total(type);
    int group = 28;
    int index = type;
    if ((total_count != 1 && type != BUILDING_LATRINES) || (type == BUILDING_LATRINES && total_count == 1)) {
        group = CUSTOM_TRANSLATION;
        if (type == BUILDING_LATRINES) {
            index = TR_BUILDING_LATRINE;
        } else if (type == BUILDING_FOUNTAIN) {
            index = TR_BUILDING_FOUNTAINS;
        } else {
            index = TR_BUILDING_WELLS;
        }
    }

    int desc_offset_x = text_draw_number(total_count, ' ', " ", 40, y_offset, FONT_NORMAL_WHITE, 0);
    lang_text_draw(group, index, 40 + desc_offset_x, y_offset, FONT_NORMAL_WHITE);

    // working
    text_draw_number_centered(type == BUILDING_WELL ? total_count : building_count_active(type),
        180, y_offset, 100, FONT_NORMAL_WHITE);

    // care for
    int width = text_draw_number(population_served, '@', " ", 305, y_offset, FONT_NORMAL_WHITE, 0);
    lang_text_draw(58, 5, 305 + width, y_offset, FONT_NORMAL_WHITE);

    // city coverage
    text_draw_percentage_centered(calc_percentage(population_served, city_population()),
        440, y_offset, 160, FONT_NORMAL_WHITE);
}

static void print_health_building_info(int y_offset, building_type type, int population_served, int coverage)
{
    // total amount & building name
    static const int BUILDING_ID_TO_STRING_ID[] = { 28, 30, 24, 26 };
    
    lang_text_draw_amount(8, BUILDING_ID_TO_STRING_ID[type - BUILDING_DOCTOR],
        building_count_total(type), 40, y_offset, FONT_NORMAL_WHITE);
    // working
    text_draw_number_centered(building_count_active(type), 180, y_offset, 100, FONT_NORMAL_WHITE);

    // care for
    int width = text_draw_number(population_served, '@', " ", 305, y_offset, FONT_NORMAL_WHITE, 0);

    if (type == BUILDING_DOCTOR || type == BUILDING_HOSPITAL) {
        lang_text_draw(56, 6, 305 + width, y_offset, FONT_NORMAL_WHITE);
    } else {
        lang_text_draw(58, 5, 305 + width, y_offset, FONT_NORMAL_WHITE);
    }

    // city coverage
    if (coverage == 0) {
        lang_text_draw_centered(57, 10, 440, y_offset, 160, FONT_NORMAL_WHITE);
    } else if (coverage < 100) {
        lang_text_draw_centered(57, coverage / 10 + 11, 440, y_offset, 160, FONT_NORMAL_WHITE);
    } else {
        lang_text_draw_centered(57, 21, 440, y_offset, 160, FONT_NORMAL_WHITE);
    }
}

int window_advisor_health_get_rating_text_id(void)
{
    // the group id is 56
    return city_health() / 10 + 16;
}

static int draw_background(void)
{
    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    image_draw(image_group(GROUP_ADVISOR_ICONS) + 6, 10, 10, COLOR_MASK_NONE, SCALE_NONE);

    int sickness_level = city_health_get_global_sickness_level();

    lang_text_draw(56, 0, 60, 12, FONT_LARGE_BLACK); // City health

    int x_offset = lang_text_draw(CUSTOM_TRANSLATION, TR_ADVISOR_HEALTH_RATING, 60, 44, FONT_NORMAL_BLACK);
    text_draw_number(city_health(), 0, "", 60 + x_offset, 44, FONT_NORMAL_BLACK, 0);

    if (city_population() >= 200) {
        lang_text_draw_multiline(56, city_health() / 10 + 16, 60, 65, 560, FONT_NORMAL_BLACK);
    } else {
        lang_text_draw_multiline(56, 15, 60, 65, 560, FONT_NORMAL_BLACK);
    }
    lang_text_draw_centered(56, 3, 165, 110, 130, FONT_SMALL_PLAIN);    // Working
    lang_text_draw(56, 4, 312, 110, FONT_SMALL_PLAIN);                  // Care for
    lang_text_draw_centered(56, 5, 441, 110, 160, FONT_SMALL_PLAIN);    // City coverage
    
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_ADVISOR_HEALTH_HEALTH_COVERAGE, generic_buttons[0].x,
        generic_buttons[0].y + 5, generic_buttons[0].width, FONT_SMALL_PLAIN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_ADVISOR_HEALTH_WATER_COVERAGE, generic_buttons[1].x,
        generic_buttons[1].y + 5, generic_buttons[1].width, FONT_SMALL_PLAIN);

    inner_panel_draw(32, 124, 36, 5);

    int population = city_population();
    if (display_water_coverage) {
        int people_covered = city_health_get_population_with_well_access();
        print_water_building_info(128, BUILDING_WELL, people_covered);

        people_covered = city_health_get_population_with_latrines_access();
        print_water_building_info(148, BUILDING_LATRINES, people_covered);

        people_covered = city_health_get_population_with_water_access();
        print_water_building_info(168, BUILDING_FOUNTAIN, people_covered);
    } else {
        int people_covered = city_health_get_population_with_baths_access();
        print_health_building_info(128, BUILDING_BATHHOUSE, people_covered, calc_percentage(people_covered, population));

        people_covered = city_health_get_population_with_barber_access();
        print_health_building_info(148, BUILDING_BARBER, people_covered, calc_percentage(people_covered, population));

        people_covered = city_health_get_population_with_clinic_access();
        print_health_building_info(168, BUILDING_DOCTOR, people_covered, calc_percentage(people_covered, population));

        people_covered = 1000 * building_count_active(BUILDING_HOSPITAL);
        print_health_building_info(188, BUILDING_HOSPITAL, people_covered, city_culture_coverage_hospital());
    }
    int text_height = lang_text_draw_multiline(56, 7 + get_health_advice(), 45, 226, 560, FONT_NORMAL_BLACK);

    lang_text_draw(CUSTOM_TRANSLATION, TR_ADVISOR_HEALTH_SURVEILLANCE, 45, 246 + text_height, FONT_NORMAL_BLACK);
    text_height += 16;
    text_draw_multiline(translation_for(TR_ADVISOR_SICKNESS_LEVEL_LOW + sickness_level),
        45, 246 + text_height, 560, 0, FONT_NORMAL_BLACK, 0);

    return ADVISOR_HEIGHT;
}

static void draw_foreground(void)
{
    button_border_draw(generic_buttons[0].x, generic_buttons[0].y, generic_buttons[0].width, generic_buttons[0].height,
        focus_button_id == 1 && display_water_coverage);
    button_border_draw(generic_buttons[1].x, generic_buttons[1].y, generic_buttons[1].width, generic_buttons[1].height,
        focus_button_id == 2 && !display_water_coverage);
}

static int handle_mouse(const mouse *m)
{
    return generic_buttons_handle_mouse(m, 0, 0, generic_buttons, 2, &focus_button_id);
}

static void button_water_buildings(const generic_button *button)
{
    display_water_coverage = button->parameter1;
    window_invalidate();
}

const advisor_window_type *window_advisor_health(void)
{
    static const advisor_window_type window = {
        draw_background,
        draw_foreground,
        handle_mouse,
        0
    };
    return &window;
}
