#ifdef ENABLE_MULTIPLAYER

#include "multiplayer_connect.h"

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
#include "multiplayer/mp_debug_log.h"
#include "network/discovery_lan.h"
#include "network/session.h"
#include "translation/translation.h"
#include "widget/input_box.h"
#include "window/multiplayer_lobby.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define PANEL_X 48
#define PANEL_Y 40
#define PANEL_WIDTH_BLOCKS 32
#define PANEL_HEIGHT_BLOCKS 22

#define INPUT_X (PANEL_X + 16)
#define INPUT_Y (PANEL_Y + 56)
#define INPUT_WIDTH_BLOCKS 20

#define LIST_X (PANEL_X + 16)
#define LIST_Y (PANEL_Y + 128)
#define LIST_WIDTH_BLOCKS 28
#define LIST_HEIGHT_BLOCKS 10
#define LIST_ITEM_HEIGHT 20

#define CONNECT_BUTTON_X (PANEL_X + 344)
#define CONNECT_BUTTON_Y (PANEL_Y + 52)
#define BACK_BUTTON_X (PANEL_X + 16)
#define BACK_BUTTON_Y (PANEL_Y + PANEL_HEIGHT_BLOCKS * 16 - 48)
#define BUTTON_WIDTH 160
#define BUTTON_HEIGHT 24

static void button_connect(const generic_button *button);
static void button_back(const generic_button *button);

static void draw_server_item(const list_box_item *item);
static void select_server(unsigned int index, int is_double_click);
static void on_input_changed(int is_addition_at_end);

static generic_button buttons[] = {
    {CONNECT_BUTTON_X, CONNECT_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, button_connect, 0, 0},
    {BACK_BUTTON_X, BACK_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, button_back, 0, 0},
};

static list_box_type server_list = {
    .x = LIST_X,
    .y = LIST_Y,
    .width_blocks = LIST_WIDTH_BLOCKS,
    .height_blocks = LIST_HEIGHT_BLOCKS,
    .item_height = LIST_ITEM_HEIGHT,
    .draw_inner_panel = 1,
    .extend_to_hidden_scrollbar = 0,
    .decorate_scrollbar = 1,
    .draw_item = draw_server_item,
    .on_select = select_server,
    .handle_tooltip = 0
};

static struct {
    uint8_t address_text[64];
    unsigned int focus_button_id;
    int is_connecting;
    uint32_t connecting_start_ms;
} data;

static input_box address_input = {
    .x = INPUT_X,
    .y = INPUT_Y,
    .width_blocks = INPUT_WIDTH_BLOCKS,
    .height_blocks = 2,
    .font = FONT_NORMAL_WHITE,
    .text = data.address_text,
    .text_length = 63,
    .on_change = on_input_changed,
    .allowed_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.:"
};

static void on_input_changed(int is_addition_at_end)
{
    window_invalidate();
}

static void try_connect(const char *address)
{
    if (!address || !address[0]) {
        return;
    }

    /* Parse address:port */
    char host[48];
    uint16_t port = NET_DEFAULT_PORT;
    const char *colon = strchr(address, ':');
    if (colon) {
        size_t host_len = (size_t)(colon - address);
        if (host_len >= sizeof(host)) {
            host_len = sizeof(host) - 1;
        }
        memcpy(host, address, host_len);
        host[host_len] = '\0';
        int parsed_port = atoi(colon + 1);
        if (parsed_port > 0 && parsed_port <= 65535) {
            port = (uint16_t)parsed_port;
        }
    } else {
        snprintf(host, sizeof(host), "%s", address);
    }

    MP_LOG_INFO("UI", "Attempting connection to %s:%d", host, (int)port);
    if (net_session_join("Player", host, port)) {
        data.is_connecting = 1;
        MP_LOG_INFO("UI", "Connection initiated — waiting for handshake");
        window_invalidate();
    } else {
        MP_LOG_ERROR("UI", "net_session_join failed immediately for %s:%d", host, (int)port);
    }
}

static void button_connect(const generic_button *button)
{
    if (data.is_connecting) {
        return;
    }
    char ascii_address[64];
    int len = string_length(data.address_text);
    for (int i = 0; i < len && i < 63; i++) {
        ascii_address[i] = (char)data.address_text[i];
    }
    ascii_address[len < 63 ? len : 63] = '\0';
    try_connect(ascii_address);
}

static void button_back(const generic_button *button)
{
    input_box_stop(&address_input);
    net_discovery_stop_listening();
    if (data.is_connecting) {
        net_session_disconnect();
        data.is_connecting = 0;
    }
    window_go_back();
}


