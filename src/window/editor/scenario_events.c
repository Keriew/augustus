#include "scenario_events.h"

#include "assets/assets.h"
#include "core/config.h"
#include "core/lang.h"
#include "core/string.h"
#include "editor/editor.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/scrollbar.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "scenario/editor_events.h"
#include "scenario/event/event.h"
#include "scenario/event/controller.h"
#include "scenario/event/parameter_data.h"
#include "scenario/property.h"
#include "window/city.h"
#include "window/editor/attributes.h"
#include "window/editor/map.h"
#include "window/editor/custom_variables.h"
#include "window/editor/scenario_event_details.h"
#include "window/file_dialog.h"
#include "window/numeric_input.h"
#include "window/popup_dialog.h"

#define WINDOW_WIDTH 27 // in blocks
#define WINDOW_HEIGHT 39

#define EVENTS_Y_OFFSET 116
#define EVENTS_ROW_HEIGHT 31
#define MAX_VISIBLE_ROWS 10
#define BUTTON_WIDTH 320

typedef struct {
    scenario_event_t *event;
    int selected;
} event_list_item;

typedef enum {
    CHECKBOX_NO_SELECTION = 0,
    CHECKBOX_SOME_SELECTED = 1,
    CHECKBOX_ALL_SELECTED = 2
} checkbox_selection_type;

static void on_scroll(void);
static void button_click(const generic_button *button);
static void button_event(const generic_button *button);
static void button_open_variables(const generic_button *button);
static void populate_list(void);
static void add_new_event(void);

static scrollbar_type scrollbar = {
    395, EVENTS_Y_OFFSET, EVENTS_ROW_HEIGHT * MAX_VISIBLE_ROWS, BUTTON_WIDTH - 17, MAX_VISIBLE_ROWS, on_scroll, 0, 4
};

