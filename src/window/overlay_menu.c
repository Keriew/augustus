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
#define SUBMENU_X_OFFSET 350
#define MENU_Y_OFFSET 72
#define MENU_ITEM_HEIGHT 24
#define MENU_CLICK_MARGIN 20
#define MENU_ITEM_WIDTH 160
#define TOP_MARGIN 74
#define LABEL_WIDTH_BLOCKS 10
#define SIDEBAR_MARGIN_X 10
#define MAX_BUTTONS 64
#define HOVER_TIMEOUT_MILLIS 900
#define NO_ITEM -1
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

    int independent_y;
} overlay_menu_entry;


static struct {
    int selected_overlay_id;

    int selected_main_overlay;
    int selected_submenu_overlay; 
    int selected_submenu2_overlay;

    int clicked_menu_item;

    int hovered_overlay_id;

    int show_menu;
    time_millis hover_time;

    generic_button buttons_main[MAX_BUTTONS];
    generic_button buttons_sub[MAX_BUTTONS];
    generic_button buttons_sub2[MAX_BUTTONS];

    unsigned int focus_main;
    unsigned int focus_sub;
    unsigned int focus_sub2;
} data;


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

static overlay_menu_entry find_overlay(const overlay_menu_entry *entries, const int overlay_id)
{
    for (unsigned i = 0; entries[i].overlay != NO_ITEM; i++) {
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

static int find_overlay_path(const overlay_menu_entry *entries, int target, int *out_main, int *out_sub, int *out_sub2)
{
    for (int i = 0; entries[i].overlay != NO_ITEM; i++) {
        const overlay_menu_entry *e = &entries[i];

        if (e->overlay == target) {
            *out_main = target;
            *out_sub = 0;
            *out_sub2 = 0;
            return 1;
        }

        if (e->submenu) {
            for (int j = 0; e->submenu[j].overlay != NO_ITEM; j++) {
                const overlay_menu_entry *s = &e->submenu[j];

                if (s->overlay == target) {
                    *out_main = e->overlay;
                    *out_sub = s->overlay;
                    *out_sub2 = 0;
                    return 1;
                }

                if (s->submenu) {
                    for (int k = 0; s->submenu[k].overlay != NO_ITEM; k++) {
                        if (s->submenu[k].overlay == target) {
                            *out_main = e->overlay;
                            *out_sub = s->overlay;
                            *out_sub2 = target;
                            return 1;
                        }
                    }
                }
            }
        }
    }
    return 0;
}

static void handle_hover_menu(void)
{
    time_millis now = time_get_millis();

    if (data.focus_main > 0) {
        int idx = data.focus_main - 1;
        data.hovered_overlay_id = data.buttons_main[idx].parameter1;

        // only update selection if NOT sticky
        if (data.clicked_menu_item == NO_ITEM) {
            data.selected_main_overlay = data.hovered_overlay_id;
        }

        data.hover_time = now;
    }

    if (data.focus_sub > 0) {
        int idx = data.focus_sub - 1;
        int id = data.buttons_sub[idx].parameter1;
        data.hovered_overlay_id = id;

        if (data.clicked_menu_item == NO_ITEM) {
            data.selected_submenu_overlay = id;
        }

        data.hover_time = now;
    }

    if (data.focus_sub2 > 0) {
        int idx = data.focus_sub2 - 1;
        int id = data.buttons_sub2[idx].parameter1;
        data.hovered_overlay_id = id;

        if (data.clicked_menu_item == NO_ITEM) {
            data.selected_submenu2_overlay = id;
        }

        data.hover_time = now;
    }

}

static void handle_hover_timeout(void)
{
    if (data.clicked_menu_item != NO_ITEM) {
        return; // disable timeout in sticky mode
    }

    if (data.focus_main == 0 &&
        data.focus_sub == 0 &&
        data.focus_sub2 == 0) {
        if (time_get_millis() - data.hover_time > HOVER_TIMEOUT_MILLIS) {
            data.selected_main_overlay = 0;
            data.selected_submenu_overlay = 0;
            data.selected_submenu2_overlay = 0;
        }
    }
}

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
    return data.hovered_overlay_id == entry->overlay;
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

static void draw_menu_item(const overlay_menu_entry *entry, const int i, const int x_offset, const int y_offset, const int button_index, generic_button *buttons)
{
    const int x = x_offset - MENU_ITEM_WIDTH;
    const int y = y_offset + MENU_ITEM_HEIGHT * i;

    buttons[button_index] = (generic_button) {
        .x = (short) x,
        .y = (short) y,
        .width = MENU_ITEM_WIDTH,
        .height = MENU_ITEM_HEIGHT,
        .left_click_handler = button_menu_item,
        .parameter1 = entry->overlay,
    };

    int is_hovered = is_mouse_hovering(entry) || data.hovered_overlay_id == entry->overlay;

    label_draw(x, y, LABEL_WIDTH_BLOCKS, is_hovered ? LABEL_TYPE_NORMAL : LABEL_TYPE_HOVER);

    text_draw_centered(get_overlay_text(entry), x_offset - MENU_ITEM_WIDTH, y + 4,
        MENU_ITEM_WIDTH, FONT_NORMAL_GREEN, COLOR_MASK_NONE);

    if (entry->submenu != NULL) {
        const int image_id = assets_get_image_id("UI", "Expand Menu Icon Left");
        image_draw(image_id, x + 3, y + 3, COLOR_MASK_NONE, SCALE_NONE);
    }
}

static int draw_menu(const overlay_menu_entry *entries, int x_offset, int y_offset, generic_button *buttons)
{
    int button_index = 0;

    for (int i = 0; entries[i].overlay != NO_ITEM; i++) {
        draw_menu_item(&entries[i], i, x_offset, y_offset, button_index++, buttons);
    }

    return button_index;
}

static void draw_foreground(void)
{
    window_city_draw();

    if (!data.show_menu) {
        return;
    }

    const int base_x = get_sidebar_x_offset() - SIDEBAR_MARGIN_X;

    // --- MAIN MENU ---
    draw_menu(overlay_menu, base_x, TOP_MARGIN, data.buttons_main);

    // --- MAIN SELECTED ---
    const overlay_menu_entry main_selected =
        find_overlay(overlay_menu, data.selected_main_overlay);

    if (main_selected.submenu == NULL) {
        return;
    }

    // --- MAIN INDEX ---
    int main_index = 0;
    for (int i = 0; overlay_menu[i].overlay != NO_ITEM; i++) {
        if (overlay_menu[i].overlay == main_selected.overlay) {
            main_index = i;
            break;
        }
    }

    // --- MAIN MARKER ---
    const int main_marker_x = base_x - MENU_ITEM_WIDTH - 15;
    const int main_marker_y = TOP_MARGIN + MENU_ITEM_HEIGHT * main_index + 6;

    image_draw(image_group(GROUP_BULLET), main_marker_x, main_marker_y, COLOR_MASK_NONE, SCALE_NONE);

    // --- SUBMENU HEIGHT ---
    int submenu_height = 0;
    for (int i = 0; main_selected.submenu[i].overlay != NO_ITEM; i++) {
        submenu_height++;
    }
    submenu_height *= MENU_ITEM_HEIGHT;

    // --- SCREEN BOUNDS ---
    int view_x, view_y, view_width, view_height;
    city_view_get_viewport(&view_x, &view_y, &view_width, &view_height);

    int submenu_y;

    if (main_selected.submenu[0].independent_y) {
        submenu_y = TOP_MARGIN;
    } else {
        submenu_y = TOP_MARGIN + MENU_ITEM_HEIGHT * main_index;

        int bottom = submenu_y + submenu_height;
        int screen_bottom = view_y + view_height;

        if (bottom > screen_bottom) {
            submenu_y = screen_bottom - submenu_height;
            if (submenu_y < TOP_MARGIN) {
                submenu_y = TOP_MARGIN;
            }
        }
    }

    // --- SUBMENU (LEVEL 2) ---
    const int submenu_x =
        base_x - SUBMENU_X_OFFSET + MENU_X_OFFSET;

    draw_menu(main_selected.submenu, submenu_x, submenu_y, data.buttons_sub);

    // --- SUB SELECTED ---
    const overlay_menu_entry sub_selected =
        find_overlay(main_selected.submenu, data.selected_submenu_overlay);

    if (sub_selected.overlay == NO_ITEM) {
        // do not kill submenu1 if sticky is active
        if (data.clicked_menu_item == NO_ITEM) {
            return;
        }
    }

    // --- SUB INDEX ---
    int sub_index = 0;
    for (int i = 0; main_selected.submenu[i].overlay != NO_ITEM; i++) {
        if (main_selected.submenu[i].overlay == sub_selected.overlay) {
            sub_index = i;
            break;
        }
    }

    // --- SUBSUBMENU (LEVEL 3) ---
    if (sub_selected.submenu != NULL) {

        // --- SUB MARKER (drawn only if subsubmenu exists) ---
        const int sub_marker_x = submenu_x - MENU_ITEM_WIDTH - 15;
        const int sub_marker_y = submenu_y + MENU_ITEM_HEIGHT * sub_index + 6;

        image_draw(image_group(GROUP_BULLET), sub_marker_x, sub_marker_y, COLOR_MASK_NONE, SCALE_NONE);

        const int subsubmenu_x = submenu_x - MENU_ITEM_WIDTH - 20;

        draw_menu(sub_selected.submenu, subsubmenu_x, submenu_y, data.buttons_sub2);
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

    handled |= generic_buttons_handle_mouse(m, 0, 0, data.buttons_main, MAX_BUTTONS, &data.focus_main);

    handled |= generic_buttons_handle_mouse(m, 0, 0, data.buttons_sub, MAX_BUTTONS, &data.focus_sub);

    handled |= generic_buttons_handle_mouse(m, 0, 0, data.buttons_sub2, MAX_BUTTONS, &data.focus_sub2);

    handle_hover_menu();      // hover open
    handle_hover_timeout();   // close

    if (!handled && click_outside_menu(m, x_offset)) {
        data.selected_main_overlay = 0;
        data.selected_submenu_overlay = 0;
        data.selected_submenu2_overlay = 0;
        data.clicked_menu_item = NO_ITEM;

        hide_menu();
        window_city_show();
    }

    if (!data.show_menu) {
        show_menu();
    }
}

static void button_menu_item(const generic_button *button)
{
    int main_id = 0;
    int sub_id = 0;
    int sub2_id = 0;

    find_overlay_path(overlay_menu, button->parameter1, &main_id, &sub_id, &sub2_id);

    const overlay_menu_entry selected =
        find_overlay(overlay_menu, button->parameter1);

    const int clicked_main = (button->parameter1 == main_id);

    const int has_submenu = (selected.submenu != NULL && selected.submenu[0].overlay != NO_ITEM);

    const int is_same_main = (data.selected_main_overlay == main_id);

    // =========================
    // CLICK ON MAIN
    // =========================
    if (clicked_main) {
        // toggle same main (reset all)
        if (is_same_main && data.clicked_menu_item != NO_ITEM) {
            data.selected_main_overlay = 0;
            data.selected_submenu_overlay = 0;
            data.selected_submenu2_overlay = 0;
            data.clicked_menu_item = NO_ITEM;
            return;
        }

        // reset sticky state BEFORE applying new selection
        data.clicked_menu_item = NO_ITEM;

        data.selected_main_overlay = main_id;
        data.selected_submenu_overlay = 0;
        data.selected_submenu2_overlay = 0;

        // =========================
        // CASE 1: MAIN WITHOUT SUBMENU
        // =========================
        if (!has_submenu) {
            data.selected_overlay_id = selected.overlay;

            hide_menu();
            game_state_set_overlay(selected.overlay);
            window_city_show();
            return;
        }

        // =========================
        // CASE 2: MAIN WITH SUBMENU
        // =========================
        const overlay_menu_entry *sub1 = &selected.submenu[0];

        if (sub1->overlay != NO_ITEM) {
            data.selected_submenu_overlay = sub1->overlay;

            // sub2 auto-open if exists
            if (sub1->submenu &&
                sub1->submenu[0].overlay != NO_ITEM) {
                data.selected_submenu2_overlay =
                    sub1->submenu[0].overlay;

                data.clicked_menu_item =
                    sub1->submenu[0].overlay;
            } else {
                data.clicked_menu_item =
                    sub1->overlay;
            }
        }

        show_menu();
        return;
    }

    // =========================
    // CLICK ON SUB / SUB2 / LEAF
    // =========================

    // IMPORTANT:
    // allow immediate override even if something was sticky
    data.clicked_menu_item = NO_ITEM;

    data.selected_main_overlay = main_id;
    data.selected_submenu_overlay = sub_id;
    data.selected_submenu2_overlay = sub2_id;

    const int has_children = (selected.submenu != NULL);

    if (has_children) {
        show_menu();
        return;
    }

    // =========================
    // LEAF FINAL SELECTION
    // =========================
    data.selected_overlay_id = selected.overlay;
    data.selected_submenu_overlay = 0;
    data.selected_submenu2_overlay = 0;

    hide_menu();
    game_state_set_overlay(selected.overlay);
    window_city_show();
}

void window_overlay_menu_show(void)
{
    const window_type window = {WINDOW_OVERLAY_MENU, draw_background, draw_foreground, handle_input};
    data.clicked_menu_item = NO_ITEM;
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
