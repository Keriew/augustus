#include "overlay_menu.h"

#include "assets/assets.h"
#include "building/type.h"
#include "city/view.h"
#include "core/image.h"
#include "core/image_group.h"
#include "core/lang.h"
#include "core/time.h"
#include "game/state.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "translation/translation.h"
#include "window/city.h"

#include <stdlib.h>

#define MENU_X_OFFSET 170
#define SUBMENU_X_OFFSET 348
#define MENU_Y_OFFSET 72
#define MENU_ITEM_HEIGHT 24
#define MENU_CLICK_MARGIN 20
#define MENU_ITEM_WIDTH 160
#define TOP_MARGIN 74
#define LABEL_WIDTH_BLOCKS 10
#define SIDEBAR_MARGIN_X 10
#define MAX_BUTTONS 20
#define OVERLAY_MENU_END { -1, -1, JULIUS, NULL }

static void button_menu_item(const generic_button *button);

typedef enum
{
    JULIUS = 0,
    AUGUSTUS = 1,
    BUILDING_TYPE = 2,
} translation_type;

typedef struct overlay_menu_entry {
    int overlay;
    int translation;
    int translation_type;
    const struct overlay_menu_entry *submenu;
} overlay_menu_entry;

static const overlay_menu_entry OVERLAY_MENU_SENTINEL = OVERLAY_MENU_END;

