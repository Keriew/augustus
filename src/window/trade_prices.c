#include "trade_prices.h"

#include "building/caravanserai.h"
#include "building/monument.h"
#include "city/buildings.h"
#include "city/trade_policy.h"
#include "empire/city.h"
#include "empire/trade_prices.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "window/advisors.h"

static struct {
    int x;
    int y;
    int width;
    int height;
    int full_screen;
} shade;

static struct {
    int four_line;
    int window_width;
    int x_offset;
} data;

static void init(int shade_x, int shade_y, int shade_width, int shade_height)
{
    shade.full_screen = shade_x == 0 && shade_y == 0 && shade_width == screen_width() && shade_height == screen_height();
    if (!shade.full_screen) {
        shade.x = shade_x;
        shade.y = shade_y;
        shade.width = shade_width;
        shade.height = shade_height;
    }
    int has_caravanserai = building_monument_working(BUILDING_CARAVANSERAI);
    int has_lighthouse = building_monument_working(BUILDING_LIGHTHOUSE);
    trade_policy land_policy = city_trade_policy_get(LAND_TRADE_POLICY);
    trade_policy sea_policy = city_trade_policy_get(SEA_TRADE_POLICY);

    int has_land_trade_policy = has_caravanserai && land_policy && land_policy != TRADE_POLICY_3;
    int has_sea_trade_policy = has_lighthouse && sea_policy && sea_policy != TRADE_POLICY_3;
    int same_policy = land_policy == sea_policy;
    data.four_line = ((has_sea_trade_policy && !has_land_trade_policy) ||
        (!has_sea_trade_policy && has_land_trade_policy) ||
        (has_sea_trade_policy && has_land_trade_policy && !same_policy));

    data.window_width = data.four_line ? 4 : 9;
    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (empire_can_import_resource_potentially(r) || empire_can_produce_resource_potentially(r)) {
            data.window_width += 2;
        }
    }
    data.x_offset = 320 - data.window_width * BLOCK_SIZE / 2;
}

