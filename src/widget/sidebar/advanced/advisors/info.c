#include "info.h"

#include "assets/assets.h"
#include "building/count.h"
#include "building/model.h"
#include "city/culture.h"
#include "city/festival.h"
#include "city/gods.h"
#include "city/health.h"
#include "city/labor.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/image.h"
#include "game/resource.h"
#include "graphics/complex_button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "window/advisors.h"
#include "window/city.h"
#include "window/message_dialog.h"

#define SECTION_PADDING 8
#define LINE_HEIGHT 16
#define INFO_PANEL_HEIGHT 494
#define INFO_PANEL_WIDTH 160
#define INFO_PANEL_LEFT_MARGIN 5
#define INFO_PANEL_TOP_MARGIN 6
#define LARARIUM_COVERAGE 10
#define SHRINE_COVERAGE 50
#define SMALL_TEMPLE_COVERAGE 750
#define LARGE_TEMPLE_COVERAGE 3000
#define ORACLE_COVERAGE 500
#define LARGE_ORACLE_COVERAGE 750
#define PANTHEON_COVERAGE 1500
#define GRAND_TEMPLE_COVERAGE 5000
#define HEADER_FONT FONT_NORMAL_WHITE
#define VALUE_FONT FONT_NORMAL_GREEN
#define IMAGE_ID_FOR_ADVISOR_BUILDING_WINDOW_BORDER_1_UI 11842
#define BUTTON_COUNT 2

static void button_advisor(int advisor, int param2);
static void button_help(int param1, int param2);

static image_button health_buttons[] = {
    { 0, 0, 27, 27, IB_NORMAL, GROUP_CONTEXT_ICONS, 0,button_help, button_none, 0, 0, 1},
    {0, 0, 27, 27, IB_NORMAL,0, 0, button_advisor, button_none, 0, 0, 1}
};

static struct {
    int x_offset;
    int y_offset;
    unsigned int focus_button_id;
    int current_advisor;
} data;


void draw_info_panel_background(const int x_offset, const int y_offset)
{
    const int panel_blocks = INFO_PANEL_HEIGHT / BLOCK_SIZE;
    graphics_draw_line(x_offset, x_offset, y_offset, y_offset + INFO_PANEL_HEIGHT, COLOR_WHITE);
    graphics_draw_line(x_offset + INFO_PANEL_WIDTH - 1, x_offset + INFO_PANEL_WIDTH - 1, y_offset,
        y_offset + INFO_PANEL_HEIGHT, COLOR_SIDEBAR);
    inner_panel_draw(x_offset + 1, y_offset, INFO_PANEL_WIDTH / BLOCK_SIZE, panel_blocks);

    data.x_offset = x_offset + INFO_PANEL_LEFT_MARGIN;
    data.y_offset = y_offset + INFO_PANEL_TOP_MARGIN;
}

static int add_info_header(const uint8_t* header, const int x_offset, const int y_offset)
{
    text_draw(header, x_offset, y_offset, HEADER_FONT, 0);
    return y_offset + LINE_HEIGHT;
}

static int add_info_number(const int value, const int x_offset, const int y_offset)
{
    text_draw_number(value, '@', "", x_offset, y_offset, VALUE_FONT, 0);
    return y_offset + LINE_HEIGHT;
}

static int add_info_number_with_subsection(const int value, const int sub_value, const int x_offset, const int y_offset)
{
    const int text_width = text_draw_number(value,'@', "", x_offset, y_offset, VALUE_FONT, 0);
    text_draw_number(sub_value, '(', ")", x_offset + text_width, y_offset, VALUE_FONT, 0);

    return y_offset + LINE_HEIGHT;
}

static int add_info_section_panel(const uint8_t* header, const int value, const int x_offset, int y_offset)
{
    y_offset = add_info_header(header, x_offset, y_offset);
    y_offset = add_info_number(value, x_offset, y_offset);
    return y_offset + SECTION_PADDING;
}

