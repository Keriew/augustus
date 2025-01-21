#include "edit_request.h"

#include "game/resource.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "scenario/editor.h"
#include "scenario/property.h"
#include "scenario/request.h"
#include "window/editor/map.h"
#include "window/numeric_input.h"
#include "window/plain_message_dialog.h"
#include "window/select_list.h"

static void button_year(const generic_button *button);
static void button_amount(const generic_button *button);
static void button_resource(const generic_button *button);
static void button_deadline_years(const generic_button *button);
static void button_favor(const generic_button *button);
static void button_extension_months(const generic_button *button);
static void button_extension_disfavor(const generic_button *button);
static void button_ignored_disfavor(const generic_button *button);
static void button_delete(const generic_button *button);
static void button_cancel(const generic_button *button);
static void button_save(const generic_button *button);

#define MAX_POSSIBLE_ERRORS 3

static struct {
    scenario_request request;
    unsigned int focus_button_id;
    int is_new_request;
    resource_type avaialble_resources[RESOURCE_MAX];
    const uint8_t *errors[MAX_POSSIBLE_ERRORS];
} data;

#define NUM_BUTTONS (sizeof(buttons) / sizeof(generic_button))

static generic_button buttons[] = {
    {30, 186, 60, 25, button_year},
    {330, 186, 80, 25, button_amount},
    {430, 186, 100, 25, button_resource},
    {70, 224, 140, 25, button_deadline_years},
    {400, 224, 80, 25, button_favor},
    {400, 264, 80, 25, button_extension_months},
    {400, 304, 80, 25, button_extension_disfavor},
    {400, 344, 80, 25, button_ignored_disfavor},
    {16, 384, 250, 25, button_delete},
    {377, 384, 100, 25, button_cancel},
    {492, 384, 100, 25, button_save}
};

static void init(int id)
{
    const scenario_request *request = scenario_request_get(id);
    data.is_new_request = request->resource == RESOURCE_NONE;
    data.request = *request;
}

static void draw_background(void)
{
    window_editor_map_draw_all();

    graphics_in_dialog();

    outer_panel_draw(0, 100, 38, 20);
    lang_text_draw_centered(44, 21, 14, 114, 580, FONT_LARGE_BLACK);

    int width = text_get_number_width(data.request.id, 0, "", FONT_NORMAL_BLACK);
    width += lang_text_get_width(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENT_ID, FONT_NORMAL_BLACK);
    int base_x = (580 - width) / 2 + 14;

    width = lang_text_draw(CUSTOM_TRANSLATION, TR_EDITOR_SCENARIO_EVENT_ID, base_x, 154, FONT_NORMAL_BLACK);
    text_draw_number(data.request.id, 0, "", base_x + width, 154, FONT_NORMAL_BLACK, 0);

    text_draw_number_centered_prefix(data.request.year, '+', 30, 192, 60, FONT_NORMAL_BLACK);
    lang_text_draw_year(scenario_property_start_year() + data.request.year, 110, 192, FONT_NORMAL_BLACK);

    lang_text_draw(44, 72, 250, 192, FONT_NORMAL_BLACK);
    text_draw_number_centered(data.request.amount.min, 330, 192, 80, FONT_NORMAL_BLACK);

    text_draw_centered(resource_get_data(data.request.resource)->text, 430, 192, 100, FONT_NORMAL_BLACK,
        COLOR_MASK_NONE);

    lang_text_draw(44, 24, 40, 230, FONT_NORMAL_BLACK);

    lang_text_draw_amount_centered(8, 8, data.request.deadline_years, 70, 230, 140, FONT_NORMAL_BLACK);

    lang_text_draw(44, 73, 300, 230, FONT_NORMAL_BLACK);

    text_draw_number_centered_prefix(data.request.favor, '+', 400, 230, 80, FONT_NORMAL_BLACK);

    lang_text_draw(CUSTOM_TRANSLATION, TR_EDITOR_FAVOUR_EXTENSION_MONTHS, 70, 270, FONT_NORMAL_BLACK);
    text_draw_number_centered_prefix(data.request.extension_months_to_comply, '+', 400, 270, 80, FONT_NORMAL_BLACK);

    lang_text_draw(CUSTOM_TRANSLATION, TR_EDITOR_FAVOUR_DISFAVOR, 70, 310, FONT_NORMAL_BLACK);
    text_draw_number_centered_prefix(data.request.extension_disfavor, '-', 400, 310, 80, FONT_NORMAL_BLACK);

    lang_text_draw(CUSTOM_TRANSLATION, TR_EDITOR_FAVOUR_IGNORED, 70, 350, FONT_NORMAL_BLACK);
    text_draw_number_centered_prefix(data.request.ignored_disfavor, '-', 400, 350, 80, FONT_NORMAL_BLACK);
    lang_text_draw_centered(18, 3, 492, 390, 100, FONT_NORMAL_BLACK);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_BUTTON_CANCEL, 377, 390, 100, FONT_NORMAL_BLACK);

    lang_text_draw_centered_colored(44, 25, 16, 390, 250, FONT_NORMAL_PLAIN,
        data.is_new_request ? COLOR_FONT_LIGHT_GRAY : COLOR_RED);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    for (size_t i = 0; i < NUM_BUTTONS; i++) {
        int focus = data.focus_button_id == i + 1;
        if (i == 8 && data.is_new_request) {
            focus = 0;
        }
        button_border_draw(buttons[i].x, buttons[i].y, buttons[i].width, buttons[i].height, focus);
    }

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (generic_buttons_handle_mouse(mouse_in_dialog(m), 0, 0, buttons, NUM_BUTTONS, &data.focus_button_id)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        button_cancel(0);
        return;
    }
    if (h->enter_pressed) {
        button_save(0);
    }
}

