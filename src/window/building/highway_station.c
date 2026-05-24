#include "highway_station.h"

#include "building/building.h"
#include "building/highway_station.h"
#include "city/constants.h"
#include "game/resource.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "translation/translation.h"
#include "window/building/figures.h"

#define ROW_HEIGHT 22

static int draw_resource_row(int x, int y, int icon_id, const uint8_t *label, int amount)
{
    image_draw(icon_id, x, y - 6, COLOR_MASK_NONE, SCALE_NONE);
    int width = text_draw(label, x + 30, y, FONT_NORMAL_BLACK, 0);
    text_draw_number(amount, '@', " ", x + 30 + width, y, FONT_NORMAL_BLACK, 0);
    return ROW_HEIGHT;
}

void window_building_draw_highway_station(building_info_context *c)
{
    c->advisor_button = ADVISOR_FINANCIAL;
    c->help_id = 0;
    window_building_play_sound(c, "wavs/forum.wav");

    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    text_draw_centered(translation_for(TR_BUILDING_HIGHWAY_STATION),
        c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, 0);

    building *b = building_get(c->building_id);
    int x_text = c->x_offset + 32;
    int text_width = BLOCK_SIZE * (c->width_blocks - 4);

    int need = building_highway_station_monthly_need();
    int highway_tiles = building_highway_station_highway_blocks();
    int stone_units = b->resources[RESOURCE_STONE] / 100;
    int sand_units = b->resources[RESOURCE_SAND] / 100;

    int y = c->y_offset + 56;

    // Stocks block
    y += draw_resource_row(x_text, y, resource_get_data(RESOURCE_STONE)->image.icon,
        translation_for(TR_BUILDING_HIGHWAY_STATION_STONE_STOCK), stone_units);
    y += draw_resource_row(x_text, y, resource_get_data(RESOURCE_SAND)->image.icon,
        translation_for(TR_BUILDING_HIGHWAY_STATION_SAND_STOCK), sand_units);

    // Monthly need
    int width = text_draw(translation_for(TR_BUILDING_HIGHWAY_STATION_MONTHLY_NEED),
        x_text, y, FONT_NORMAL_BLACK, 0);
    text_draw_number(need, '@', " ", x_text + width, y, FONT_NORMAL_BLACK, 0);
    y += ROW_HEIGHT;

    // Highway tile count in the city
    width = text_draw(translation_for(TR_BUILDING_HIGHWAY_STATION_HIGHWAY_TILES),
        x_text, y, FONT_NORMAL_BLACK, 0);
    text_draw_number(highway_tiles, '@', " ", x_text + width, y, FONT_NORMAL_BLACK, 0);
    y += ROW_HEIGHT + 4;

    // Status line — green when functional (bonus active), red when not
    int status_tr = 0;
    font_t status_font = FONT_NORMAL_BLACK;
    if (!c->has_road_access) {
        window_building_draw_description_at(c, y - c->y_offset, 69, 25);
    } else if (b->num_workers <= 0) {
        status_tr = TR_BUILDING_HIGHWAY_STATION_NO_EMPLOYEES;
        status_font = FONT_NORMAL_RED;
    } else if (!building_highway_station_is_functional(b)) {
        status_tr = TR_BUILDING_HIGHWAY_STATION_NO_RESOURCES;
        status_font = FONT_NORMAL_RED;
    } else {
        status_tr = TR_BUILDING_HIGHWAY_STATION_FUNCTIONAL;
        status_font = FONT_NORMAL_GREEN;
    }
    if (status_tr) {
        text_draw_multiline(translation_for(status_tr),
            x_text, y, text_width, 0, status_font, 0);
    }

    // Employment panel + risks
    inner_panel_draw(c->x_offset + 16, c->y_offset + 200, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 204);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 208);

    // Description below the panel
    text_draw_multiline(translation_for(TR_BUILDING_HIGHWAY_STATION_DESC),
        x_text, c->y_offset + 280, text_width, 0, FONT_NORMAL_BLACK, 0);
}
