#ifdef ENABLE_MULTIPLAYER

#include "multiplayer_chat.h"

#include "core/string.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "input/keyboard.h"
#include "multiplayer/player_registry.h"
#include "network/session.h"
#include "translation/translation.h"

#include <string.h>
#include <stdio.h>

#define PANEL_X 96
#define PANEL_Y 56
#define PANEL_WIDTH_BLOCKS 25
#define PANEL_HEIGHT_BLOCKS 20

#define CHAT_AREA_X (PANEL_X + 16)
#define CHAT_AREA_Y (PANEL_Y + 40)
#define CHAT_LINE_HEIGHT 16
#define CHAT_MAX_VISIBLE_LINES 16

#define INPUT_X (PANEL_X + 16)
#define INPUT_Y (PANEL_Y + PANEL_HEIGHT_BLOCKS * 16 - 56)
#define INPUT_WIDTH ((PANEL_WIDTH_BLOCKS - 4) * 16)

#define SEND_BUTTON_X (PANEL_X + PANEL_WIDTH_BLOCKS * 16 - 80)
#define SEND_BUTTON_Y INPUT_Y
#define SEND_BUTTON_WIDTH 64
#define SEND_BUTTON_HEIGHT 20

#define CLOSE_BUTTON_X (PANEL_X + PANEL_WIDTH_BLOCKS * 16 - 80)
#define CLOSE_BUTTON_Y (PANEL_Y + PANEL_HEIGHT_BLOCKS * 16 - 28)
#define CLOSE_BUTTON_WIDTH 64
#define CLOSE_BUTTON_HEIGHT 20

static void button_send(const generic_button *button);
static void button_close(const generic_button *button);

static generic_button buttons[] = {
    {SEND_BUTTON_X, SEND_BUTTON_Y, SEND_BUTTON_WIDTH, SEND_BUTTON_HEIGHT, button_send, 0, 0},
    {CLOSE_BUTTON_X, CLOSE_BUTTON_Y, CLOSE_BUTTON_WIDTH, CLOSE_BUTTON_HEIGHT, button_close, 0, 0},
};

static struct {
    uint8_t input_text[128];
    unsigned int focus_button_id;
    int scroll_offset;
} data;

static void send_current_message(void)
{
    if (data.input_text[0] == 0) {
        return;
    }

    /* Convert uint8_t text to char for the send API */
    char message[128];
    int i;
    for (i = 0; i < 127 && data.input_text[i]; i++) {
        message[i] = (char)data.input_text[i];
    }
    message[i] = '\0';

    net_session_send_chat(message);
    memset(data.input_text, 0, sizeof(data.input_text));
    keyboard_stop_capture();
    keyboard_start_capture(data.input_text, 126, 1, INPUT_WIDTH, FONT_NORMAL_WHITE, NULL);
}

static void button_send(const generic_button *button)
{
    send_current_message();
}

static void button_close(const generic_button *button)
{
    keyboard_stop_capture();
    net_session_chat_mark_read();
    window_go_back();
}