static void draw_server_item(const list_box_item *item)
{
    int host_count = net_discovery_get_host_count();
    if ((int)item->index >= host_count) {
        return;
    }

    const net_discovered_host *host = net_discovery_get_host(item->index);
    if (!host || !host->active) {
        return;
    }

    font_t font = item->is_focused ? FONT_NORMAL_WHITE : FONT_NORMAL_GREEN;

    /* Host name */
    uint8_t name_buf[64];
    string_copy(string_from_ascii(host->host_name), name_buf, 64);
    text_draw(name_buf, item->x + 4, item->y + 2, font, 0);

    /* IP address */
    uint8_t ip_buf[64];
    string_copy(string_from_ascii(host->host_ip), ip_buf, 64);
    text_draw(ip_buf, item->x + 180, item->y + 2, FONT_NORMAL_PLAIN, 0);

    /* Player count */
    char count_str[16];
    snprintf(count_str, sizeof(count_str), "%d/%d", host->player_count, host->max_players);
    uint8_t count_buf[16];
    string_copy(string_from_ascii(count_str), count_buf, 16);
    text_draw(count_buf, item->x + 350, item->y + 2, FONT_NORMAL_PLAIN, 0);

    if (item->is_focused) {
        button_border_draw(item->x, item->y, item->width, item->height, 1);
    }
}

static void select_server(unsigned int index, int is_double_click)
{
    if (!is_double_click) {
        return;
    }
    int host_count = net_discovery_get_host_count();
    if ((int)index >= host_count) {
        return;
    }
    const net_discovered_host *host = net_discovery_get_host(index);
    if (!host || !host->active) {
        return;
    }
    try_connect(host->host_ip);
}

static void draw_background(void)
{
    graphics_in_dialog();
    outer_panel_draw(PANEL_X, PANEL_Y, PANEL_WIDTH_BLOCKS, PANEL_HEIGHT_BLOCKS);

    /* Title */
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_CONNECT_TITLE,
        PANEL_X, PANEL_Y + 14, PANEL_WIDTH_BLOCKS * 16, FONT_LARGE_BLACK);

    /* Address label */
    lang_text_draw(CUSTOM_TRANSLATION, TR_MP_CONNECT_ADDRESS,
        INPUT_X, INPUT_Y - 18, FONT_NORMAL_BLACK);

    /* LAN servers label */
    lang_text_draw(CUSTOM_TRANSLATION, TR_MP_CONNECT_LAN_SERVERS,
        LIST_X, LIST_Y - 18, FONT_NORMAL_BLACK);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    /* Address input box */
    input_box_draw(&address_input);

    /* Connect button */
    large_label_draw(CONNECT_BUTTON_X, CONNECT_BUTTON_Y,
        BUTTON_WIDTH / 16, data.focus_button_id == 1 ? 1 : 0);
    if (data.is_connecting) {
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_CONNECT_CONNECTING,
            CONNECT_BUTTON_X, CONNECT_BUTTON_Y + 5, BUTTON_WIDTH, FONT_NORMAL_GREEN);
    } else {
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_CONNECT_BUTTON_CONNECT,
            CONNECT_BUTTON_X, CONNECT_BUTTON_Y + 5, BUTTON_WIDTH, FONT_NORMAL_GREEN);
    }

    /* Back button */
    large_label_draw(BACK_BUTTON_X, BACK_BUTTON_Y,
        BUTTON_WIDTH / 16, data.focus_button_id == 2 ? 1 : 0);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_CONNECT_BUTTON_BACK,
        BACK_BUTTON_X, BACK_BUTTON_Y + 5, BUTTON_WIDTH, FONT_NORMAL_GREEN);

    /* Server list */
    int host_count = net_discovery_get_host_count();
    list_box_update_total_items(&server_list, host_count);
    list_box_draw(&server_list);

    /* No servers message */
    if (host_count == 0) {
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_CONNECT_NO_SERVERS,
            LIST_X, LIST_Y + LIST_HEIGHT_BLOCKS * 8, LIST_WIDTH_BLOCKS * 16, FONT_NORMAL_PLAIN);
    }

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    /* Check if connection succeeded */
    if (data.is_connecting) {
        net_session_state state = net_session_get_state();
        if (state == NET_SESSION_CLIENT_LOBBY) {
            MP_LOG_INFO("UI", "Connection succeeded — transitioning to lobby");
            data.is_connecting = 0;
            input_box_stop(&address_input);
            net_discovery_stop_listening();
            window_multiplayer_lobby_show();
            return;
        }
        if (state == NET_SESSION_IDLE) {
            /* Connection failed or rejected */
            MP_LOG_WARN("UI", "Connection failed — session returned to IDLE");
            data.is_connecting = 0;
            window_invalidate();
        }
    }

    if (input_box_is_accepted()) {
        button_connect(0);
        return;
    }

    if (input_box_handle_mouse(m_dialog, &address_input)) {
        return;
    }

    if (list_box_handle_input(&server_list, m_dialog, 1)) {
        return;
    }

    if (generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, 2, &data.focus_button_id)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        button_back(0);
    }

    /* Refresh discovery */
    net_discovery_update();
    list_box_request_refresh(&server_list);
}

void window_multiplayer_connect_show(void)
{
    memset(&data, 0, sizeof(data));
    data.is_connecting = 0;

    net_discovery_start_listening();
    list_box_init(&server_list, 0);

    window_type window = {
        WINDOW_MULTIPLAYER_CONNECT,
        draw_background,
        draw_foreground,
        handle_input,
        0,
        0
    };
    window_show(&window);
    input_box_start(&address_input);
}

#endif /* ENABLE_MULTIPLAYER */
