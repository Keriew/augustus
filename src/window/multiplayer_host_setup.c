#ifdef ENABLE_MULTIPLAYER

#include "multiplayer_host_setup.h"

#include "core/dir.h"
#include "core/encoding.h"
#include "core/file.h"
#include "core/string.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/list_box.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "multiplayer/bootstrap.h"
#include "multiplayer/scenario_selection.h"
#include "multiplayer/mp_debug_log.h"
#include "translation/translation.h"
#include "window/multiplayer_lobby.h"

#include <string.h>
#include <stdio.h>

#define PANEL_X 64
#define PANEL_Y 40
#define PANEL_WIDTH_BLOCKS 30
#define PANEL_HEIGHT_BLOCKS 24

#define LIST_X (PANEL_X + 16)
#define LIST_Y (PANEL_Y + 80)
#define LIST_WIDTH_BLOCKS 26
#define LIST_HEIGHT_BLOCKS 14
#define LIST_ITEM_HEIGHT 16

#define CONTINUE_BUTTON_X (PANEL_X + 16)
#define CONTINUE_BUTTON_Y (PANEL_Y + PANEL_HEIGHT_BLOCKS * 16 - 48)
#define BACK_BUTTON_X (PANEL_X + 320)
#define BACK_BUTTON_Y CONTINUE_BUTTON_Y
#define BUTTON_WIDTH 160
#define BUTTON_HEIGHT 24

static void button_continue(const generic_button *button);
static void button_back(const generic_button *button);
static void draw_scenario_item(const list_box_item *item);
static void select_scenario(unsigned int index, int is_double_click);

static generic_button buttons[] = {
    {CONTINUE_BUTTON_X, CONTINUE_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, button_continue, 0, 0},
    {BACK_BUTTON_X, BACK_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, button_back, 0, 0},
};

static list_box_type scenario_list = {
    .x = LIST_X,
    .y = LIST_Y,
    .width_blocks = LIST_WIDTH_BLOCKS,
    .height_blocks = LIST_HEIGHT_BLOCKS,
    .item_height = LIST_ITEM_HEIGHT,
    .draw_inner_panel = 1,
    .extend_to_hidden_scrollbar = 1,
    .decorate_scrollbar = 1,
    .draw_item = draw_scenario_item,
    .on_select = select_scenario,
    .handle_tooltip = 0
};

static struct {
    unsigned int focus_button_id;
    const dir_listing *scenarios;
    char selected_filename[FILE_NAME_MAX];
    uint8_t selected_display[FILE_NAME_MAX];
    int has_selection;
    int eligible_cities;    /* Eligible trade cities for selected scenario (-1 = unknown) */
    int validation_done;    /* 1 if eligibility has been checked */
} data;

static void init(void)
{
    data.scenarios = dir_find_files_with_extension_at_location(
        PATH_LOCATION_SCENARIO, "map");
    data.scenarios = dir_append_files_with_extension("mapx");
    data.has_selection = 0;
    data.selected_filename[0] = '\0';
    data.selected_display[0] = 0;
    data.eligible_cities = -1;
    data.validation_done = 0;

    list_box_init(&scenario_list, data.scenarios->num_files);

    /* Pre-select first item if available */
    if (data.scenarios->num_files > 0) {
        list_box_select_index(&scenario_list, 0);
        select_scenario(0, 0);
    }
}

static void draw_scenario_item(const list_box_item *item)
{
    if ((int)item->index >= data.scenarios->num_files) {
        return;
    }

    uint8_t displayable_file[FILE_NAME_MAX];
    font_t font = item->is_selected ? FONT_NORMAL_WHITE : FONT_NORMAL_GREEN;
    encoding_from_utf8(data.scenarios->files[item->index].name,
                       displayable_file, FILE_NAME_MAX);
    file_remove_extension((char *)displayable_file);
    text_ellipsize(displayable_file, font, item->width);
    text_draw(displayable_file, item->x, item->y, font, 0);

    if (item->is_focused) {
        button_border_draw(item->x - 4, item->y - 4,
                           item->width + 6, item->height + 4, 1);
    }
}