static void set_year(int value)
{
    data.request.year = value;
}

static void button_year(const generic_button *button)
{
    window_numeric_input_show(0, 0, button, 3, 999, set_year);
}

static void set_amount(int value)
{
    data.request.amount.min = value;
    data.request.amount.max = value;
}

static void button_amount(const generic_button *button)
{
    int max_amount = 999;
    int max_digits = 3;
    if (data.request.resource == RESOURCE_DENARII) {
        max_amount = 30000;
        max_digits = 5;
    }
    window_numeric_input_show(0, 0, button, max_digits, max_amount, set_amount);
}

static void set_resource(int value)
{
    data.request.resource = data.avaialble_resources[value];
    if (data.request.amount.min > 999) {
        data.request.amount.min = 999;
        data.request.amount.max = 999;
    }
}

static void button_resource(const generic_button *button)
{
    static const uint8_t *resource_texts[RESOURCE_MAX + RESOURCE_TOTAL_SPECIAL];
    static int total_resources = 0;
    if (!total_resources) {
        for (resource_type resource = RESOURCE_NONE; resource < RESOURCE_MAX + RESOURCE_TOTAL_SPECIAL; resource++) {
            if ((!resource_is_storable(resource) && resource < RESOURCE_MAX) || resource == RESOURCE_TROOPS) {
                continue;
            }
            resource_texts[total_resources] = resource_get_data(resource)->text;
            data.avaialble_resources[total_resources] = resource;
            total_resources++;
        }
    }
    window_select_list_show_text(screen_dialog_offset_x(), screen_dialog_offset_y(), button,
        resource_texts, total_resources, set_resource);
}

static void set_deadline_years(int value)
{
    data.request.deadline_years = value;
}

static void button_deadline_years(const generic_button *button)
{
    window_numeric_input_show(0, 0, button, 3, 999, set_deadline_years);
}

static void set_favor(int value)
{
    data.request.favor = value;
}

static void button_favor(const generic_button *button)
{
    window_numeric_input_show(0, 0, button, 3, 100, set_favor);
}

static void set_extension_months(int value)
{
    data.request.extension_months_to_comply = value;
}

static void button_extension_months(const generic_button *button)
{
    window_numeric_input_show(0, 0, button, 3, 120, set_extension_months);
}

static void set_extension_disfavor(int value)
{
    data.request.extension_disfavor = value;
}

static void button_extension_disfavor(const generic_button *button)
{
    window_numeric_input_show(0, 0, button, 3, 100, set_extension_disfavor);
}

static void set_ignored_disfavor(int value)
{
    data.request.ignored_disfavor = value;
}

static void button_ignored_disfavor(const generic_button *button)
{
    window_numeric_input_show(0, 0, button, 3, 100, set_ignored_disfavor);
}

static void button_delete(const generic_button *button)
{
    if (data.is_new_request) {
        return;
    }
    scenario_request_delete(data.request.id);
    scenario_editor_set_as_unsaved();
    window_go_back();
}

static void button_cancel(const generic_button *button)
{
    window_go_back();
}

static unsigned int validate(void)
{
    unsigned int num_errors = 0;

    for (int i = 0; i < MAX_POSSIBLE_ERRORS; i++) {
        data.errors[i] = 0;
    }

    if (data.request.resource == RESOURCE_NONE) {
        data.errors[num_errors++] = translation_for(TR_EDITOR_EDIT_REQUEST_NO_RESOURCE);
    }
    if (data.request.amount.min <= 0) {
        data.errors[num_errors++] = translation_for(TR_EDITOR_EDIT_REQUEST_NO_AMOUNT);
    }
    if (data.request.deadline_years == 0) {
        data.errors[num_errors++] = translation_for(TR_EDITOR_EDIT_REQUEST_SET_DEADLINE);
    }

    return num_errors;
}

static void button_save(const generic_button *button)
{
    unsigned int num_errors = validate();
    if (num_errors) {
        window_plain_message_dialog_show_text_list(TR_EDITOR_FORM_ERRORS_FOUND, TR_EDITOR_FORM_HAS_FOLLOWING_ERRORS,
            data.errors, num_errors);
        return;
    }
    scenario_request_update(&data.request);
    scenario_editor_set_as_unsaved();
    window_go_back();
}

void window_editor_edit_request_show(int id)
{
    window_type window = {
        WINDOW_EDITOR_EDIT_REQUEST,
        draw_background,
        draw_foreground,
        handle_input
    };
    init(id);
    window_show(&window);
}
