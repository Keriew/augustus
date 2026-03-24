#ifdef ENABLE_MULTIPLAYER

#include "multiplayer_menu.h"

#include "core/config.h"
#include "core/string.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "multiplayer/mp_debug_log.h"
#include "network/session.h"
#include "translation/translation.h"
#include "widget/input_box.h"
#include "multiplayer/bootstrap.h"
#include "window/multiplayer_connect.h"
#include "window/multiplayer_host_setup.h"
#include "window/multiplayer_lobby.h"
#include "window/multiplayer_resume_lobby.h"
#include "window/plain_message_dialog.h"
#include "window/file_dialog.h"

#include <string.h>

#define PANEL_X 96
#define PANEL_Y 80
#define PANEL_WIDTH_BLOCKS 26
#define PANEL_HEIGHT_BLOCKS 16

#define NAME_INPUT_X (PANEL_X + 120)
#define NAME_INPUT_Y (PANEL_Y + 52)
#define NAME_INPUT_WIDTH_BLOCKS 12

#define BUTTON_X (PANEL_X + 80)
#define BUTTON_WIDTH 224
#define BUTTON_HEIGHT 25
#define BUTTON_Y_HOST (PANEL_Y + 100)
#define BUTTON_Y_JOIN (PANEL_Y + 130)
#define BUTTON_Y_RESUME (PANEL_Y + 160)
#define BUTTON_Y_BACK (PANEL_Y + 190)

#define MAX_BUTTONS 4
#define MAX_NICKNAME_LENGTH 31  /* NET_MAX_PLAYER_NAME - 1 */

static void button_host(const generic_button *button);
static void button_join(const generic_button *button);
static void button_resume_saved(const generic_button *button);
static void button_back(const generic_button *button);
static void on_name_changed(int is_addition_at_end);

static generic_button buttons[] = {
    {BUTTON_X, BUTTON_Y_HOST, BUTTON_WIDTH, BUTTON_HEIGHT, button_host, 0, 0},
    {BUTTON_X, BUTTON_Y_JOIN, BUTTON_WIDTH, BUTTON_HEIGHT, button_join, 0, 0},
    {BUTTON_X, BUTTON_Y_RESUME, BUTTON_WIDTH, BUTTON_HEIGHT, button_resume_saved, 0, 0},
    {BUTTON_X, BUTTON_Y_BACK, BUTTON_WIDTH, BUTTON_HEIGHT, button_back, 0, 0},
};

static struct {
    unsigned int focus_button_id;
    int host_attempted;
    uint8_t nickname_text[64];
    int name_empty_warning;
} data;

static input_box nickname_input = {
    .x = NAME_INPUT_X,
    .y = NAME_INPUT_Y,
    .width_blocks = NAME_INPUT_WIDTH_BLOCKS,
    .height_blocks = 2,
    .font = FONT_NORMAL_WHITE,
    .text = data.nickname_text,
    .text_length = MAX_NICKNAME_LENGTH,
    .on_change = on_name_changed,
    .allowed_chars = INPUT_BOX_CHARS_ALPHANUMERIC
};

static void on_name_changed(int is_addition_at_end)
{
    data.name_empty_warning = 0;
    window_invalidate();
}

static int validate_and_apply_name(void)
{
    /* Convert uint8_t text to ASCII */
    char ascii_name[64];
    int len = string_length(data.nickname_text);
    for (int i = 0; i < len && i < 63; i++) {
        ascii_name[i] = (char)data.nickname_text[i];
    }
    ascii_name[len < 63 ? len : 63] = '\0';

    /* Trim leading spaces */
    const char *start = ascii_name;
    while (*start == ' ') {
        start++;
    }
    if (!*start) {
        data.name_empty_warning = 1;
        return 0;
    }

    /* Apply the name (net_session_set_local_name handles trim + persist) */
    net_session_set_local_name(start);
    data.name_empty_warning = 0;
    return 1;
}

static void button_host(const generic_button *button)
{
    if (data.host_attempted) {
        return;
    }

    if (!validate_and_apply_name()) {
        return;
    }

    data.host_attempted = 1;
    MP_LOG_INFO("UI", "Host LAN Game: name='%s'", net_session_get_local_name());

    /* Initialize networking if not already done */
    if (!net_session_is_active()) {
        net_session_init();
    }

    input_box_stop(&nickname_input);

    if (net_session_host(net_session_get_local_name(), NET_DEFAULT_PORT)) {
        MP_LOG_INFO("UI", "Host created successfully — transitioning to host setup");
        window_multiplayer_host_setup_show();
    } else {
        MP_LOG_ERROR("UI", "Host creation FAILED on port %d", NET_DEFAULT_PORT);
        data.host_attempted = 0;
        input_box_start(&nickname_input);
        window_plain_message_dialog_show(
            TR_MP_MENU_HOST_FAILED, TR_MP_MENU_HOST_FAILED, 0);
    }
}