static generic_button buttons[] = {
    {44, EVENTS_Y_OFFSET + (0 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH + 24, EVENTS_ROW_HEIGHT, button_event, 0, 1}, // adding 24 because of the extra space for the checkbox
    {44, EVENTS_Y_OFFSET + (1 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH + 24, EVENTS_ROW_HEIGHT, button_event, 0, 2},
    {44, EVENTS_Y_OFFSET + (2 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH + 24, EVENTS_ROW_HEIGHT, button_event, 0, 3},
    {44, EVENTS_Y_OFFSET + (3 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH + 24, EVENTS_ROW_HEIGHT, button_event, 0, 4},
    {44, EVENTS_Y_OFFSET + (4 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH + 24, EVENTS_ROW_HEIGHT, button_event, 0, 5},
    {44, EVENTS_Y_OFFSET + (5 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH + 24, EVENTS_ROW_HEIGHT, button_event, 0, 6},
    {44, EVENTS_Y_OFFSET + (6 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH + 24, EVENTS_ROW_HEIGHT, button_event, 0, 7},
    {44, EVENTS_Y_OFFSET + (7 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH + 24, EVENTS_ROW_HEIGHT, button_event, 0, 8},
    {44, EVENTS_Y_OFFSET + (8 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH + 24, EVENTS_ROW_HEIGHT, button_event, 0, 9},
    {44, EVENTS_Y_OFFSET + (9 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH + 24, EVENTS_ROW_HEIGHT, button_event, 0, 10},
    {68, EVENTS_Y_OFFSET + (11 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH, EVENTS_ROW_HEIGHT, button_click, 0, 11}, // add new
    {68, EVENTS_Y_OFFSET + (14 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH, EVENTS_ROW_HEIGHT, button_click, 0, 12}, // delete selected
    {68, EVENTS_Y_OFFSET + (15 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH / 2, EVENTS_ROW_HEIGHT, button_click, 0, 13}, // import
    {68 + BUTTON_WIDTH / 2, EVENTS_Y_OFFSET + (15 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH / 2, EVENTS_ROW_HEIGHT, button_click, 0, 14}, // export
    {WINDOW_WIDTH * BLOCK_SIZE - BUTTON_WIDTH / 2, 60, BUTTON_WIDTH / 2, EVENTS_ROW_HEIGHT, button_open_variables}, // variables
    {68, EVENTS_Y_OFFSET + (13 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH / 2, EVENTS_ROW_HEIGHT, button_click, 0, 15}, // copy
    {68 + BUTTON_WIDTH / 2, EVENTS_Y_OFFSET + (13 * EVENTS_ROW_HEIGHT), BUTTON_WIDTH / 2, EVENTS_ROW_HEIGHT, button_click, 0, 16}, // paste
    {44, EVENTS_Y_OFFSET - 24, 20, 20, button_click, 0, 17} // select all/none
};
#define MAX_BUTTONS (sizeof(buttons) / sizeof(generic_button))

static struct {
    unsigned int focus_button_id;
    unsigned int total_events;
    unsigned int active_events;
    array(event_list_item) list;
    checkbox_selection_type selection_type;
    int do_not_ask_again_for_delete_event;
    int did_copy_events;
} data;

static void update_selection_type(void);

static void count_active_events(void)
{
    data.active_events = 0;
    event_list_item *list_item;
    array_foreach(data.list, list_item) {
        if (!list_item->event) {
            continue;
        }
        if (list_item->event->state == EVENT_STATE_UNDEFINED) {
            continue;
        }
        data.active_events++;
    }
}

static void init_list(void)
{
    data.total_events = scenario_events_get_count();
    array_init(data.list, sizeof(event_list_item), 0, 0);
    populate_list();
    count_active_events();
    update_selection_type();
    scrollbar_init(&scrollbar, 0, data.total_events);
}

static void init(void)
{
    scenario_events_parameter_data_sort_alphabetically();
    init_list();
}

static void populate_list(void)
{
    for (unsigned int i = 0; i < data.total_events; i++) {
        event_list_item *item;
        array_new_item(data.list, item);
        item->event = scenario_event_get(i);
        item->selected = 0;
    }
}

static void add_new_event(void)
{
    scenario_event_t *event = scenario_event_create(0, 0, 0);
    array_advance(event->condition_groups);

    init_list();
    window_request_refresh();
}

static void draw_background(void)
{
    window_editor_map_draw_all();
}

static color_t color_from_state(event_state state)
{
    if (!editor_is_active()) {
        if (state == EVENT_STATE_ACTIVE) {
            return COLOR_MASK_GREEN;
        } else if (state == EVENT_STATE_PAUSED) {
            return COLOR_MASK_RED;
        } else if (state == EVENT_STATE_DISABLED) {
            return COLOR_MASK_GRAY;
        }
    }
    return COLOR_MASK_NONE;
}

static void draw_foreground(void)
{
    graphics_in_dialog_with_size(WINDOW_WIDTH * BLOCK_SIZE, WINDOW_HEIGHT * BLOCK_SIZE);

    outer_panel_draw(16, 16, WINDOW_WIDTH, WINDOW_HEIGHT);

    text_draw_centered(translation_for(TR_EDITOR_SCENARIO_EVENTS_TITLE), 0, 30, WINDOW_WIDTH * BLOCK_SIZE, FONT_LARGE_BLACK, 0);
    text_draw_label_and_number(translation_for(TR_EDITOR_SCENARIO_EVENTS_COUNT), data.total_events, "",
        30, 70, FONT_NORMAL_PLAIN, COLOR_BLACK);

    int checkmark_id = assets_lookup_image_id(ASSET_UI_SELECTION_CHECKMARK);
    const image *img = image_get(checkmark_id);
    if (data.selection_type == CHECKBOX_SOME_SELECTED) {
        text_draw(string_from_ascii("-"), buttons[17].x + 8, buttons[17].y + 4,
            FONT_NORMAL_BLACK, 0);
    } else if (data.selection_type == CHECKBOX_ALL_SELECTED) {
        image_draw(checkmark_id, buttons[17].x + (20 - img->original.width) / 2,
             buttons[17].y + (20 - img->original.height) / 2, COLOR_MASK_NONE, SCALE_NONE);
    }

    for (unsigned int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        event_list_item *list_item = array_item(data.list, i + scrollbar.scroll_position);
        if (list_item->event) {
            // Checkbox
            if (list_item->event->state != EVENT_STATE_UNDEFINED) {
                int selection_button_y_offset = (buttons[i].height - 20) / 2;
                button_border_draw(buttons[i].x, buttons[i].y + selection_button_y_offset, 20, 20,
                    data.focus_button_id == i + 1 && mouse_in_dialog(mouse_get())->x < 64);
                if (list_item->selected) {
                    image_draw(checkmark_id, buttons[i].x + (20 - img->original.width) / 2,
                        buttons[i].y + selection_button_y_offset + (20 - img->original.height) / 2, COLOR_MASK_NONE, SCALE_NONE);
                }
            }
            // Label and text
            large_label_draw(68, buttons[i].y, (buttons[i].width - 24) / BLOCK_SIZE, data.focus_button_id == i + 1);
            color_t color = color_from_state(list_item->event->state);

            if (list_item->event->state != EVENT_STATE_UNDEFINED) {
                text_draw_number(list_item->event->id, 0, "", 68 + 6, buttons[i].y + 8, FONT_NORMAL_GREEN, color);
                if (!*list_item->event->name) {
                    text_draw_label_and_number(translation_for(TR_EDITOR_SCENARIO_EVENTS_CONDITIONS),
                        scenario_event_count_conditions(list_item->event), "", 120, buttons[i].y + 8,
                        FONT_NORMAL_GREEN, COLOR_MASK_NONE);
                    text_draw_label_and_number(translation_for(TR_EDITOR_SCENARIO_EVENTS_ACTIONS),
                        list_item->event->actions.size, "", 270, buttons[i].y + 8, FONT_NORMAL_GREEN, COLOR_MASK_NONE);
                } else {
                    text_draw(list_item->event->name, 120, buttons[i].y + 8, FONT_NORMAL_GREEN, color);
                }
            } else {
                text_draw_centered(translation_for(TR_EDITOR_SCENARIO_EVENT_DELETED), 68, buttons[i].y + 8,
                    BUTTON_WIDTH, FONT_NORMAL_GREEN, 0);
            }
        }
    }

    for (size_t i = 10; i < MAX_BUTTONS - 1; i++) {
        large_label_draw(buttons[i].x, buttons[i].y, buttons[i].width / BLOCK_SIZE, data.focus_button_id == i + 1 ? 1 : 0);
    }
    if (data.active_events > 0) {
        button_border_draw(buttons[17].x, buttons[17].y, 20, 20, data.focus_button_id == 16);
    }
    generic_button *btn = &buttons[10];
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENTS_ADD, btn->x, btn->y + 8, btn->width, FONT_NORMAL_GREEN);

    btn = &buttons[11];
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENTS_DELETE_SELECTED, btn->x, btn->y + 8, btn->width, FONT_NORMAL_GREEN);

    btn = &buttons[12];
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENTS_IMPORT, btn->x, btn->y + 8, btn->width, FONT_NORMAL_GREEN);

    btn = &buttons[13];
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENTS_EXPORT, btn->x, btn->y + 8, btn->width, FONT_NORMAL_GREEN);

    btn = &buttons[14];
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_CUSTOM_VARIABLES_TITLE, btn->x, btn->y + 8, btn->width, FONT_NORMAL_GREEN);

    btn = &buttons[15];
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENTS_COPY_SELECTED, btn->x, btn->y + 8, btn->width, FONT_NORMAL_GREEN);

    btn = &buttons[16];
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENTS_PASTE_SELECTED, btn->x, btn->y + 8, btn->width, FONT_NORMAL_GREEN);

    lang_text_draw_centered(13, 3, 0, WINDOW_HEIGHT * BLOCK_SIZE - 8, WINDOW_WIDTH * BLOCK_SIZE, FONT_NORMAL_BLACK); // Right-click to Continue

    scrollbar_draw(&scrollbar);
    graphics_reset_dialog();
}

static void update_selection_type(void)
{
    uint8_t some_selected = 0;
    uint8_t all_selected = 1;
    event_list_item *list_item;
    array_foreach(data.list, list_item) {
        if (!list_item->event) {
            continue;
        }
        if (list_item->event->state == EVENT_STATE_UNDEFINED) {
            continue;
        }
        some_selected |= list_item->selected;
        all_selected &= list_item->selected;
        if (some_selected != all_selected) {
            data.selection_type = CHECKBOX_SOME_SELECTED;
            return;
        }
    }
    data.selection_type = some_selected ? CHECKBOX_ALL_SELECTED : CHECKBOX_NO_SELECTION;
}

static void button_event(const generic_button *button)
{
    int target_index = button->parameter1 - 1 + scrollbar.scroll_position;
    event_list_item *list_item = array_item(data.list, target_index);
    if (!list_item->event) {
        return;
    }
    if (list_item->event->state == EVENT_STATE_UNDEFINED) {
        return;
    }
    const mouse *m = mouse_in_dialog(mouse_get());
    if (m->x < 68) {
        if (m->x < 64) {
            list_item->selected = !list_item->selected;
            update_selection_type();
        }
        return; // don't access event when ticking the checkbox
    }
    window_editor_scenario_event_details_show(list_item->event->id);
}

static void on_scroll(void)
{
    window_request_refresh();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    if (scrollbar_handle_mouse(&scrollbar, m_dialog, 1) ||
        generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, MAX_BUTTONS, &data.focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        window_editor_attributes_show();
    }
}

static void delete_selected(int is_ok, int checked)
{
    if (!is_ok) {
        return;
    }
    if (checked) {
        data.do_not_ask_again_for_delete_event = 1;
    }
    event_list_item *item;
    array_foreach(data.list, item) {
        if (!item->event) {
            continue;
        }
        if (!item->selected) {
            continue;
        }
        scenario_event_delete(item->event);
    }
    count_active_events();
    update_selection_type();
}

static void select_all(void)
{
    event_list_item *list_item;
    array_foreach(data.list, list_item) {
        list_item->selected = 1;
    }
    data.selection_type = CHECKBOX_ALL_SELECTED;
}

static void select_none(void)
{
    event_list_item *list_item;
    array_foreach(data.list, list_item) {
        list_item->selected = 0;
    }
    data.selection_type = CHECKBOX_NO_SELECTION;
}

static void copy_selected(void)
{

}

static void paste_selected(void)
{

}

static void button_click(const generic_button *button)
{
    int type = button->parameter1;
    if (type == 11) {
        add_new_event();
    } else if (type == 12) {
        if (data.selection_type == CHECKBOX_NO_SELECTION) {
            return;
        }
        if (!data.do_not_ask_again_for_delete_event && config_get(CONFIG_UI_EDITOR_SHOW_DELETION_WARNINGS)) {
            const uint8_t *title = lang_get_string(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENTS_DELETE_EVENTS_CONFIRM_TITLE);
            const uint8_t *text = lang_get_string(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENTS_DELETE_EVENTS_CONFIRM_TEXT);
            const uint8_t *check_text = lang_get_string(CUSTOM_TRANSLATION, TR_SAVE_DIALOG_OVERWRITE_FILE_DO_NOT_ASK_AGAIN);
            window_popup_dialog_show_confirmation(title, text, check_text, delete_selected);
        } else {
            delete_selected(1, 1);
        }
        data.total_events = scenario_events_get_count();
    } else if (type == 13) {
        window_file_dialog_show(FILE_TYPE_SCENARIO_EVENTS, FILE_DIALOG_LOAD);
    } else if (type == 14) {
        window_file_dialog_show(FILE_TYPE_SCENARIO_EVENTS, FILE_DIALOG_SAVE);
    } else if (type == 15) {
        if (data.selection_type == CHECKBOX_NO_SELECTION) {
            return;
        }
        copy_selected();
    } else if (type == 16) {
        if (!data.did_copy_events) {
            return;
        }
        paste_selected();
    } else if (type == 17) {
        if (data.selection_type == CHECKBOX_ALL_SELECTED) {
            select_none();
        } else {
            select_all();
        }
    }
}

static void button_open_variables(const generic_button *button)
{
    window_editor_custom_variables_show(0);
}

static void handle_check_all_none_tooltip(tooltip_context *c)
{
    if (data.focus_button_id == 16) {
        if (data.active_events > 0) {
            c->precomposed_text = lang_get_string(CUSTOM_TRANSLATION,
                data.selection_type == CHECKBOX_ALL_SELECTED ? TR_SELECT_NONE : TR_SELECT_ALL);
            c->type = TOOLTIP_BUTTON;
        }
    }
}

static void get_tooltip(tooltip_context *c)
{
    handle_check_all_none_tooltip(c);
}

void window_editor_scenario_events_show(void)
{
    window_type window = {
        WINDOW_EDITOR_SCENARIO_EVENTS,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    init();
    window_show(&window);
}
