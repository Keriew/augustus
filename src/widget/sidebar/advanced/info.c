#include "info.h"

#include "building/count.h"
#include "building/model.h"
#include "city/population.h"
#include "core/image.h"
#include "game/resource.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"


void draw_infopanel_background(int x_offset, int y_offset, int width, int height)
{
    int panel_blocks = height / BLOCK_SIZE;
    graphics_draw_line(x_offset, x_offset, y_offset, y_offset + height, COLOR_WHITE);
    graphics_draw_line(x_offset + width - 1, x_offset + width - 1, y_offset,
        y_offset + height, COLOR_SIDEBAR);
    inner_panel_draw(x_offset + 1, y_offset, width / BLOCK_SIZE, panel_blocks);

}

void draw_housing_table(int x, int y_offset)
{
    int rows = 0;

    resource_list list = { 0 };
    for (resource_type r = RESOURCE_MIN_NON_FOOD; r < RESOURCE_MAX_NON_FOOD; r++) {
        if (resource_is_inventory(r)) {
            list.items[list.size++] = r;
        }
    }

    int total_residences = 0;
    int houses_using_goods[RESOURCE_MAX] = { 0 };

    for (house_level level = HOUSE_MIN; level <= HOUSE_MAX; level++) {
        int residences_at_level = building_count_active(BUILDING_HOUSE_SMALL_TENT + level);
        if (!residences_at_level) {
            continue;
        }
        total_residences += residences_at_level;

        for (unsigned int i = 0; i < list.size; i++) {
            if (model_house_uses_inventory(level, list.items[i])) {
                houses_using_goods[list.items[i]] += residences_at_level;
            }
        }

        lang_text_draw(29, level, x + 30, y_offset + (20 * rows), FONT_NORMAL_GREEN);
        text_draw_number(residences_at_level, '@', " ", x, y_offset + (20 * rows), FONT_NORMAL_WHITE, 0);
        if (rows == 11) {
            x += 280;
            rows = 0;
        } else {
            rows++;
        }
    }


    // info in the top right corner
    text_draw(translation_for(TR_ADVISOR_TOTAL_NUM_HOUSES), 320, y_offset + 180, FONT_NORMAL_GREEN, 0);
    text_draw_number(total_residences, '@', " ", 500, y_offset + 180, FONT_NORMAL_WHITE, 0);

    text_draw(translation_for(TR_ADVISOR_AVAILABLE_HOUSING_CAPACITY), 320, y_offset + 200, FONT_NORMAL_GREEN, 0);
    text_draw_number(city_population_open_housing_capacity(), '@', " ", 500, y_offset + 200, FONT_NORMAL_WHITE, 0);

    text_draw(translation_for(TR_ADVISOR_TOTAL_HOUSING_CAPACITY), 320, y_offset + 220, FONT_NORMAL_GREEN, 0);
    text_draw_number(city_population_total_housing_capacity(), '@', " ", 500, y_offset + 220, FONT_NORMAL_WHITE, 0);


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