static const overlay_menu_entry submenu_risks[] = {
    { OVERLAY_FIRE, 0, JULIUS, NULL },
    { OVERLAY_DAMAGE, 0, JULIUS, NULL },
    { OVERLAY_CRIME, 0, JULIUS, NULL },
    { OVERLAY_NATIVE, 0, JULIUS, NULL },
    { OVERLAY_PROBLEMS, 0, JULIUS, NULL },
    { OVERLAY_ENEMY, TR_OVERLAY_ENEMY, AUGUSTUS, NULL },
    { OVERLAY_SICKNESS, TR_OVERLAY_SICKNESS, AUGUSTUS, NULL },
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_entertainment[] = {
    { OVERLAY_ENTERTAINMENT, OVERLAY_ENTERTAINMENT, JULIUS, NULL },
    { OVERLAY_TAVERN, TR_OVERLAY_TAVERN, AUGUSTUS, NULL },
    { OVERLAY_THEATER, 0, JULIUS, NULL },
    { OVERLAY_AMPHITHEATER, 0, JULIUS, NULL },
    { OVERLAY_ARENA, TR_OVERLAY_ARENA_COL, AUGUSTUS, NULL },
    { OVERLAY_COLOSSEUM, 0, JULIUS, NULL },
    { OVERLAY_HIPPODROME, 0, JULIUS, NULL },
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_education[] = {
    {OVERLAY_EDUCATION, OVERLAY_EDUCATION, JULIUS, NULL},
    {OVERLAY_SCHOOL, 0, JULIUS, NULL},
    {OVERLAY_LIBRARY, 0, JULIUS, NULL},
    {OVERLAY_ACADEMY, 0, JULIUS, NULL},
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_health[] = {
    {OVERLAY_HEALTH, TR_OVERLAY_HEALTH, AUGUSTUS, NULL},
    {OVERLAY_BARBER, 0, JULIUS, NULL},
    {OVERLAY_BATHHOUSE, 0, JULIUS, NULL},
    {OVERLAY_CLINIC, 0, JULIUS, NULL},
    {OVERLAY_HOSPITAL, 0, JULIUS, NULL},
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_commerce[] = {
    {OVERLAY_LOGISTICS, TR_OVERLAY_LOGISTICS, AUGUSTUS, NULL},
    {OVERLAY_FOOD_STOCKS, 0, JULIUS, NULL},
    {OVERLAY_EFFICIENCY, TR_OVERLAY_EFFICIENCY, AUGUSTUS, NULL},
    {OVERLAY_MOTHBALL, TR_OVERLAY_MOTHBALL, AUGUSTUS, NULL},
    {OVERLAY_TAX_INCOME, 0, JULIUS, NULL},
    {OVERLAY_LEVY, TR_OVERLAY_LEVY, AUGUSTUS, NULL},
    {OVERLAY_EMPLOYMENT, TR_OVERLAY_EMPLOYMENT, AUGUSTUS, NULL},
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_housing_groups[] = {
    { OVERLAY_HOUSING_GROUPS_TENTS, TR_OVERLAY_HOUSING_TENTS, AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_SHACKS,TR_OVERLAY_HOUSING_SHACKS, AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_HOVELS,TR_OVERLAY_HOUSING_HOVELS, AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_CASAE,TR_OVERLAY_HOUSING_CASAS, AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_INSULAE,TR_OVERLAY_HOUSE_INSULAS, AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_VILLAS,TR_OVERLAY_HOUSE_VILLAS, AUGUSTUS, NULL},
    { OVERLAY_HOUSING_GROUPS_PALACES,TR_OVERLAY_HOUSE_PALACES, AUGUSTUS, NULL},
    OVERLAY_MENU_END
};

static const overlay_menu_entry submenu_housing[] = {
    { OVERLAY_HOUSING_GROUPS, TR_OVERLAY_BY_GROUP, AUGUSTUS, submenu_housing_groups},
    { OVERLAY_HOUSE_SMALL_TENT, BUILDING_HOUSE_SMALL_TENT, BUILDING_TYPE, NULL},
    { OVERLAY_HOUSE_LARGE_TENT, BUILDING_HOUSE_LARGE_TENT, BUILDING_TYPE, NULL},
    { OVERLAY_HOUSE_SMALL_SHACK, BUILDING_HOUSE_SMALL_SHACK, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_LARGE_SHACK, BUILDING_HOUSE_LARGE_SHACK, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_SMALL_HOVEL, BUILDING_HOUSE_SMALL_HOVEL, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_LARGE_HOVEL, BUILDING_HOUSE_LARGE_HOVEL, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_SMALL_CASA, BUILDING_HOUSE_SMALL_CASA, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_LARGE_CASA, BUILDING_HOUSE_LARGE_CASA, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_SMALL_INSULA, BUILDING_HOUSE_SMALL_INSULA, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_MEDIUM_INSULA, BUILDING_HOUSE_MEDIUM_INSULA, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_LARGE_INSULA, BUILDING_HOUSE_LARGE_INSULA, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_GRAND_INSULA, BUILDING_HOUSE_GRAND_INSULA, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_SMALL_VILLA, BUILDING_HOUSE_SMALL_VILLA, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_MEDIUM_VILLA, BUILDING_HOUSE_MEDIUM_VILLA, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_LARGE_VILLA, BUILDING_HOUSE_LARGE_VILLA, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_GRAND_VILLA, BUILDING_HOUSE_GRAND_VILLA, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_SMALL_PALACE, BUILDING_HOUSE_SMALL_PALACE, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_MEDIUM_PALACE, BUILDING_HOUSE_MEDIUM_PALACE, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_LARGE_PALACE, BUILDING_HOUSE_LARGE_PALACE, BUILDING_TYPE, NULL },
    { OVERLAY_HOUSE_LUXURY_PALACE, BUILDING_HOUSE_LUXURY_PALACE, BUILDING_TYPE, NULL },
    OVERLAY_MENU_END
};

static const overlay_menu_entry overlay_menu[] = {
    { OVERLAY_NONE,0, JULIUS, NULL },
    { OVERLAY_WATER,0, JULIUS, NULL },
    { 1, 0, JULIUS, submenu_risks},
    { 3, 0, JULIUS, submenu_entertainment},
    { 5,0, JULIUS, submenu_education},
    { 6,0, JULIUS, submenu_health},
    { 7,0, JULIUS, submenu_commerce},
    { OVERLAY_RELIGION,0, JULIUS, NULL },
    { OVERLAY_ROADS, TR_OVERLAY_ROADS, AUGUSTUS, NULL },
    { OVERLAY_DESIRABILITY,0, JULIUS, NULL },
    { OVERLAY_SENTIMENT, TR_OVERLAY_SENTIMENT, AUGUSTUS, NULL },
    { OVERLAY_HOUSING, TR_HEADER_HOUSING, AUGUSTUS, submenu_housing },
    OVERLAY_MENU_END
};

static struct {
    int selected_overlay_id;
    int selected_overlay_clicked;
    int show_menu;
    unsigned int menu_focus_button_index;
    generic_button buttons[MAX_BUTTONS];
} data;

static void show_menu(void)
{
    data.show_menu = 1;
}

static void hide_menu(void)
{
    data.show_menu = 0;
}

static void draw_background(void)
{
    window_city_draw_panels();
}

static int get_sidebar_x_offset(void)
{
    int view_x, view_y, view_width, view_height;
    city_view_get_viewport(&view_x, &view_y, &view_width, &view_height);
    return view_x + view_width;
}

static int is_mouse_hovering(const overlay_menu_entry *entry)
{
    const int index = (int) data.menu_focus_button_index - 1;

    if (index < 0) {
        return 0;
    }

    return data.buttons[index].parameter1 == entry->overlay;
}

static const uint8_t *get_overlay_text(const overlay_menu_entry *entry)
{
    if (entry->translation_type == AUGUSTUS) {
        return translation_for(entry->translation);
    }

    if (entry->translation_type == BUILDING_TYPE) {
        return lang_get_building_type_string(entry->translation);
    }

    return lang_get_string(14, entry->overlay);
}

static void draw_menu_item(const overlay_menu_entry *entry, const int i, const int x_offset, const int button_index)
{
    const int x = x_offset - MENU_ITEM_WIDTH;
    const int y = TOP_MARGIN + MENU_ITEM_HEIGHT * i;

    data.buttons[button_index] = (generic_button) {
        .x = (short) x,
        .y = (short) y,
        .width = MENU_ITEM_WIDTH,
        .height = MENU_ITEM_HEIGHT,
        .left_click_handler = button_menu_item,
        .parameter1 = entry->overlay,
    };

    label_draw(x, y, LABEL_WIDTH_BLOCKS, is_mouse_hovering(entry) ? LABEL_TYPE_NORMAL : LABEL_TYPE_HOVER);

    text_draw_centered(get_overlay_text(entry),
        x_offset - MENU_ITEM_WIDTH,
        y + 4,
        MENU_ITEM_WIDTH,
        FONT_NORMAL_GREEN,
        COLOR_MASK_NONE);

    if (entry->submenu != NULL) {
        const int image_id = assets_get_image_id("UI", "Expand Menu Icon");
        image_draw(image_id, x + MENU_ITEM_WIDTH - 16, y + 3, COLOR_MASK_NONE, SCALE_NONE);
    }
}

static overlay_menu_entry find_overlay(const overlay_menu_entry *entries, const int overlay_id)
{
    for (unsigned i = 0; entries[i].overlay != -1; i++) {
        if (entries[i].overlay == overlay_id) {
            return entries[i];
        }

        if (entries[i].submenu != NULL) {
            const overlay_menu_entry found_sub_item = find_overlay(entries[i].submenu, overlay_id);
            if (found_sub_item.overlay != OVERLAY_MENU_SENTINEL.overlay) {
                return found_sub_item;
            }
        }
    }

    return OVERLAY_MENU_SENTINEL;
}

static void draw_menu(const overlay_menu_entry *entries)
{
    const int x_offset = get_sidebar_x_offset() - SIDEBAR_MARGIN_X;
    int button_index = 0;

    for (int i = 0; entries[i].overlay != -1; i++) {
        draw_menu_item(&entries[i], i, x_offset, button_index++);
    }
}

static void draw_foreground(void)
{
    window_city_draw();

    if (data.show_menu == 1) {
        const overlay_menu_entry menu_item = find_overlay(overlay_menu, data.selected_overlay_clicked);
        if (menu_item.submenu != NULL) {
            draw_menu(menu_item.submenu);
        } else {
            draw_menu(overlay_menu);;
        }
    }
}

static int click_outside_menu(const mouse *m, const int x_offset)
{
    return m->left.went_up &&
        (m->x < x_offset - MENU_CLICK_MARGIN - MENU_X_OFFSET ||
        m->x > x_offset + MENU_CLICK_MARGIN ||
        m->y < MENU_Y_OFFSET - MENU_CLICK_MARGIN ||
        m->y > MENU_Y_OFFSET + MENU_CLICK_MARGIN + MENU_ITEM_HEIGHT * MAX_BUTTONS);
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const int x_offset = get_sidebar_x_offset();
    int handled = 0;

    handled |= generic_buttons_handle_mouse(m, 0, 0, data.buttons, MAX_BUTTONS, &data.menu_focus_button_index);

    if (!handled && click_outside_menu(m, x_offset)) {
        data.selected_overlay_clicked = 0;
        hide_menu();
        window_city_show();
    }

    show_menu();
}

static void button_menu_item(const generic_button *button)
{
    const overlay_menu_entry selected_overlay = find_overlay(overlay_menu, button->parameter1);
    data.selected_overlay_clicked = selected_overlay.overlay;

    if (selected_overlay.submenu != NULL) {
        show_menu();
    } else {
        data.selected_overlay_id = selected_overlay.overlay;
        hide_menu();
        game_state_set_overlay(selected_overlay.overlay);
        window_city_show();
    }
}

void window_overlay_menu_show(void)
{
    const window_type window = {
        WINDOW_OVERLAY_MENU,
        draw_background,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}

void window_overlay_menu_update(void)
{
    data.selected_overlay_id = game_state_overlay();
}

const uint8_t *get_current_overlay_text(void)
{
    const overlay_menu_entry overlay_item = find_overlay(overlay_menu, data.selected_overlay_id);
    return get_overlay_text(&overlay_item);
}
