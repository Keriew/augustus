#include "info.h"

#include "building/count.h"
#include "building/model.h"
#include "city/labor.h"
#include "city/population.h"
#include "core/image.h"
#include "game/resource.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#define SECTION_PADDING 8
#define LINE_HEIGHT 16

void draw_infopanel_background(int x_offset, int y_offset, int width, int height)
{
    int panel_blocks = height / BLOCK_SIZE;
    graphics_draw_line(x_offset, x_offset, y_offset, y_offset + height, COLOR_WHITE);
    graphics_draw_line(x_offset + width - 1, x_offset + width - 1, y_offset,
        y_offset + height, COLOR_SIDEBAR);
    inner_panel_draw(x_offset + 1, y_offset, width / BLOCK_SIZE, panel_blocks);
}

static int add_info_header(const uint8_t* header, const int x_offset, const int y_offset)
{
    text_draw(header, x_offset, y_offset, FONT_NORMAL_WHITE, 0);
    return y_offset + LINE_HEIGHT;
}

static int add_info_number(const int value, const int x_offset, const int y_offset)
{
    text_draw_number(value, '@', "", x_offset, y_offset, FONT_NORMAL_GREEN, 0);
    return y_offset + LINE_HEIGHT;
}

static int add_info_number_with_subsection(const int value, const int sub_value, const int x_offset, const int y_offset)
{
    const int text_width = text_draw_number(value,'@', "", x_offset, y_offset, FONT_NORMAL_GREEN, 0);
    text_draw_number(sub_value, '(', ")", x_offset + text_width, y_offset, FONT_NORMAL_GREEN, 0);

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

    const int text_width = text_draw_percentage(value, x_offset, y_offset, FONT_NORMAL_GREEN);

    text_draw_number(sub_value, '(', ")", x_offset +text_width, y_offset, FONT_NORMAL_GREEN, 0);

    y_offset += LINE_HEIGHT + SECTION_PADDING;
    return y_offset;
}

void draw_housing_table(int x_offset, int y_offset)
{
    x_offset += 5;
    int total_residences = 0;

    int housing_y_offset = y_offset + (6 * LINE_HEIGHT) + (3 * SECTION_PADDING);

    for (house_level level = HOUSE_MIN; level <= HOUSE_MAX; level++) {
        const int residences_at_level = building_count_active(BUILDING_HOUSE_SMALL_TENT + level);
        if (!residences_at_level) {
            continue;
        }
        total_residences += residences_at_level;

        add_info_number(residences_at_level, x_offset, housing_y_offset);
        housing_y_offset = add_info_header(
            lang_get_string(29, level),
            x_offset + 30,
            housing_y_offset
            );
    }

    y_offset = add_info_section_panel_percentage(
        lang_get_string(68, 148),
        city_labor_unemployment_percentage(),
        city_labor_workers_unemployed() - city_labor_workers_needed(),
        x_offset,
        y_offset
        );

    y_offset = add_info_section_panel(
        translation_for(TR_ADVISOR_TOTAL_NUM_HOUSES),
        total_residences,
        x_offset,
        y_offset
    );

    y_offset = add_info_section_panel_with_sub_value(
        translation_for(TR_ADVISOR_TOTAL_HOUSING_CAPACITY),
        city_population_total_housing_capacity(),
        city_population_open_housing_capacity(),
        x_offset,
        y_offset
    );
}

#define LARARIUM_COVERAGE 10
#define SHRINE_COVERAGE 50
#define SMALL_TEMPLE_COVERAGE 750
#define LARGE_TEMPLE_COVERAGE 3000
#define ORACLE_COVERAGE 500
#define LARGE_ORACLE_COVERAGE 750
#define PANTHEON_COVERAGE 1500
#define GRAND_TEMPLE_COVERAGE 5000

static int draw_god_row(god_type god, int x_offset, int y_offset, int base_god_value, building_type altar, building_type small_temple,
    building_type large_temple, building_type grand_temple)
{

    int value = base_god_value +
        SHRINE_COVERAGE * building_count_active(altar) +
        SMALL_TEMPLE_COVERAGE * building_count_active(small_temple) +
        LARGE_TEMPLE_COVERAGE * building_count_active(large_temple) +
        GRAND_TEMPLE_COVERAGE * building_count_active(grand_temple);

    y_offset = add_info_section_panel(
        lang_get_string(59,11 + god),
        value,
        x_offset,
        y_offset
        );

    return y_offset;

}

void draw_gods_table(int x_offset, int y_offset)
{
    x_offset += 5;

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

    // // god rows
    y_offset = draw_god_row(GOD_CERES, x_offset, y_offset, base_god_value, BUILDING_SHRINE_CERES, BUILDING_SMALL_TEMPLE_CERES,
         BUILDING_LARGE_TEMPLE_CERES, BUILDING_GRAND_TEMPLE_CERES);
    y_offset = draw_god_row(GOD_NEPTUNE, x_offset, y_offset, base_god_value, BUILDING_SHRINE_NEPTUNE, BUILDING_SMALL_TEMPLE_NEPTUNE,
        BUILDING_LARGE_TEMPLE_NEPTUNE, BUILDING_GRAND_TEMPLE_NEPTUNE);
    y_offset = draw_god_row(GOD_MERCURY, x_offset, y_offset, base_god_value, BUILDING_SHRINE_MERCURY, BUILDING_SMALL_TEMPLE_MERCURY,
        BUILDING_LARGE_TEMPLE_MERCURY, BUILDING_GRAND_TEMPLE_MERCURY);
    y_offset = draw_god_row(GOD_MARS, x_offset, y_offset, base_god_value, BUILDING_SHRINE_MARS, BUILDING_SMALL_TEMPLE_MARS,
        BUILDING_LARGE_TEMPLE_MARS, BUILDING_GRAND_TEMPLE_MARS);
    draw_god_row(GOD_VENUS, x_offset, y_offset, base_god_value, BUILDING_SHRINE_VENUS, BUILDING_SMALL_TEMPLE_VENUS,
        BUILDING_LARGE_TEMPLE_VENUS, BUILDING_GRAND_TEMPLE_VENUS);
}
