#include "empire_sidebar_sort.h"

#include "core/config.h"
#include "core/string.h"
#include "empire/city.h"
#include "empire/trade_prices.h"
#include "empire/trade_route.h"
#include "game/resource.h"
#include "graphics/button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "input/mouse.h"
#include "translation/translation.h"

#include <string.h>

#define NO_POSITION ((unsigned int) -1)
#define WIDTH_BORDER 16

/* next Refactor notes:
* move out everything relating to the sidebar to this file, rename it to empire_sidebar.c
* complex_button.c should be split into separate files for cycling and checkbox buttons
* then all of them including dropdown_button should be moved to widget folder for clarity and simplicty
* simplify the sort/filter getting/setting/reading/saving logic - too many functions. should be one for read one for write.
*/

// Forward declaration of sidebar_city_entry structure
typedef struct {
    int sidebar_item_id;
    int empire_object_id;
    int city_id;
    int x, y;
    int width;
    int height;
} sidebar_city_entry;

// Static variables for sorting and filtering state
static struct {
    sort_method current_sorting;
    filter_method current_filtering;
    resource_type selected_filter_resource;
    int hovered_sorting_button;
    int sorting_reversed;
    int expanded_main;
    int trade_year;
} sort_data = {
    .current_sorting = SORT_BY_NAME,
    .current_filtering = FILTER_NONE,
    .selected_filter_resource = RESOURCE_NONE,
    .hovered_sorting_button = NO_POSITION,
    .sorting_reversed = 0,
    .expanded_main = -1,
    .trade_year = 0
};

// Arrow button info structure
typedef struct {
    int x, y, width, height;
    int is_down;
} arrow_button_info;

static arrow_button_info sorting_arrow_button;
static int sorting_arrow_focused = 0;

// Sorting buttons state
static sorting_button sorting_buttons[MAX_SORTING_BUTTONS];
static int sorting_button_count = 0;

void window_empire_sidebar_sort_set_trade_year(int year)
{
    sort_data.trade_year = year;
}

static filter_method filters_from_config(void)
{
    filter_method filters = FILTER_NONE;

    switch (config_get(CONFIG_UI_EMPIRE_SIDEBAR_FILTER_ROUTE_TYPE)) {
        case 1:
            filters |= FILTER_BY_LAND;
            break;
        case 2:
            filters |= FILTER_BY_SEA;
            break;
    }

    switch (config_get(CONFIG_UI_EMPIRE_SIDEBAR_FILTER_ROUTE_OPEN)) {
        case 1:
            filters |= FILTER_BY_OPEN;
            break;
        case 2:
            filters |= FILTER_BY_CLOSED;
            break;
    }

    switch (config_get(CONFIG_UI_EMPIRE_SIDEBAR_FILTER_RESOURCE_TYPE)) {
        case 1:
            filters |= FILTER_BY_RESOURCE;
            break;
        case 2:
            filters |= FILTER_BY_RESOURCE_BUY;
            break;
        case 3:
            filters |= FILTER_BY_RESOURCE_SELL;
            break;
    }

    return filters;
}

static void save_filters_to_config(filter_method filters)
{
    int route_type = 0;
    int route_open = 0;
    int resource_type = 0;

    if (filters & FILTER_BY_LAND) {
        route_type = 1;
    } else if (filters & FILTER_BY_SEA) {
        route_type = 2;
    }

    if (filters & FILTER_BY_OPEN) {
        route_open = 1;
    } else if (filters & FILTER_BY_CLOSED) {
        route_open = 2;
    }

    if (filters & FILTER_BY_RESOURCE_BUY) {
        resource_type = 2;
    } else if (filters & FILTER_BY_RESOURCE_SELL) {
        resource_type = 3;
    } else if (filters & FILTER_BY_RESOURCE) {
        resource_type = 1;
    }

    config_set(CONFIG_UI_EMPIRE_SIDEBAR_FILTER_ROUTE_TYPE, route_type);
    config_set(CONFIG_UI_EMPIRE_SIDEBAR_FILTER_ROUTE_OPEN, route_open);
    config_set(CONFIG_UI_EMPIRE_SIDEBAR_FILTER_RESOURCE_TYPE, resource_type);
}

