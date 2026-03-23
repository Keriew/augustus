#ifdef ENABLE_MULTIPLAYER

#include "multiplayer_menu.h"

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
#include "window/multiplayer_connect.h"
#include "window/multiplayer_lobby.h"
#include "window/plain_message_dialog.h"

#include <string.h>

#define PANEL_X 128
#define PANEL_Y 100
#define PANEL_WIDTH_BLOCKS 24
#define PANEL_HEIGHT_BLOCKS 12

#define BUTTON_X (PANEL_X + 80)
#define BUTTON_WIDTH 224
#define BUTTON_HEIGHT 25
#define BUTTON_Y_HOST (PANEL_Y + 56)
#define BUTTON_Y_JOIN (PANEL_Y + 96)
#define BUTTON_Y_BACK (PANEL_Y + 136)

#define MAX_BUTTONS 3

static void button_host(const generic_button *button);
static void button_join(const generic_button *button);
static void button_back(const generic_button *button);

static generic_button buttons[] = {
    {BUTTON_X, BUTTON_Y_HOST, BUTTON_WIDTH, BUTTON_HEIGHT, button_host, 0, 0},
    {BUTTON_X, BUTTON_Y_JOIN, BUTTON_WIDTH, BUTTON_HEIGHT, button_join, 0, 0},
    {BUTTON_X, BUTTON_Y_BACK, BUTTON_WIDTH, BUTTON_HEIGHT, button_back, 0, 0},
};

static struct {
    unsigned int focus_button_id;
    int host_attempted;
} data;

static void button_host(const generic_button *button)
{
    if (data.host_attempted) {
        return;
    }
    data.host_attempted = 1;

    MP_LOG_INFO("UI", "Host LAN Game button pressed");

    /* Initialize networking if not already done */
    if (!net_session_is_active()) {
        MP_LOG_INFO("UI", "Initializing network session (first time)");
        net_session_init();
    }

    if (net_session_host("Player", NET_DEFAULT_PORT)) {
        MP_LOG_INFO("UI", "Host created successfully — transitioning to lobby");
        window_multiplayer_lobby_show();
    } else {
        MP_LOG_ERROR("UI", "Host creation FAILED on port %d — showing error dialog", NET_DEFAULT_PORT);
        data.host_attempted = 0;
        window_plain_message_dialog_show(
            TR_MP_MENU_HOST_FAILED, TR_MP_MENU_HOST_FAILED, 0);
    }
}

static void button_join(const generic_button *button)
{
    MP_LOG_INFO("UI", "Join LAN Game button pressed");

    /* Initialize networking if not already done */
    if (!net_session_is_active()) {
        MP_LOG_INFO("UI", "Initializing network session (first time)");
        net_session_init();
    }
    MP_LOG_INFO("UI", "Transitioning to connect window");
    window_multiplayer_connect_show();
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
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_MENU_TITLE,
        PANEL_X, PANEL_Y + 16, PANEL_WIDTH_BLOCKS * 16, FONT_LARGE_BLACK);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    for (int i = 0; i < MAX_BUTTONS; i++) {
        large_label_draw(buttons[i].x, buttons[i].y, BUTTON_WIDTH / 16,
            data.focus_button_id == (unsigned int)(i + 1) ? 1 : 0);
    }

    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_MENU_HOST_LAN,
        BUTTON_X, BUTTON_Y_HOST + 6, BUTTON_WIDTH, FONT_NORMAL_GREEN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_MENU_JOIN_LAN,
        BUTTON_X, BUTTON_Y_JOIN + 6, BUTTON_WIDTH, FONT_NORMAL_GREEN);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_MENU_BACK,
        BUTTON_X, BUTTON_Y_BACK + 6, BUTTON_WIDTH, FONT_NORMAL_GREEN);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
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

    window_type window = {
        WINDOW_MULTIPLAYER_MENU,
        draw_background,
        draw_foreground,
        handle_input,
        0,
        0
    };
    window_show(&window);
}

#endif /* ENABLE_MULTIPLAYER */
