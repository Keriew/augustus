#ifndef WINDOW_EMPIRE_SIDEBAR_SORT_H
#define WINDOW_EMPIRE_SIDEBAR_SORT_H

#include "empire/city.h"
#include "game/resource.h"
#include "input/mouse.h"

// Constants
#define MAX_SORTING_BUTTONS 64

#define BUTTON_INDEX_SORT_MAIN 0
#define BUTTON_INDEX_FILTER_MAIN 1
#define BUTTON_INDEX_FIRST_SORT_METHOD (BUTTON_INDEX_FILTER_MAIN + 1) // 2
#define BUTTON_INDEX_FIRST_FILTER_METHOD (BUTTON_INDEX_FIRST_SORT_METHOD + MAX_SORTING_KEY)
#define BUTTON_INDEX_FILTERING_RESOURCES 100

// Enums
typedef enum {
    SORT_BY_NAME,
    SORT_BY_QUOTA_FILL_EXPORT,
    SORT_BY_QUOTA_FILL_IMPORT,
    SORT_BY_ROUTE_COST,
    SORT_BY_PROFIT,
    MAX_SORTING_KEY
} sort_method;

typedef enum {
    FILTER_BY_RESOURCE,
    FILTER_BY_OPEN,
    FILTER_BY_CLOSED,
    FILTER_BY_LAND,
    FILTER_BY_SEA,
    FILTER_NONE,
    MAX_FILTER_KEY
} filter_method;

typedef struct {
    int x, y, width, height;
    int button_type;
} sorting_button;

// Forward declaration for sidebar_city_entry (defined in empire.c)
struct sidebar_city_entry;

// Core sorting and filtering functions
int window_empire_sidebar_sort_sidebar_city_sorter(const void *a, const void *b);
int window_empire_sidebar_sort_city_matches_current_filter(const empire_city *city);

// Sorting and filtering state management
int window_empire_sidebar_sort_get_current_sorting(void);
int window_empire_sidebar_sort_get_current_filtering(void);
resource_type window_empire_sidebar_sort_get_selected_filter_resource(void);
int window_empire_sidebar_sort_get_hovered_sorting_button(void);
int window_empire_sidebar_sort_get_sorting_reversed(void);
int window_empire_sidebar_sort_get_expanded_main(void);
int window_empire_sidebar_sort_get_resource_selection_active(void);
int window_empire_sidebar_sort_count_trade_resources(const empire_city *city, int is_sell);
void window_empire_sidebar_sort_set_current_sorting(int sorting);
void window_empire_sidebar_sort_set_current_filtering(int filtering);
void window_empire_sidebar_sort_set_selected_filter_resource(resource_type resource);
void window_empire_sidebar_sort_set_hovered_sorting_button(int button);
void window_empire_sidebar_sort_set_sorting_reversed(int reversed);
void window_empire_sidebar_sort_set_expanded_main(int expanded);
void window_empire_sidebar_sort_set_resource_selection_active(int active);

void window_empire_sidebar_sort_reset_hovered_sorting_button(void);

// Button management
void window_empire_sidebar_sort_register_sorting_button(int x, int y, int width, int height, int button_type);
void window_empire_sidebar_sort_reset_sorting_button_count(void);
int window_empire_sidebar_sort_get_sorting_button_count(void);
const sorting_button *window_empire_sidebar_sort_get_sorting_button(int index);

// Button drawing functions
void window_empire_sidebar_sort_draw_simple_button(int x, int y, int width, int height, int is_focused, int group1, int number1,
    int group2, int number2, int button_type, int image_id);
void window_empire_sidebar_sort_draw_sorting_arrow_button(int button_x, int button_y, int button_width, int button_height);
void window_empire_sidebar_sort_draw_expanding_buttons(int sidebar_x_min, int sidebar_y_min, int sidebar_width, int has_scrollbar);

// Input handling
int window_empire_sidebar_sort_handle_expanding_buttons_input(const mouse *m);

// Arrow button state access
int window_empire_sidebar_sort_get_sorting_arrow_focused(void);
int window_empire_sidebar_sort_get_sorting_arrow_is_down(void);

// Initialization
void window_empire_sidebar_sort_init(void);

#endif // WINDOW_EMPIRE_SIDEBAR_SORT_H
