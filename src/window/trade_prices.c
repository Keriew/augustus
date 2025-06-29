#include "trade_prices.h"

#include "building/caravanserai.h"
#include "building/lighthouse.h"
#include "building/monument.h"
#include "city/buildings.h"
#include "city/resource.h"
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
} data;

static color_t get_price_color(int land_trader, int is_sell) {
    int trade_factor = trade_factor_sign(land_trader, is_sell);
    if (trade_factor == 0) {
        return COLOR_BLACK; // No change
    }
    if (is_sell) { // Higher sell price - green
        return trade_factor > 0 ? COLOR_MASK_DARK_GREEN : COLOR_MASK_PURPLE;
    } else { // Higher buy price - red 
        return trade_factor > 0 ? COLOR_MASK_PURPLE : COLOR_MASK_DARK_GREEN;
    }
}

static void init(int shade_x, int shade_y, int shade_width, int shade_height)
{
    shade.full_screen = shade_x == 0 && shade_y == 0 && shade_width == screen_width() && shade_height == screen_height();
    if (!shade.full_screen) {
        shade.x = shade_x;
        shade.y = shade_y;
        shade.width = shade_width;
        shade.height = shade_height;
    }
    int has_caravanserai = building_caravanserai_is_fully_functional();
    int has_lighthouse = building_lighthouse_is_fully_functional();
    trade_policy land_policy = city_trade_policy_get(LAND_TRADE_POLICY);
    trade_policy sea_policy = city_trade_policy_get(SEA_TRADE_POLICY);

    int has_land_trade_policy = has_caravanserai && land_policy && land_policy != TRADE_POLICY_3;
    int has_sea_trade_policy = has_lighthouse && sea_policy && sea_policy != TRADE_POLICY_3;
    int same_policy = land_policy == sea_policy;
    data.four_line = ((has_sea_trade_policy && !has_land_trade_policy) ||
        (!has_sea_trade_policy && has_land_trade_policy) ||
        (has_sea_trade_policy && has_land_trade_policy && !same_policy));

    city_resource_determine_available(1);
    data.window_width = (data.four_line ? 4 : 9) + city_resource_get_potential()->size * 2;
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

    int has_caravanserai = building_caravanserai_is_fully_functional();
    int has_lighthouse = building_lighthouse_is_fully_functional();
    trade_policy land_policy = city_trade_policy_get(LAND_TRADE_POLICY);
    trade_policy sea_policy = city_trade_policy_get(SEA_TRADE_POLICY);

    int has_land_trade_policy = has_caravanserai && land_policy && land_policy != TRADE_POLICY_3;
    int has_sea_trade_policy = has_lighthouse && sea_policy && sea_policy != TRADE_POLICY_3;
    int window_height = 11;
    int line_buy_position = 86;
    int line_sell_position = 126;
    int number_margin = 25;
    int button_position = 151;
    int icon_shift = 142;
    int price_shift = 136;
    int no_policy = !has_land_trade_policy && !has_sea_trade_policy;

    if (data.four_line) {
        window_height = 17;
        line_sell_position = 161;
        button_position = 244;
        icon_shift = 52;
        price_shift = 46;
    }

    graphics_in_dialog_with_size(data.window_width * BLOCK_SIZE, window_height * BLOCK_SIZE);
    outer_panel_draw(0, 0, data.window_width, window_height);

    lang_text_draw_centered(54, 21, 0, 9, data.window_width * BLOCK_SIZE, FONT_LARGE_BLACK);

    if (data.four_line) {
        lang_text_draw_centered(54, 22, 0, line_buy_position,
            data.window_width * BLOCK_SIZE, FONT_NORMAL_BLACK);
        lang_text_draw_centered(54, 23, 0, line_sell_position,
            data.window_width * BLOCK_SIZE, FONT_NORMAL_BLACK);
    } else {
        lang_text_draw(54, 22, 10, line_buy_position, FONT_NORMAL_BLACK);
        lang_text_draw(54, 23, 10, line_sell_position, FONT_NORMAL_BLACK);
    }

    const resource_list *list = city_resource_get_potential();
    int resource_offset = BLOCK_SIZE * 2;

    for (unsigned int i = 0; i < list->size; i++) {
        resource_type r = list->items[i];

        int image_id = resource_get_data(r)->image.icon;
        const image *img = image_get(image_id);
        int base_width = (25 - img->original.width) / 2;
        int base_height = (25 - img->original.height) / 2;

        image_draw(resource_get_data(r)->image.icon, icon_shift + base_width - 4 + i * resource_offset,
            50 + base_height, COLOR_MASK_NONE, SCALE_NONE);

        if (!data.four_line || no_policy) {//same price on land and sea
            if (no_policy) {
                text_draw_number_centered(trade_price_buy(r, 0),
                    price_shift + i * resource_offset, line_buy_position, 30, FONT_SMALL_PLAIN);
                text_draw_number_centered(trade_price_sell(r, 0),
                    price_shift + i * resource_offset, line_sell_position, 30, FONT_SMALL_PLAIN);
            } else {

                text_draw_number_centered_colored(trade_price_buy(r, 1), //buy
                    price_shift + i * resource_offset, line_buy_position, 30, FONT_SMALL_PLAIN,
                    get_price_color(1, 0)); // land or trade doesnt matter - already established that it's same price
                text_draw_number_centered_colored(trade_price_sell(r, 1), //sell
                    price_shift + i * resource_offset, line_sell_position, 30, FONT_SMALL_PLAIN,
                    get_price_color(1, 1)); //if trade_policy1 , then RED. Change this
            }
        } else { //price difference between land and sea

            if (has_land_trade_policy) {

                text_draw_number_centered_colored(trade_price_buy(r, 1), // land route buy
                    price_shift + i * resource_offset, line_buy_position + number_margin, 30, FONT_SMALL_PLAIN,
                    get_price_color(1, 0));
                if (land_policy == TRADE_POLICY_2) { // land route sell 
                    text_draw_number_centered(trade_price_sell(r, 1),
                    price_shift + i * resource_offset, line_sell_position + number_margin, 30, FONT_SMALL_PLAIN);
                } else {
                    text_draw_number_centered_colored(trade_price_sell(r, 1),
                    price_shift + i * resource_offset, line_sell_position + number_margin, 30, FONT_SMALL_PLAIN,
                    get_price_color(1, 1));
                }
            } else {
                text_draw_number_centered(trade_price_buy(r, 1), //buy
                    price_shift + i * resource_offset, line_buy_position + number_margin, 30, FONT_SMALL_PLAIN);
                text_draw_number_centered(trade_price_sell(r, 1), //sell
                    price_shift + i * resource_offset, line_sell_position + number_margin, 30, FONT_SMALL_PLAIN);
            }
            if (has_sea_trade_policy) {
                text_draw_number_centered_colored(trade_price_buy(r, 0), // sea route
                    price_shift + i * resource_offset, line_buy_position + 2 * number_margin, 30, FONT_SMALL_PLAIN,
                    get_price_color(0, 0));
                if (sea_policy == TRADE_POLICY_2) {
                    text_draw_number_centered(trade_price_sell(r, 0),
                    price_shift + i * resource_offset, line_sell_position + 2 * number_margin, 30, FONT_SMALL_PLAIN);
                } else {
                    text_draw_number_centered_colored(trade_price_sell(r, 0),
                price_shift + i * resource_offset, line_sell_position + 2 * number_margin, 30, FONT_SMALL_PLAIN,
                get_price_color(0, 1));
                }

            } else {
                text_draw_number_centered(trade_price_buy(r, 0), // sea route
                    price_shift + i * resource_offset, line_buy_position + 2 * number_margin, 30, FONT_SMALL_PLAIN);
                text_draw_number_centered(trade_price_sell(r, 0),
                    price_shift + i * resource_offset, line_sell_position + 2 * number_margin, 30, FONT_SMALL_PLAIN);
            }
        }
    }

    if (data.four_line) {
        int y_positions[4] = {
            line_buy_position + number_margin,         // land buy
            line_buy_position + 2 * number_margin,     // sea buy
            line_sell_position + number_margin,        // land sell
            line_sell_position + 2 * number_margin     // sea sell
        };

        int image_id_land = image_group(GROUP_EMPIRE_TRADE_ROUTE_TYPE) + 1;
        int image_id_sea = image_group(GROUP_EMPIRE_TRADE_ROUTE_TYPE);

        // Land route icons
        image_draw(image_id_land, 13, y_positions[0] - 10, COLOR_MASK_NONE, SCALE_NONE); // land buy
        image_draw(image_id_land, 13, y_positions[2] - 10, COLOR_MASK_NONE, SCALE_NONE); // land sell

        // Sea route icons
        image_draw(image_id_sea, 10, y_positions[1] - 5, COLOR_MASK_NONE, SCALE_NONE);  // sea buy
        image_draw(image_id_sea, 10, y_positions[3] - 5, COLOR_MASK_NONE, SCALE_NONE);  // sea sell
    }

    lang_text_draw_centered(13, 1, 0, button_position, data.window_width * BLOCK_SIZE, FONT_NORMAL_BLACK);
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
    int x_base = 52 + (screen_width() - data.window_width * BLOCK_SIZE) / 2;
    if (!data.four_line) {
        x_base += 90;
    }
    int y = 48 + (screen_height() - (data.four_line ? 17 : 11) * BLOCK_SIZE) / 2;
    int x_mouse = c->mouse_x;
    int y_mouse = c->mouse_y;

    const resource_list *list = city_resource_get_potential();

    for (unsigned int i = 0; i < list->size; i++) {
        int x = x_base + i * BLOCK_SIZE * 2;
        if (x <= x_mouse && x + 24 > x_mouse && y <= y_mouse && y + 24 > y_mouse) {
            return list->items[i];
        }
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
