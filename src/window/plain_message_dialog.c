#include "plain_message_dialog.h"

#include "core/image_group.h"
#include "core/string.h"
#include "graphics/graphics.h"
#include "graphics/image_button.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "window/message_dialog_layout.h"

#define MINIMUM_DIALOG_WIDTH 480
#define MINIMUM_DIALOG_HEIGHT 208
#define MESSAGE_FOOTER_HEIGHT 49
#define EXTRA_TEXT_TOP_MARGIN 10
#define EXTRA_TEXT_SPACING 30

static void button_ok(int param1, int param2);

static image_button buttons[] = {
    {223, 0, 39, 26, IB_NORMAL, GROUP_OK_CANCEL_SCROLL_BUTTONS, 0, button_ok, button_none, 1, 0, 1},
};

static const uint8_t EMPTY_STRING[] = { 0 };
static const uint8_t *original_extra_messages[3];

static struct {
    const uint8_t *title;
    const uint8_t *message;
    int draw_underlying_window;
    message_dialog_layout layout;
    struct {
        const uint8_t **texts;
        unsigned int total_texts;
        int is_list;
    } extra;
} data;

static int init(translation_key title, translation_key message, int should_draw_underlying_window,
    const uint8_t **texts, unsigned int num_texts, int texts_as_list)
{
    if (window_is(WINDOW_PLAIN_MESSAGE_DIALOG)) {
        // don't show popup over popup
        return 0;
    }
    if (!texts) {
        num_texts = 0;
    }
    data.title = translation_for(title);
    data.message = translation_for(message);
    data.draw_underlying_window = should_draw_underlying_window;
    data.extra.texts = texts;
    data.extra.is_list = texts_as_list;
    data.extra.total_texts = num_texts;
    int preferred_width = MINIMUM_DIALOG_WIDTH;
    for (unsigned int i = 0; i < num_texts; i++) {
        const uint8_t *extra_text = texts[i] ? texts[i] : EMPTY_STRING;
        int extra_width = text_get_width(extra_text, FONT_NORMAL_BLACK) + 40;
        if (extra_width > preferred_width) {
            preferred_width = extra_width;
        }
    }
    int extra_height = num_texts ? EXTRA_TEXT_TOP_MARGIN + (num_texts - 1) * EXTRA_TEXT_SPACING +
        font_definition_for(FONT_NORMAL_BLACK)->line_height : 0;
    message_dialog_layout_calculate(&data.layout, data.title, data.message, preferred_width,
        MINIMUM_DIALOG_WIDTH, MINIMUM_DIALOG_HEIGHT, MESSAGE_FOOTER_HEIGHT + extra_height);
    buttons[0].x_offset = (data.layout.width - buttons[0].width) / 2;
    buttons[0].y_offset = data.layout.height - buttons[0].height - 13;
    return 1;
}

static void draw_background(void)
{
    if (data.draw_underlying_window) {
        window_draw_underlying_window();
    }

    graphics_in_dialog_with_size(data.layout.width, data.layout.height);
    outer_panel_draw(0, 0, data.layout.width / BLOCK_SIZE, data.layout.height / BLOCK_SIZE);
    text_draw_multiline(data.title, data.layout.title_x, data.layout.title_y,
        data.layout.title_width, 1, FONT_LARGE_BLACK, 0);
    text_draw_multiline(data.message, data.layout.message_x, data.layout.message_y,
        data.layout.message_width, 0, FONT_NORMAL_BLACK, 0);
    int y_offset = data.layout.message_y + data.layout.message_height + EXTRA_TEXT_TOP_MARGIN;
    if (data.extra.total_texts) {
        for (unsigned int i = 0; i < data.extra.total_texts; i++) {
            const uint8_t *extra_text = data.extra.texts[i] ? data.extra.texts[i] : EMPTY_STRING;
            if (!data.extra.is_list) {
                text_draw_centered(extra_text, data.layout.message_x, y_offset,
                    data.layout.message_width, FONT_NORMAL_BLACK, 0);
            } else {
                int width = text_draw(string_from_ascii("-"), data.layout.message_x, y_offset,
                    FONT_NORMAL_BLACK, 0);
                text_draw(extra_text, data.layout.message_x + width, y_offset,
                    FONT_NORMAL_BLACK, 0);
            }
            y_offset += EXTRA_TEXT_SPACING;
        }
    }
    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog_with_size(data.layout.width, data.layout.height);
    image_buttons_draw(0, 0, buttons, 1);
    graphics_reset_dialog();
}

static void close(void)
{
    window_go_back();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog_with_size(m, data.layout.width, data.layout.height);
    if (image_buttons_handle_mouse(m_dialog, 0, 0, buttons, 1, 0)) {
        return;
    }
    if (input_go_back_requested(m, h) || h->enter_pressed) {
        close();
    }
}

static void button_ok(int param1, int param2)
{
    close();
}

void window_plain_message_dialog_show(translation_key title, translation_key message, int should_draw_underlying_window)
{
    if (init(title, message, should_draw_underlying_window, 0, 0, 0)) {
        window_type window = {
            WINDOW_PLAIN_MESSAGE_DIALOG,
            draw_background,
            draw_foreground,
            handle_input
        };
        window_show(&window);
    }
}

void window_plain_message_dialog_show_with_extra(translation_key title, translation_key message,
    const uint8_t *extra, const uint8_t *extra2)
{
    int num_texts = 0;
    if (extra) {
        original_extra_messages[0] = extra;
        num_texts++;
    }
    if (extra2) {
        if (!num_texts) {
            original_extra_messages[0] = EMPTY_STRING;
        }
        original_extra_messages[1] = extra2;
        num_texts++;
    }
    if (init(title, message, 1, original_extra_messages, num_texts, 0)) {
        window_type window = {
            WINDOW_PLAIN_MESSAGE_DIALOG,
            draw_background,
            draw_foreground,
            handle_input
        };
        window_show(&window);
    }
}

void window_plain_message_dialog_show_text_list(translation_key title, translation_key message,
    const uint8_t **texts, unsigned int num_texts)
{
    if (init(title, message, 1, texts, num_texts, 1)) {
        window_type window = {
            WINDOW_PLAIN_MESSAGE_DIALOG,
            draw_background,
            draw_foreground,
            handle_input
        };
        window_show(&window);
    }
}
