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

    // info that is normally in the bottom
    /*



    text_draw(translation_for(TR_ADVISOR_TOTAL_HOUSING_CAPACITY), 320, y_offset + 220, FONT_NORMAL_GREEN, 0);
    text_draw_number(city_population_total_housing_capacity(), '@', " ", 500, y_offset + 220, FONT_NORMAL_WHITE, 0);
*/

    // the using tabel pottery etc...
    // for (unsigned int i = 0; i < list.size; i++) {
    //
    //     int image_id = resource_get_data(list.items[i])->image.icon;
    //     const image *img = image_get(image_id);
    //     int base_width = (26 - img->original.width) / 2;
    //     int base_height = (26 - img->original.height) / 2;
    //
    //     image_draw(resource_get_data(list.items[i])->image.icon, 54 + base_width, y_offset + 260 + (23 * i) + base_height - 5,
    //         COLOR_MASK_NONE, SCALE_NONE);
    //     text_draw(translation_for(TR_ADVISOR_RESIDENCES_USING_POTTERY + i), 90, y_offset + 263 + (23 * i),
    //         FONT_NORMAL_BLACK, 0);
    //     text_draw_number(houses_using_goods[list.items[i]], '@', " ", 499, y_offset + 263 + (23 * i),
    //         FONT_NORMAL_BLACK, 0);
    //     image_draw(resource_get_data(list.items[i])->image.icon, 550 + base_width, y_offset + 260 + (23 * i) + base_height - 5,
    //         COLOR_MASK_NONE, SCALE_NONE);
    // }
}