static int add_info_section_panel_with_sub_value(const uint8_t* header, const int value, const int sub_value, const int x_offset, int y_offset)
{
    y_offset = add_info_header(header, x_offset, y_offset);
    y_offset = add_info_number_with_subsection(value, sub_value, x_offset, y_offset);
    return y_offset + SECTION_PADDING;
}

static int add_info_section_panel_percentage(const uint8_t* header, const int value, const int sub_value, const int x_offset, int y_offset)
{
    y_offset = add_info_header(header, x_offset, y_offset);

    const int text_width = text_draw_percentage(value, x_offset, y_offset, VALUE_FONT);

    text_draw_number(sub_value, '(', ")", x_offset +text_width, y_offset, VALUE_FONT, 0);

    y_offset += LINE_HEIGHT + SECTION_PADDING;
    return y_offset;
}

int info_panel_mouse_handle(const mouse *m)
{
    return image_buttons_handle_mouse(m, 0,0, health_buttons, BUTTON_COUNT, &data.focus_button_id);
}

void place_button_at_bottom(const int advisor, const int health_information_dialog)
{
    health_buttons[0].y_offset = data.y_offset + 447;
    health_buttons[0].x_offset = data.x_offset + 2;
    health_buttons[0].parameter1 = health_information_dialog;

    health_buttons[1].image_offset = IMAGE_ID_FOR_ADVISOR_BUILDING_WINDOW_BORDER_1_UI + advisor * 4;
    health_buttons[1].y_offset = data.y_offset + 447;
    health_buttons[1].x_offset = data.x_offset + 29;
    health_buttons[1].parameter1 = advisor;

    image_buttons_draw(0,0, health_buttons, BUTTON_COUNT);
}

static int draw_health_building_info(const int x_offset, int y_offset, const building_type type, const int population_served, const int coverage)
{
    static const int BUILDING_ID_TO_STRING_ID[] = { 28, 30, 24, 26 };

    lang_text_draw_amount(8, BUILDING_ID_TO_STRING_ID[type - BUILDING_DOCTOR],
        building_count_total(type), x_offset, y_offset, HEADER_FONT);

    y_offset += LINE_HEIGHT;

    if (coverage == 0) {
        lang_text_draw(57, 10, x_offset, y_offset, VALUE_FONT);
    } else if (coverage < 100) {
        lang_text_draw(57, coverage / 10 + 11, x_offset, y_offset,  VALUE_FONT);
    } else {
        lang_text_draw(57, 21, x_offset, y_offset,  VALUE_FONT);
    }

    y_offset += LINE_HEIGHT;

    const int width = text_draw_number(population_served, '@', " ", x_offset, y_offset, VALUE_FONT, 0);

    if (type == BUILDING_DOCTOR || type == BUILDING_HOSPITAL) {
        lang_text_draw(56, 6, x_offset + width, y_offset, VALUE_FONT);
    } else {
        lang_text_draw(58, 5, x_offset + width, y_offset, VALUE_FONT);
    }

    y_offset += LINE_HEIGHT + SECTION_PADDING;
    return y_offset;
}

void draw_health_table(void)
{
    place_button_at_bottom(ADVISOR_HEALTH, MESSAGE_DIALOG_ADVISOR_HEALTH);

    int y_offset = data.y_offset;
    const int population = city_population();

    int people_covered = city_health_get_population_with_baths_access();
    y_offset = draw_health_building_info(data.x_offset, y_offset, BUILDING_BATHHOUSE, people_covered, calc_percentage(people_covered, population));

    people_covered = city_health_get_population_with_barber_access();
    y_offset = draw_health_building_info(data.x_offset, y_offset, BUILDING_BARBER, people_covered, calc_percentage(people_covered, population));

    people_covered = city_health_get_population_with_clinic_access();
    y_offset = draw_health_building_info(data.x_offset, y_offset, BUILDING_DOCTOR, people_covered, calc_percentage(people_covered, population));

    people_covered = 1000 * building_count_active(BUILDING_HOSPITAL);
    y_offset = draw_health_building_info(data.x_offset, y_offset, BUILDING_HOSPITAL, people_covered, city_culture_coverage_hospital());
}