static void button_join(const generic_button *button)
{
    if (!validate_and_apply_name()) {
        return;
    }

    MP_LOG_INFO("UI", "Join LAN Game: name='%s'", net_session_get_local_name());

    /* Initialize networking if not already done */
    if (!net_session_is_active()) {
        net_session_init();
    }

    input_box_stop(&nickname_input);
    window_multiplayer_connect_show();
}

static void on_resume_file_selected(const char *filename)
{
    mp_bootstrap_set_save(filename);
    if (!mp_bootstrap_host_resume_game()) {
        net_session_disconnect();
        net_session_clear_join_status();
        window_plain_message_dialog_show(
            TR_MP_MENU_HOST_FAILED, TR_MP_MENU_HOST_FAILED, 0);
    }
}

static void button_resume_saved(const generic_button *button)
{
    if (!validate_and_apply_name()) {
        return;
    }

    MP_LOG_INFO("UI", "Resume Saved Game: name='%s'", net_session_get_local_name());

    if (!net_session_is_active()) {
        net_session_init();
    }

    input_box_stop(&nickname_input);

    if (net_session_host(net_session_get_local_name(), NET_DEFAULT_PORT)) {
        window_file_dialog_show_with_callback(
            FILE_TYPE_SAVED_GAME, FILE_DIALOG_LOAD, on_resume_file_selected);
    } else {
        MP_LOG_ERROR("UI", "Host creation FAILED for resume on port %d", NET_DEFAULT_PORT);
        input_box_start(&nickname_input);
        window_plain_message_dialog_show(
            TR_MP_MENU_HOST_FAILED, TR_MP_MENU_HOST_FAILED, 0);
    }
}

static void button_back(const generic_button *button)
{
    /* Save the nickname before leaving (in case user typed but didn't host/join) */
    validate_and_apply_name();
    config_save();
    input_box_stop(&nickname_input);
    window_go_back();
}

static void draw_background(void)
{
    graphics_in_dialog();
    outer_panel_draw(PANEL_X, PANEL_Y, PANEL_WIDTH_BLOCKS, PANEL_HEIGHT_BLOCKS);

    /* Title */
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_MENU_TITLE,
        PANEL_X, PANEL_Y + 16, PANEL_WIDTH_BLOCKS * 16, FONT_LARGE_BLACK);

    /* Nickname label */
    uint8_t label_buf[32];
    string_copy(string_from_ascii("Nickname:"), label_buf, 32);
    text_draw(label_buf, PANEL_X + 24, NAME_INPUT_Y + 6, FONT_NORMAL_BLACK, 0);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    /* Nickname input box */
    input_box_draw(&nickname_input);

    /* Empty name warning */
    if (data.name_empty_warning) {
        uint8_t warn_buf[48];
        string_copy(string_from_ascii("Enter a nickname"), warn_buf, 48);
        text_draw_centered(warn_buf, PANEL_X, NAME_INPUT_Y + 34,
            PANEL_WIDTH_BLOCKS * 16, FONT_NORMAL_RED, 0);
    }

    for (int i = 0; i < MAX_BUTTONS; i++) {
        large_label_draw(buttons[i].x, buttons[i].y, BUTTON_WIDTH / 16,
            data.focus_button_id == (unsigned int)(i + 1) ? 1 : 0);
    }

    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_MENU_HOST_LAN,
        BUTTON_X, BUTTON_Y_HOST + 6, BUTTON_WIDTH, FONT_NORMAL_GREEN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_MENU_JOIN_LAN,
        BUTTON_X, BUTTON_Y_JOIN + 6, BUTTON_WIDTH, FONT_NORMAL_GREEN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_MENU_RESUME_SAVED,
        BUTTON_X, BUTTON_Y_RESUME + 6, BUTTON_WIDTH, FONT_NORMAL_GREEN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_MENU_BACK,
        BUTTON_X, BUTTON_Y_BACK + 6, BUTTON_WIDTH, FONT_NORMAL_GREEN);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    if (input_box_handle_mouse(m_dialog, &nickname_input)) {
        return;
    }

    if (generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, MAX_BUTTONS, &data.focus_button_id)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        button_back(0);
    }
}

void window_multiplayer_menu_show(void)
{
    memset(&data, 0, sizeof(data));

    /* Load saved nickname into the text input */
    const char *saved_name = net_session_get_local_name();
    if (saved_name && saved_name[0]) {
        string_copy(string_from_ascii(saved_name), data.nickname_text, 64);
    }

    window_type window = {
        WINDOW_MULTIPLAYER_MENU,
        draw_background,
        draw_foreground,
        handle_input,
        0,
        0
    };
    window_show(&window);
    input_box_start(&nickname_input);
}

#endif /* ENABLE_MULTIPLAYER */
