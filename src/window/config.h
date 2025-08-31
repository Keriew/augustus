#ifndef WINDOW_CONFIG_H
#define WINDOW_CONFIG_H

typedef enum {
    CONFIG_FIRST_PAGE = 0,
    CONFIG_PAGE_GENERAL = 0,
    CONFIG_PAGE_UI_CHANGES = 1,
    CONFIG_PAGE_GAMEPLAY_CHANGES = 2,
    CONFIG_PAGE_CITY_MANAGEMENT_CHANGES = 3,
    CONFIG_PAGES = 4
} window_config_page;

typedef enum {
    CATEGORY_UI_GENERAL = 0,
    CATEGORY_UI_SCROLLING = 1,
    CATEGORY_UI_BUILDING = 2,
    CATEGORY_UI_CITY_VIEW = 3,
    CATEGORY_UI_WEATHER = 4,
    CATEGORY_UI_COUNT
} ui_config_category;

typedef enum {
    CATEGORY_CITY_MANAGEMENT_STORAGE = 0,
    CATEGORY_CITY_MANAGEMENT_ROADS = 1,
    CATEGORY_CITY_MANAGEMENT_ROADBLOCK_SETTINGS = 2,
    CATEGORY_CITY_MANAGEMENT_HOUSING = 3,
    CATEGORY_CITY_COUNT
} city_management_category;

void window_config_show(window_config_page page, int show_background_image);

#endif // WINDOW_CONFIG_H