static int draw_education_section(const int building_type, const int person_coverage, const int population, const int coverage, int y_offset)
{
    lang_text_draw_amount(8, 18, building_count_total(building_type), data.x_offset, y_offset, FONT_NORMAL_WHITE);
    y_offset += LINE_HEIGHT;

    int number = 10;
    if (coverage == 0) {
        number = 10;
    } else if (coverage < 100) {
        number = coverage / 10 + 11;
    } else {
        number = 21;
    }

    lang_text_draw(57, number, data.x_offset, y_offset, VALUE_FONT);
    y_offset += LINE_HEIGHT;

    y_offset = add_info_number_with_subsection(
        person_coverage,
        population,
        data.x_offset,
        y_offset
    );

    y_offset += LINE_HEIGHT;

    return y_offset;
}

void draw_education_table(void)
{
    place_button_at_bottom(ADVISOR_EDUCATION, MESSAGE_DIALOG_ADVISOR_EDUCATION);

    int y_offset = data.y_offset;

    y_offset = draw_education_section(BUILDING_SCHOOL, city_culture_get_school_person_coverage(),
        city_population_school_age(), city_culture_coverage_school(), y_offset);

    y_offset = draw_education_section(BUILDING_ACADEMY, city_culture_get_academy_person_coverage(),
       city_population_academy_age(), city_culture_coverage_academy(), y_offset);

    draw_education_section(BUILDING_LIBRARY, city_culture_get_library_person_coverage(),
        city_population(), city_culture_coverage_library(), y_offset);
}

void draw_housing_table(void)
{
    place_button_at_bottom(ADVISOR_HOUSING, MESSAGE_DIALOG_ADVISOR_POPULATION);

    int y_offset = data.y_offset;

    int total_residences = 0;

    int housing_y_offset = y_offset + (6 * LINE_HEIGHT) + (3 * SECTION_PADDING);

    for (house_level level = HOUSE_MIN; level <= HOUSE_MAX; level++) {
        const int residences_at_level = building_count_active(BUILDING_HOUSE_SMALL_TENT + level);
        if (!residences_at_level) {
            continue;
        }
        total_residences += residences_at_level;

        add_info_number(residences_at_level, data.x_offset, housing_y_offset);
        housing_y_offset = add_info_header(
            lang_get_string(29, level),
            data.x_offset + 30,
            housing_y_offset
            );
    }

    y_offset = add_info_section_panel_percentage(
        lang_get_string(68, 148),
        city_labor_unemployment_percentage(),
        city_labor_workers_unemployed() - city_labor_workers_needed(),
        data.x_offset,
        y_offset
        );

    y_offset = add_info_section_panel(
        translation_for(TR_ADVISOR_TOTAL_NUM_HOUSES),
        total_residences,
        data.x_offset,
        y_offset
    );

    y_offset = add_info_section_panel_with_sub_value(
        translation_for(TR_ADVISOR_TOTAL_HOUSING_CAPACITY),
        city_population_total_housing_capacity(),
        city_population_open_housing_capacity(),
        data.x_offset,
        y_offset
    );
}

