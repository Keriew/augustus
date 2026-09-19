#include "popup_dialog.h"

#include "core/image_group.h"
#include "core/lang.h"
#include "core/string.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "translation/translation.h"
#include "window/message_dialog_layout.h"

#define GROUP 5

#define PROCEED_GROUP 43
#define PROCEED_TEXT 5
#define CHECKBOX_CHECK_SIZE 20
#define CHECKBOX_TEXT_OFFSET 30
#define CHECKBOX_HORIZONTAL_MARGIN 20
#define MINIMUM_DIALOG_WIDTH 480
#define MINIMUM_DIALOG_HEIGHT 160
#define CHECKBOX_DIALOG_HEIGHT 176
#define ACTION_BUTTON_FOOTER_HEIGHT 50
#define CHECKBOX_FOOTER_HEIGHT 80

static void button_checkbox(const generic_button *button);
static void button_ok(int param1, int param2);
static void button_cancel(int param1, int param2);
static void confirm(void);

static image_button buttons[] = {
    {192, 100, 39, 26, IB_NORMAL, GROUP_OK_CANCEL_SCROLL_BUTTONS, 0, button_ok, button_none, 1, 0, 1},
    {256, 100, 39, 26, IB_NORMAL, GROUP_OK_CANCEL_SCROLL_BUTTONS, 4, button_cancel, button_none, 0, 0, 1},
};

static generic_button checkbox = { 160, 180, 360, 20, button_checkbox };

static struct {
    int ok_clicked;
    void (*close_func)(int accepted, int checked);
    int has_buttons;
    int translation_key;
    int checked;
    unsigned int has_focus;
    int checkbox_start_width;
    message_dialog_layout layout;
    const uint8_t *custom_title;
    const uint8_t *custom_text;
    const uint8_t *checkbox_text;
} data;

static int init(const uint8_t *custom_title, const uint8_t *custom_text,
    const uint8_t *checkbox_text, void (*close_func)(int accepted, int checked), int has_ok_cancel_buttons)
{
    if (window_is(WINDOW_POPUP_DIALOG)) {
        // don't show popup over popup
        return 0;
    }
    data.ok_clicked = 0;
    data.close_func = close_func;
    data.has_buttons = has_ok_cancel_buttons;
    data.custom_title = custom_title;
    data.custom_text = custom_text;
    data.checkbox_text = checkbox_text;
    data.checked = 0;
    if (!data.custom_text) {
        data.custom_text = lang_get_string(PROCEED_GROUP, PROCEED_TEXT);
    }
    if (data.checkbox_text) {
        int checkbox_width = text_get_width(data.checkbox_text, FONT_NORMAL_BLACK) + 70;
        message_dialog_layout_calculate(&data.layout, data.custom_title, data.custom_text, checkbox_width,
            MINIMUM_DIALOG_WIDTH, CHECKBOX_DIALOG_HEIGHT, CHECKBOX_FOOTER_HEIGHT);
    } else {
        message_dialog_layout_calculate(&data.layout, data.custom_title, data.custom_text, MINIMUM_DIALOG_WIDTH,
            MINIMUM_DIALOG_WIDTH, MINIMUM_DIALOG_HEIGHT, ACTION_BUTTON_FOOTER_HEIGHT);
    }
    buttons[0].x_offset = (data.layout.width - 103) / 2;
    buttons[1].x_offset = buttons[0].x_offset + 64;
    buttons[0].y_offset = buttons[1].y_offset = data.layout.height - ACTION_BUTTON_FOOTER_HEIGHT;
    if (data.checkbox_text) {
        int checkbox_content_width = text_get_width(data.checkbox_text, FONT_NORMAL_BLACK) + CHECKBOX_TEXT_OFFSET;
        int maximum_checkbox_width = data.layout.width - 2 * CHECKBOX_HORIZONTAL_MARGIN;
        if (checkbox_content_width > maximum_checkbox_width) {
            checkbox_content_width = maximum_checkbox_width;
        }
        data.checkbox_start_width = (data.layout.width - checkbox_content_width) / 2;
        checkbox.x = data.checkbox_start_width;
        checkbox.y = buttons[0].y_offset - 30;
        checkbox.width = checkbox_content_width;
    }
    return 1;
}

static void draw_background(void)
{
    window_draw_underlying_window();
    graphics_in_dialog_with_size(data.layout.width, data.layout.height);
    outer_panel_draw(0, 0, data.layout.width / BLOCK_SIZE, data.layout.height / BLOCK_SIZE);
    if (data.custom_title) {
        text_draw_multiline(data.custom_title, data.layout.title_x, data.layout.title_y,
            data.layout.title_width, 1, FONT_LARGE_BLACK, 0);
    }
    text_draw_multiline(data.custom_text, data.layout.message_x, data.layout.message_y,
        data.layout.message_width, data.layout.message_lines == 1, FONT_NORMAL_BLACK, 0);
    if (data.checkbox_text) {
        if (data.checked) {
            text_draw(string_from_ascii("x"), checkbox.x + 6, checkbox.y + 3, FONT_NORMAL_BLACK, 0);
        }
        text_draw_ellipsized(data.checkbox_text, checkbox.x + CHECKBOX_TEXT_OFFSET, checkbox.y + 4,
            checkbox.width - CHECKBOX_TEXT_OFFSET, FONT_NORMAL_BLACK, 0);
    }
    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog_with_size(data.layout.width, data.layout.height);
    if (data.checkbox_text) {
        button_border_draw(checkbox.x, checkbox.y, CHECKBOX_CHECK_SIZE, CHECKBOX_CHECK_SIZE, data.has_focus);
    }
    if (data.has_buttons) {
        image_buttons_draw(0, 0, buttons, 2);
    } else {
        lang_text_draw_centered(13, 1, 0, data.layout.height - 32, data.layout.width, FONT_NORMAL_BLACK);
    }
    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog_with_size(m, data.layout.width, data.layout.height);
    if (data.checkbox_text && generic_buttons_handle_mouse(m_dialog, 0, 0, &checkbox, 1, &data.has_focus)) {
        return;
    }
    if (data.has_buttons && image_buttons_handle_mouse(m_dialog, 0, 0, buttons, 2, 0)) {
        return;
    }
    if (input_go_back_requested(m, h)) {
        window_go_back();
        data.close_func(0, 0);
    }
    if (h->enter_pressed) {
        confirm();
    }
}

static void button_ok(int param1, int param2)
{
    confirm();
}

static void button_cancel(int param1, int param2)
{
    window_go_back();
    data.close_func(0, 0);
}

static void button_checkbox(const generic_button *button)
{
    data.checked ^= 1;
    window_request_refresh();
}

static void confirm(void)
{
    window_go_back();
    data.close_func(1, data.checked);
}

void window_popup_dialog_show(popup_dialog_type type,
    void (*close_func)(int accepted, int checked), int has_ok_cancel_buttons)
{
    if (init(lang_get_string(GROUP, type), lang_get_string(GROUP, type + 1), 0, close_func, has_ok_cancel_buttons)) {
        window_type window = {
            WINDOW_POPUP_DIALOG,
            draw_background,
            draw_foreground,
            handle_input
        };
        window_show(&window);
    }
}

void window_popup_dialog_show_confirmation(const uint8_t *custom_title, const uint8_t *custom_text,
    const uint8_t *checkbox_text, void (*close_func)(int accepted, int checked))
{
    if (init(custom_title, custom_text, checkbox_text, close_func, 1)) {
        window_type window = {
            WINDOW_POPUP_DIALOG,
            draw_background,
            draw_foreground,
            handle_input
        };
        window_show(&window);
    }
}