static void draw_background(void)
{
    window_draw_underlying_window();

    if (shade.full_screen) {
        graphics_shade_rect(0, 0, screen_width(), screen_height(), 8);
    } else {
        graphics_shade_rect(shade.x + screen_dialog_offset_x(), shade.y + screen_dialog_offset_y(),
            shade.width, shade.height, 8);
    }

    graphics_in_dialog();

    int has_caravanserai = building_monument_working(BUILDING_CARAVANSERAI);
    int has_lighthouse = building_monument_working(BUILDING_LIGHTHOUSE);
    trade_policy land_policy = city_trade_policy_get(LAND_TRADE_POLICY);
    trade_policy sea_policy = city_trade_policy_get(SEA_TRADE_POLICY);

    int has_land_trade_policy = has_caravanserai && land_policy && land_policy != TRADE_POLICY_3;
    int has_sea_trade_policy = has_lighthouse && sea_policy && sea_policy != TRADE_POLICY_3;
    int same_policy = land_policy == sea_policy;
    int window_height = 11;
    int line_buy_position = 230;
    int line_sell_position = 270;
    int number_margin = 25;
    int button_position = 295;
    int icon_shift = data.x_offset + 110;
    int price_shift = data.x_offset + 104;
    int no_policy = !has_land_trade_policy && !has_sea_trade_policy;

    if (data.four_line) {
        window_height = 17;
        line_sell_position = 305;
        button_position = 390;
        icon_shift = data.x_offset + 20;
        price_shift = data.x_offset + 14;
    }

    outer_panel_draw(data.x_offset, 144, data.window_width, window_height);

    lang_text_draw_centered(54, 21, data.x_offset, 153, data.window_width * BLOCK_SIZE, FONT_LARGE_BLACK);

    if (data.four_line) {
        lang_text_draw_centered(54, 22, data.x_offset, line_buy_position,
            data.window_width * BLOCK_SIZE, FONT_NORMAL_BLACK);
        lang_text_draw_centered(54, 23, data.x_offset, line_sell_position,
            data.window_width * BLOCK_SIZE, FONT_NORMAL_BLACK);
    } else {
        lang_text_draw(54, 22, data.x_offset + 10, line_buy_position, FONT_NORMAL_BLACK);
        lang_text_draw(54, 23, data.x_offset + 10, line_sell_position, FONT_NORMAL_BLACK);
    }

    int x_offset = 32;

    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (!empire_can_import_resource_potentially(r) && !empire_can_produce_resource_potentially(r)) {
            continue;
        }
        image_draw(resource_get_data(r)->image.icon, icon_shift + x_offset, 194, COLOR_MASK_NONE, SCALE_NONE);

        if (!data.four_line || no_policy) {
            if (no_policy) {
                text_draw_number_centered(trade_price_buy(r, 0),
                    price_shift + x_offset, line_buy_position, 30, FONT_SMALL_PLAIN);
                text_draw_number_centered(trade_price_sell(r, 0),
                    price_shift + x_offset, line_sell_position, 30, FONT_SMALL_PLAIN);
            } else {
                text_draw_number_centered_colored(trade_price_buy(r, 1),
                    price_shift + x_offset, line_buy_position, 30, FONT_SMALL_PLAIN,
                    land_policy == TRADE_POLICY_1 ? COLOR_MASK_PURPLE : COLOR_MASK_DARK_GREEN);
                text_draw_number_centered_colored(trade_price_sell(r, 1),
                    price_shift + x_offset, line_sell_position, 30, FONT_SMALL_PLAIN,
                    land_policy == TRADE_POLICY_1 ? COLOR_MASK_DARK_GREEN : COLOR_MASK_PURPLE);
            }
        } else {
            if (has_land_trade_policy) {
                text_draw_number_centered_colored(trade_price_buy(r, 1),
                    price_shift + x_offset, line_buy_position + number_margin, 30, FONT_SMALL_PLAIN,
                    land_policy == TRADE_POLICY_1 ? COLOR_MASK_PURPLE : COLOR_MASK_DARK_GREEN);
                text_draw_number_centered_colored(trade_price_sell(r, 1),
                    price_shift + x_offset, line_sell_position + number_margin, 30, FONT_SMALL_PLAIN,
                    land_policy == TRADE_POLICY_1 ? COLOR_MASK_DARK_GREEN : COLOR_MASK_PURPLE);
            } else {
                text_draw_number_centered(trade_price_buy(r, 1),
                    price_shift + x_offset, line_buy_position + number_margin, 30, FONT_SMALL_PLAIN);
                text_draw_number_centered(trade_price_sell(r, 1),
                    price_shift + x_offset, line_sell_position + number_margin, 30, FONT_SMALL_PLAIN);
            }
            if (has_sea_trade_policy) {
                text_draw_number_centered_colored(trade_price_buy(r, 0),
                    price_shift + x_offset, line_buy_position + 2 * number_margin, 30, FONT_SMALL_PLAIN,
                    sea_policy == TRADE_POLICY_1 ? COLOR_MASK_PURPLE : COLOR_MASK_DARK_GREEN);
                text_draw_number_centered_colored(trade_price_sell(r, 0),
                    price_shift + x_offset, line_sell_position + 2 * number_margin, 30, FONT_SMALL_PLAIN,
                    sea_policy == TRADE_POLICY_1 ? COLOR_MASK_DARK_GREEN : COLOR_MASK_PURPLE);
            } else {
                text_draw_number_centered(trade_price_buy(r, 0),
                    price_shift + x_offset, line_buy_position + 2 * number_margin, 30, FONT_SMALL_PLAIN);
                text_draw_number_centered(trade_price_sell(r, 0),
                    price_shift + x_offset, line_sell_position + 2 * number_margin, 30, FONT_SMALL_PLAIN);
            }
        }

        x_offset += 32;
    }

    if (data.four_line) {
        int y_pos_buy = line_buy_position + number_margin - 5;
        int y_pos_sell = line_sell_position + number_margin - 5;

        int image_id = image_group(GROUP_EMPIRE_TRADE_ROUTE_TYPE) + 1;

        image_draw(image_id, data.x_offset + 16, y_pos_buy, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_id, data.x_offset + 16, y_pos_sell, COLOR_MASK_NONE, SCALE_NONE);

        image_id = image_group(GROUP_EMPIRE_TRADE_ROUTE_TYPE);
        if (!same_policy) {
            y_pos_buy += number_margin;
            y_pos_sell += number_margin;
        }
        image_draw(image_id, data.x_offset + 16, y_pos_buy, COLOR_MASK_NONE, SCALE_NONE);
        image_draw(image_id, data.x_offset + 16, y_pos_sell, COLOR_MASK_NONE, SCALE_NONE);
    }

    lang_text_draw_centered(13, 1, data.x_offset, button_position, data.window_width * BLOCK_SIZE, FONT_NORMAL_BLACK);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (input_go_back_requested(m, h)) {
        window_go_back();
    }
}

static resource_type get_tooltip_resource(tooltip_context *c)
{
    int x_base = data.x_offset + 20 + screen_dialog_offset_x();
    if (!data.four_line) {
        x_base += 90;
    }
    int y = screen_dialog_offset_y() + 192;
    int x_mouse = c->mouse_x;
    int y_mouse = c->mouse_y;

    int x_offset = 32;

    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (!empire_can_import_resource_potentially(r) && !empire_can_produce_resource_potentially(r)) {
            continue;
        }
        int x = x_base + x_offset;
        if (x <= x_mouse && x + 24 > x_mouse && y <= y_mouse && y + 24 > y_mouse) {
            return r;
        }
        x_offset += 32;
    }
    return RESOURCE_NONE;
}

static void get_tooltip(tooltip_context *c)
{
    resource_type resource = get_tooltip_resource(c);
    if (resource == RESOURCE_NONE) {
        return;
    }
    c->type = TOOLTIP_BUTTON;
    c->precomposed_text = resource_get_data(resource)->text;
}

void window_trade_prices_show(int shade_x, int shade_y, int shade_width, int shade_height)
{
    init(shade_x, shade_y, shade_width, shade_height);
    window_type window = {
        WINDOW_TRADE_PRICES,
        draw_background,
        0,
        handle_input,
        get_tooltip
    };
    window_show(&window);
}