static int draw_god_row(const god_type god, int y_offset, const int base_god_value,
    const building_type altar, const building_type small_temple,
    const building_type large_temple, const building_type grand_temple)
{

    const int value = base_god_value +
        SHRINE_COVERAGE * building_count_active(altar) +
        SMALL_TEMPLE_COVERAGE * building_count_active(small_temple) +
        LARGE_TEMPLE_COVERAGE * building_count_active(large_temple) +
        GRAND_TEMPLE_COVERAGE * building_count_active(grand_temple);

    const int god_name_width =  text_draw(lang_get_string(59,11 + god)
        , data.x_offset,
        y_offset, HEADER_FONT, 0);

    const int bolts = city_god_wrath_bolts(god);
    for (int i = 0; i < bolts / 10; i++) {
        image_draw(image_group(GROUP_GOD_BOLT),
            data.x_offset + 10 * i + god_name_width, y_offset -2 , COLOR_MASK_NONE, SCALE_NONE);
    }
    const int happy_bolts = city_god_happy_bolts(god);
    for (int i = 0; i < happy_bolts; i++) {
        image_draw(assets_get_image_id("UI", "Happy God Icon"),
            data.x_offset + 10 * i + god_name_width, y_offset -2 , COLOR_MASK_NONE, SCALE_NONE);
    }

    y_offset += LINE_HEIGHT;
    y_offset = add_info_number(
        value,
        data.x_offset,
        y_offset);

    lang_text_draw(59, 32 + city_god_happiness(god) / 10, data.x_offset, y_offset, VALUE_FONT);

    y_offset += LINE_HEIGHT + SECTION_PADDING;

    return y_offset;

}

void draw_gods_table(void)
{
    place_button_at_bottom(ADVISOR_RELIGION, MESSAGE_DIALOG_ADVISOR_RELIGION);

    int y_offset = data.y_offset;

    const int oracles = building_count_active(BUILDING_ORACLE);
    const int larariums = building_count_total(BUILDING_LARARIUM);
    const int nymphaeums = building_count_active(BUILDING_NYMPHAEUM);
    const int small_mausoleums = building_count_active(BUILDING_SMALL_MAUSOLEUM);
    const int large_mausoleums = building_count_active(BUILDING_LARGE_MAUSOLEUM);

    const int base_god_value =
       PANTHEON_COVERAGE * building_count_active(BUILDING_PANTHEON) +
       LARARIUM_COVERAGE * larariums +
       ORACLE_COVERAGE * oracles +
       ORACLE_COVERAGE * small_mausoleums +
       LARGE_ORACLE_COVERAGE * nymphaeums +
       LARGE_ORACLE_COVERAGE * large_mausoleums ;

    y_offset = draw_god_row(GOD_CERES, y_offset, base_god_value, BUILDING_SHRINE_CERES,
        BUILDING_SMALL_TEMPLE_CERES, BUILDING_LARGE_TEMPLE_CERES, BUILDING_GRAND_TEMPLE_CERES);
    y_offset = draw_god_row(GOD_NEPTUNE, y_offset, base_god_value, BUILDING_SHRINE_NEPTUNE,
        BUILDING_SMALL_TEMPLE_NEPTUNE, BUILDING_LARGE_TEMPLE_NEPTUNE, BUILDING_GRAND_TEMPLE_NEPTUNE);
    y_offset = draw_god_row(GOD_MERCURY, y_offset, base_god_value, BUILDING_SHRINE_MERCURY,
        BUILDING_SMALL_TEMPLE_MERCURY, BUILDING_LARGE_TEMPLE_MERCURY, BUILDING_GRAND_TEMPLE_MERCURY);
    y_offset = draw_god_row(GOD_MARS, y_offset, base_god_value, BUILDING_SHRINE_MARS,
        BUILDING_SMALL_TEMPLE_MARS, BUILDING_LARGE_TEMPLE_MARS, BUILDING_GRAND_TEMPLE_MARS);
    draw_god_row(GOD_VENUS, y_offset, base_god_value, BUILDING_SHRINE_VENUS,
        BUILDING_SMALL_TEMPLE_VENUS, BUILDING_LARGE_TEMPLE_VENUS, BUILDING_GRAND_TEMPLE_VENUS);
}

static void button_advisor(const int advisor, int param2)
{
    window_advisors_show_advisor(advisor);
}

static void button_help(const int param1, int param2)
{
    window_message_dialog_show(param1, 0);
}