static resource_type filter_resource_from_config(void)
{
    int resource = config_get(CONFIG_UI_EMPIRE_SIDEBAR_FILTER_RESOURCE);
    if (resource < RESOURCE_NONE || resource >= RESOURCE_MAX) {
        return RESOURCE_NONE;
    }
    return resource;
}


int window_empire_sidebar_sort_count_trade_resources(const empire_city *city, int is_sell)
{
    int count = 0;
    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (resource_is_storable(r)) {
            if ((is_sell && city->sells_resource[r]) ||
                (!is_sell && city->buys_resource[r])) {
                count++;
            }
        }
    }
    return count;
}

static int get_city_trade_quota_fill(const empire_city *city, int is_sell)
{
    int total_now = 0;
    int total_max = 0;

    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (!resource_is_storable(r)) continue;

        if ((is_sell && !city->sells_resource[r]) || (!is_sell && !city->buys_resource[r])) continue;

        int max = trade_route_limit(city->route_id, r, !is_sell);
        int now = trade_route_traded(city->route_id, r, !is_sell);

        total_max += max;
        total_now += now;
    }

    if (total_max == 0) return 0;
    return (100 * total_now) / total_max;
}

// Initialization
void window_empire_sidebar_sort_init(void)
{
    int sorting = config_get(CONFIG_UI_EMPIRE_SIDEBAR_SORT_METHOD);
    sort_data.current_sorting = sorting >= SORT_BY_NAME && sorting < MAX_SORTING_KEY ? sorting : SORT_BY_NAME;
    sort_data.current_filtering = filters_from_config();
    sort_data.selected_filter_resource = filter_resource_from_config();
    sort_data.hovered_sorting_button = NO_POSITION;
    sort_data.sorting_reversed = config_get(CONFIG_UI_EMPIRE_SIDEBAR_SORT_REVERSED) ? 1 : 0;
    sort_data.expanded_main = -1;
    sorting_button_count = 0;
}

// Getter functions
int window_empire_sidebar_sort_get_current_sorting(void) { return sort_data.current_sorting; }
int window_empire_sidebar_sort_get_current_filtering(void) { return sort_data.current_filtering; }
resource_type window_empire_sidebar_sort_get_selected_filter_resource(void) { return sort_data.selected_filter_resource; }
int window_empire_sidebar_sort_get_hovered_sorting_button(void) { return sort_data.hovered_sorting_button; }
int window_empire_sidebar_sort_get_sorting_reversed(void) { return sort_data.sorting_reversed; }
int window_empire_sidebar_sort_get_expanded_main(void) { return sort_data.expanded_main; }

// Setter functions
void window_empire_sidebar_sort_set_current_sorting(int sorting)
{
    sort_data.current_sorting = sorting;
    config_set(CONFIG_UI_EMPIRE_SIDEBAR_SORT_METHOD, sorting);
}

void window_empire_sidebar_sort_set_current_filtering(int filtering)
{
    sort_data.current_filtering = filtering;
    save_filters_to_config(filtering);
}

void window_empire_sidebar_sort_set_selected_filter_resource(resource_type resource)
{
    sort_data.selected_filter_resource = resource;
    config_set(CONFIG_UI_EMPIRE_SIDEBAR_FILTER_RESOURCE, resource);
}

void window_empire_sidebar_sort_set_hovered_sorting_button(int button) { sort_data.hovered_sorting_button = button; }
void window_empire_sidebar_sort_set_sorting_reversed(int reversed)
{
    sort_data.sorting_reversed = reversed;
    config_set(CONFIG_UI_EMPIRE_SIDEBAR_SORT_REVERSED, reversed);
}

void window_empire_sidebar_sort_set_expanded_main(int expanded) { sort_data.expanded_main = expanded; }