static void select_scenario(unsigned int index, int is_double_click)
{
    if ((int)index >= data.scenarios->num_files) {
        return;
    }

    snprintf(data.selected_filename, FILE_NAME_MAX, "%s",
             data.scenarios->files[index].name);
    encoding_from_utf8(data.selected_filename, data.selected_display,
                       FILE_NAME_MAX);
    file_remove_extension((char *)data.selected_display);
    data.has_selection = 1;
    data.eligible_cities = -1;
    data.validation_done = 0;

    /* Store in bootstrap for later use */
    char ascii_name[FILE_NAME_MAX];
    snprintf(ascii_name, FILE_NAME_MAX, "%s", data.selected_filename);
    file_remove_extension(ascii_name);
    mp_bootstrap_set_scenario(ascii_name);

    window_invalidate();

    if (is_double_click) {
        button_continue(0);
    }
}

static void button_continue(const generic_button *button)
{
    if (!data.has_selection) {
        return;
    }

    MP_LOG_INFO("UI", "Host setup: proceeding to lobby with scenario '%s'",
                data.selected_filename);

    /* Scenario validation will happen at game start time (in bootstrap)
     * when we know the actual player count. Proceed to lobby. */
    window_multiplayer_lobby_show();
}

static void button_back(const generic_button *button)
{
    window_go_back();
}

static void draw_background(void)
{
    graphics_in_dialog();
    outer_panel_draw(PANEL_X, PANEL_Y, PANEL_WIDTH_BLOCKS, PANEL_HEIGHT_BLOCKS);

    /* Title */
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_HOST_SETUP_TITLE,
        PANEL_X, PANEL_Y + 14, PANEL_WIDTH_BLOCKS * 16, FONT_LARGE_BLACK);

    /* Select label */
    lang_text_draw(CUSTOM_TRANSLATION, TR_MP_HOST_SETUP_SELECT_SCENARIO,
        LIST_X, LIST_Y - 20, FONT_NORMAL_BLACK);

    /* Selected scenario display */
    int info_y = PANEL_Y + 48;
    if (data.has_selection) {
        const uint8_t *selected_label = translation_for(TR_MP_HOST_SETUP_SELECTED);
        text_draw(selected_label, PANEL_X + 260, info_y, FONT_NORMAL_BLACK, 0);
        text_draw(data.selected_display, PANEL_X + 330, info_y,
                  FONT_NORMAL_GREEN, 0);
    } else {
        lang_text_draw(CUSTOM_TRANSLATION, TR_MP_HOST_SETUP_NO_SCENARIO,
            PANEL_X + 260, info_y, FONT_NORMAL_RED);
    }

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    /* Scenario list */
    list_box_draw(&scenario_list);

    /* Continue button */
    int can_continue = data.has_selection;
    large_label_draw(CONTINUE_BUTTON_X, CONTINUE_BUTTON_Y,
        BUTTON_WIDTH / 16, data.focus_button_id == 1 ? 1 : 0);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_HOST_SETUP_BUTTON_CONTINUE,
        CONTINUE_BUTTON_X, CONTINUE_BUTTON_Y + 5, BUTTON_WIDTH,
        can_continue ? FONT_NORMAL_GREEN : FONT_NORMAL_PLAIN);

    /* Back button */
    large_label_draw(BACK_BUTTON_X, BACK_BUTTON_Y,
        BUTTON_WIDTH / 16, data.focus_button_id == 2 ? 1 : 0);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_HOST_SETUP_BUTTON_BACK,
        BACK_BUTTON_X, BACK_BUTTON_Y + 5, BUTTON_WIDTH, FONT_NORMAL_GREEN);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    if (list_box_handle_input(&scenario_list, m_dialog, 1)) {
        return;
    }

    if (generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, 2,
                                      &data.focus_button_id)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        button_back(0);
    }
}

void window_multiplayer_host_setup_show(void)
{
    memset(&data, 0, sizeof(data));
    mp_bootstrap_init();
    init();

    window_type window = {
        WINDOW_MULTIPLAYER_HOST_SETUP,
        draw_background,
        draw_foreground,
        handle_input,
        0,
        0
    };
    window_show(&window);
}

#endif /* ENABLE_MULTIPLAYER */