static void draw_background(void)
{
    window_draw_underlying_window();
    graphics_in_dialog();
    outer_panel_draw(PANEL_X, PANEL_Y, PANEL_WIDTH_BLOCKS, PANEL_HEIGHT_BLOCKS);

    /* Title: "Chat" */
    uint8_t chat_title[8];
    string_copy(string_from_ascii("Chat"), chat_title, 8);
    text_draw_centered(chat_title,
        PANEL_X, PANEL_Y + 14, PANEL_WIDTH_BLOCKS * 16, FONT_LARGE_BLACK, 0);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    /* Draw chat message area with inner panel */
    inner_panel_draw(CHAT_AREA_X, CHAT_AREA_Y,
        (PANEL_WIDTH_BLOCKS - 2), CHAT_MAX_VISIBLE_LINES + 1);

    /* Draw chat messages */
    int total_messages = net_session_chat_get_count();
    int start_index = 0;
    if (total_messages > CHAT_MAX_VISIBLE_LINES) {
        start_index = total_messages - CHAT_MAX_VISIBLE_LINES + data.scroll_offset;
        if (start_index < 0) {
            start_index = 0;
        }
        if (start_index > total_messages - CHAT_MAX_VISIBLE_LINES) {
            start_index = total_messages - CHAT_MAX_VISIBLE_LINES;
        }
    }

    int y = CHAT_AREA_Y + 4;
    for (int i = start_index; i < total_messages && (i - start_index) < CHAT_MAX_VISIBLE_LINES; i++) {
        uint8_t sender_id;
        const char *msg = net_session_chat_get_message(i, &sender_id);
        if (!msg) {
            continue;
        }

        /* Format: "[PlayerName] message" */
        char line[192];
        mp_player *sender = mp_player_registry_get(sender_id);
        if (sender && sender->active) {
            snprintf(line, sizeof(line), "[%s] %s", sender->name, msg);
        } else {
            snprintf(line, sizeof(line), "[Player %d] %s", sender_id, msg);
        }

        uint8_t line_buf[192];
        string_copy(string_from_ascii(line), line_buf, 192);

        /* Color based on sender */
        font_t font = FONT_NORMAL_WHITE;
        if (sender_id == net_session_get_local_player_id()) {
            font = FONT_NORMAL_GREEN;
        }
        text_draw(line_buf, CHAT_AREA_X + 8, y, font, 0);
        y += CHAT_LINE_HEIGHT;
    }

    /* Draw input area */
    inner_panel_draw(INPUT_X, INPUT_Y, (PANEL_WIDTH_BLOCKS - 6), 1);

    /* Draw input text */
    text_draw(data.input_text, INPUT_X + 4, INPUT_Y + 3, FONT_NORMAL_WHITE, 0);

    /* Send button */
    large_label_draw(SEND_BUTTON_X, SEND_BUTTON_Y,
        SEND_BUTTON_WIDTH / 16, data.focus_button_id == 1 ? 1 : 0);
    {
        uint8_t send_label[8];
        string_copy(string_from_ascii("Send"), send_label, 8);
        text_draw_centered(send_label, SEND_BUTTON_X, SEND_BUTTON_Y + 3,
            SEND_BUTTON_WIDTH, FONT_NORMAL_GREEN, 0);
    }

    /* Close button */
    large_label_draw(CLOSE_BUTTON_X, CLOSE_BUTTON_Y,
        CLOSE_BUTTON_WIDTH / 16, data.focus_button_id == 2 ? 1 : 0);
    {
        uint8_t close_label[8];
        string_copy(string_from_ascii("Close"), close_label, 8);
        text_draw_centered(close_label, CLOSE_BUTTON_X, CLOSE_BUTTON_Y + 3,
            CLOSE_BUTTON_WIDTH, FONT_NORMAL_GREEN, 0);
    }

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    if (!net_session_is_active()) {
        keyboard_stop_capture();
        window_go_back();
        return;
    }

    if (generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, 2, &data.focus_button_id)) {
        return;
    }

    /* Enter key sends message */
    if (keyboard_input_is_accepted()) {
        send_current_message();
        return;
    }

    /* Escape closes */
    if (input_go_back_requested(m, h)) {
        keyboard_stop_capture();
        net_session_chat_mark_read();
        window_go_back();
        return;
    }

    /* Scroll with mouse wheel */
    int max_scroll_back = net_session_chat_get_count() - CHAT_MAX_VISIBLE_LINES;
    if (max_scroll_back < 0) {
        max_scroll_back = 0;
    }
    if (m->scrolled == SCROLL_UP && data.scroll_offset > -max_scroll_back) {
        data.scroll_offset--;
    }
    if (m->scrolled == SCROLL_DOWN && data.scroll_offset < 0) {
        data.scroll_offset++;
    }
}

void window_multiplayer_chat_show(void)
{
    memset(&data, 0, sizeof(data));

    /* Start keyboard capture for chat input */
    keyboard_start_capture(data.input_text, 126, 1, INPUT_WIDTH, FONT_NORMAL_WHITE, NULL);

    /* Mark existing messages as read */
    net_session_chat_mark_read();

    window_type window = {
        WINDOW_MULTIPLAYER_CHAT,
        draw_background,
        draw_foreground,
        handle_input,
        0,
        0
    };
    window_show(&window);
}

#endif /* ENABLE_MULTIPLAYER */