// Reset functions
void window_empire_sidebar_sort_reset_hovered_sorting_button(void) { sort_data.hovered_sorting_button = NO_POSITION; }
void window_empire_sidebar_sort_reset_sorting_button_count(void) { sorting_button_count = 0; }

// Button management
int window_empire_sidebar_sort_get_sorting_button_count(void) { return sorting_button_count; }
const sorting_button *window_empire_sidebar_sort_get_sorting_button(int index)
{
    if (index < 0 || index >= sorting_button_count) return 0;
    return &sorting_buttons[index];
}

void window_empire_sidebar_sort_register_sorting_button(int x, int y, int width, int height, int button_type)
{
    if (sorting_button_count >= MAX_SORTING_BUTTONS) return;
    sorting_buttons[sorting_button_count++] = (sorting_button) { x, y, width, height, button_type };
}

int window_empire_sidebar_sort_sidebar_city_sorter(const void *a, const void *b)
{
    const sidebar_city_entry *entry_a = (const sidebar_city_entry *) a;
    const sidebar_city_entry *entry_b = (const sidebar_city_entry *) b;

    const empire_city *city_a = empire_city_get(entry_a->city_id);
    const empire_city *city_b = empire_city_get(entry_b->city_id);

    // Add null pointer checks to prevent crashes
    if (!city_a || !city_b) {
        // If one is null and the other isn't, put the null one at the end
        if (!city_a && !city_b) return 0;
        if (!city_a) return 1;
        if (!city_b) return -1;
    }

    int result = 0;

    switch (sort_data.current_sorting) {
        case SORT_BY_NAME:
        {
            const char *name_a = (const char *) empire_city_get_name(city_a);
            const char *name_b = (const char *) empire_city_get_name(city_b);
            result = strcmp(name_a, name_b);
            break;
        }

        case SORT_BY_QUOTA_FILL_EXPORT:
        case SORT_BY_QUOTA_FILL_IMPORT:
        {
            int is_sell = (sort_data.current_sorting == SORT_BY_QUOTA_FILL_IMPORT);
            int quota_a = get_city_trade_quota_fill(city_a, is_sell);
            int quota_b = get_city_trade_quota_fill(city_b, is_sell);
            result = (quota_a > quota_b) - (quota_a < quota_b);
            break;
        }

        case SORT_BY_ROUTE_COST:
        {
            int cost_a = city_a->cost_to_open;
            int cost_b = city_b->cost_to_open;
            result = (cost_a > cost_b) - (cost_a < cost_b);
            break;
        }

        case SORT_BY_PROFIT:
        {
            int profit_a = 0;
            int profit_b = 0;

            for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                if (!resource_is_storable(r)) continue;

                if (city_a->sells_resource[r]) {
                    int amount = trade_route_traded(city_a->route_id, r, 0);
                    int price = trade_price_sell(r, !city_a->is_sea_trade);
                    profit_a += amount * price;
                }
                if (city_a->buys_resource[r]) {
                    int amount = trade_route_traded(city_a->route_id, r, 1);
                    int price = trade_price_buy(r, !city_a->is_sea_trade);
                    profit_a -= amount * price;
                }

                if (city_b->sells_resource[r]) {
                    int amount = trade_route_traded(city_b->route_id, r, 0);
                    int price = trade_price_sell(r, !city_b->is_sea_trade);
                    profit_b += amount * price;
                }
                if (city_b->buys_resource[r]) {
                    int amount = trade_route_traded(city_b->route_id, r, 1);
                    int price = trade_price_buy(r, !city_b->is_sea_trade);
                    profit_b -= amount * price;
                }
            }

            result = (profit_a > profit_b) - (profit_a < profit_b);
            break;
        }

        default:
            break;
    }

    if (sort_data.sorting_reversed)
        result = -result;

    return result;
}

int window_empire_sidebar_sort_city_matches_current_filter(const empire_city *city)
{
    if (!city) {
        return 0; // Null cities don't match any filter
    }

    filter_method filters = sort_data.current_filtering;
    if (filters == FILTER_NONE) {
        return 1;
    }
    int was_open = trade_route_was_open(city->route_id, sort_data.trade_year);
    if ((filters & FILTER_BY_OPEN) && !was_open) {
        return 0;
    }
    if ((filters & FILTER_BY_CLOSED) && was_open) {
        return 0;
    }
    if ((filters & FILTER_BY_LAND) && city->is_sea_trade) {
        return 0;
    }
    if ((filters & FILTER_BY_SEA) && !city->is_sea_trade) {
        return 0;
    }

    if (filters & (FILTER_BY_RESOURCE | FILTER_BY_RESOURCE_SELL | FILTER_BY_RESOURCE_BUY)) {
        int matches_resource = 0;

        if (sort_data.selected_filter_resource == RESOURCE_NONE) {
            for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                if (!resource_is_storable(r)) {
                    continue;
                }
                if ((filters & FILTER_BY_RESOURCE) && (city->buys_resource[r] || city->sells_resource[r])) {
                    matches_resource = 1;
                }
                if ((filters & FILTER_BY_RESOURCE_SELL) && city->sells_resource[r]) {
                    matches_resource = 1;
                }
                if ((filters & FILTER_BY_RESOURCE_BUY) && city->buys_resource[r]) {
                    matches_resource = 1;
                }
                if (matches_resource) {
                    break;
                }
            }
        } else {
            for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                if (sort_data.selected_filter_resource != r) {
                    continue;
                }
                if ((filters & FILTER_BY_RESOURCE) && (city->buys_resource[r] || city->sells_resource[r])) {
                    matches_resource = 1;
                }
                if ((filters & FILTER_BY_RESOURCE_SELL) && city->sells_resource[r]) {
                    matches_resource = 1;
                }
                if ((filters & FILTER_BY_RESOURCE_BUY) && city->buys_resource[r]) {
                    matches_resource = 1;
                }
                break;
            }
        }

        if (!matches_resource) {
            return 0;
        }
    }

    return 1;
}

void window_empire_sidebar_sort_draw_simple_button(int x, int y, int width, int height, int is_focused, int group1, int number1,
     int group2, int number2, int button_type, int image_id)
{
    (void) x;
    (void) y;
    (void) width;
    (void) height;
    (void) is_focused;
    (void) group1;
    (void) number1;
    (void) group2;
    (void) number2;
    (void) button_type;
    (void) image_id;
}

void window_empire_sidebar_sort_draw_sorting_arrow_button(int button_x, int button_y, int button_width, int button_height)
{
    sorting_arrow_button.is_down = window_empire_sidebar_sort_get_sorting_reversed() ? 0 : 1;
    (void) button_x;
    (void) button_y;
    (void) button_width;
    (void) button_height;
}

void window_empire_sidebar_sort_draw_expanding_buttons(int sidebar_x_min, int sidebar_y_min, int sidebar_width, int has_scrollbar)
{
    window_empire_sidebar_sort_reset_sorting_button_count(); // Reset count for sorting buttons
    sorting_arrow_focused = 0;
    sorting_arrow_button.is_down = window_empire_sidebar_sort_get_sorting_reversed() ? 0 : 1;
    (void) sidebar_x_min;
    (void) sidebar_y_min;
    (void) sidebar_width;
    (void) has_scrollbar;
}

int window_empire_sidebar_sort_handle_expanding_buttons_input(const mouse *m)
{
    window_empire_sidebar_sort_set_hovered_sorting_button(NO_POSITION);
    window_empire_sidebar_sort_set_expanded_main(NO_POSITION);
    sorting_arrow_focused = 0;
    (void) m;
    return 0;
}

int window_empire_sidebar_sort_get_sorting_arrow_focused(void)
{
    return sorting_arrow_focused;
}

int window_empire_sidebar_sort_get_sorting_arrow_is_down(void)
{
    return sorting_arrow_button.is_down;
}
