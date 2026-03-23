#ifdef ENABLE_MULTIPLAYER

#include "multiplayer_lobby.h"

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
#include "multiplayer/player_registry.h"
#include "network/peer.h"
#include "network/session.h"
#include "translation/translation.h"

#include <string.h>
#include <stdio.h>

#define PANEL_X 64
#define PANEL_Y 32
#define PANEL_WIDTH_BLOCKS 30
#define PANEL_HEIGHT_BLOCKS 24

#define LIST_X (PANEL_X + 16)
#define LIST_Y (PANEL_Y + 64)
#define LIST_WIDTH_BLOCKS 26
#define LIST_HEIGHT_BLOCKS 12
#define LIST_ITEM_HEIGHT 24

#define READY_BUTTON_X (PANEL_X + 16)
#define READY_BUTTON_Y (PANEL_Y + PANEL_HEIGHT_BLOCKS * 16 - 48)
#define START_BUTTON_X (PANEL_X + 192)
#define START_BUTTON_Y READY_BUTTON_Y
#define LEAVE_BUTTON_X (PANEL_X + 368)
#define LEAVE_BUTTON_Y READY_BUTTON_Y
#define BUTTON_WIDTH 160
#define BUTTON_HEIGHT 24

static void button_ready(const generic_button *button);
static void button_start(const generic_button *button);
static void button_leave(const generic_button *button);
static void draw_player_item(const list_box_item *item);
static void select_player(unsigned int index, int is_double_click);

static generic_button buttons[] = {
    {READY_BUTTON_X, READY_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, button_ready, 0, 0},
    {START_BUTTON_X, START_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, button_start, 0, 0},
    {LEAVE_BUTTON_X, LEAVE_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, button_leave, 0, 0},
};

static list_box_type player_list = {
    .x = LIST_X,
    .y = LIST_Y,
    .width_blocks = LIST_WIDTH_BLOCKS,
    .height_blocks = LIST_HEIGHT_BLOCKS,
    .item_height = LIST_ITEM_HEIGHT,
    .draw_inner_panel = 1,
    .extend_to_hidden_scrollbar = 0,
    .decorate_scrollbar = 0,
    .draw_item = draw_player_item,
    .on_select = select_player,
    .handle_tooltip = 0
};

static struct {
    unsigned int focus_button_id;
    int is_ready;
    int player_ids[MP_MAX_PLAYERS];
    int player_count;
} data;

static void refresh_player_list(void)
{
    data.player_count = 0;
    int id = mp_player_registry_get_first_active_id();
    while (id >= 0 && data.player_count < MP_MAX_PLAYERS) {
        data.player_ids[data.player_count++] = id;
        id = mp_player_registry_get_next_active_id(id);
    }
    list_box_update_total_items(&player_list, data.player_count);
}

static const char *status_text_key(mp_player_status status)
{
    switch (status) {
        case MP_PLAYER_READY: return 0;
        case MP_PLAYER_IN_GAME: return 0;
        case MP_PLAYER_DISCONNECTED: return 0;
        default: return 0;
    }
}

static translation_key status_translation(mp_player_status status)
{
    switch (status) {
        case MP_PLAYER_READY: return TR_MP_LOBBY_READY;
        case MP_PLAYER_IN_GAME: return TR_MP_LOBBY_IN_GAME;
        case MP_PLAYER_DISCONNECTED: return TR_MP_LOBBY_DISCONNECTED;
        case MP_PLAYER_LOBBY: return TR_MP_LOBBY_NOT_READY;
        default: return TR_MP_LOBBY_NOT_READY;
    }
}

static font_t status_font(mp_player_status status)
{
    switch (status) {
        case MP_PLAYER_READY: return FONT_NORMAL_GREEN;
        case MP_PLAYER_IN_GAME: return FONT_NORMAL_GREEN;
        case MP_PLAYER_DISCONNECTED: return FONT_NORMAL_RED;
        default: return FONT_NORMAL_PLAIN;
    }
}

static void draw_player_item(const list_box_item *item)
{
    if ((int)item->index >= data.player_count) {
        return;
    }

    int player_id = data.player_ids[item->index];
    mp_player *player = mp_player_registry_get(player_id);
    if (!player || !player->active) {
        return;
    }

    font_t name_font = item->is_focused ? FONT_NORMAL_WHITE : FONT_NORMAL_GREEN;

    /* Player name */
    uint8_t name_buf[64];
    string_copy(string_from_ascii(player->name), name_buf, 64);
    text_draw(name_buf, item->x + 8, item->y + 4, name_font, 0);

    /* Host indicator */
    if (player->is_host) {
        const uint8_t *host_text = translation_for(TR_MP_HOST);
        text_draw(host_text, item->x + 180, item->y + 4, FONT_NORMAL_BROWN, 0);
    }

    /* Status */
    translation_key status_key = status_translation(player->status);
    const uint8_t *status_text_str = translation_for(status_key);
    text_draw(status_text_str, item->x + 240, item->y + 4, status_font(player->status), 0);

    /* Ping (for remote players) */
    if (!player->is_local) {
        const net_peer *peer = 0;
        for (int i = 0; i < NET_MAX_PEERS; i++) {
            const net_peer *p = net_session_get_peer(i);
            if (p && p->active && p->player_id == player->player_id) {
                peer = p;
                break;
            }
        }
        if (peer) {
            char ping_str[16];
            snprintf(ping_str, sizeof(ping_str), "%u ms", peer->rtt_smoothed_ms);
            uint8_t ping_buf[16];
            string_copy(string_from_ascii(ping_str), ping_buf, 16);
            text_draw(ping_buf, item->x + 360, item->y + 4, FONT_NORMAL_PLAIN, 0);
        }
    }

    if (item->is_focused) {
        button_border_draw(item->x, item->y, item->width, item->height, 1);
    }
}

static void select_player(unsigned int index, int is_double_click)
{
    /* Selection only, no action on click */
    window_invalidate();
}

static void button_ready(const generic_button *button)
{
    data.is_ready = !data.is_ready;
    net_session_set_ready(data.is_ready);
    mp_player *local = mp_player_registry_get_local();
    if (local) {
        mp_player_registry_set_status(local->player_id,
            data.is_ready ? MP_PLAYER_READY : MP_PLAYER_LOBBY);
    }
    window_invalidate();
}

static void button_start(const generic_button *button)
{
    if (!net_session_is_host()) {
        return;
    }
    if (!net_session_all_peers_ready()) {
        return;
    }
    net_session_start_game();
}

static void button_leave(const generic_button *button)
{
    net_session_disconnect();
    window_go_back();
}

static void draw_background(void)
{
    graphics_in_dialog();
    outer_panel_draw(PANEL_X, PANEL_Y, PANEL_WIDTH_BLOCKS, PANEL_HEIGHT_BLOCKS);

    /* Title */
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_LOBBY_TITLE,
        PANEL_X, PANEL_Y + 14, PANEL_WIDTH_BLOCKS * 16, FONT_LARGE_BLACK);

    /* Column headers */
    int header_y = LIST_Y - 18;
    lang_text_draw(CUSTOM_TRANSLATION, TR_MP_LOBBY_PLAYERS, LIST_X + 8, header_y, FONT_NORMAL_BLACK);
    lang_text_draw(CUSTOM_TRANSLATION, TR_MP_LOBBY_STATUS, LIST_X + 240, header_y, FONT_NORMAL_BLACK);
    lang_text_draw(CUSTOM_TRANSLATION, TR_MP_LOBBY_PING, LIST_X + 360, header_y, FONT_NORMAL_BLACK);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    /* Refresh player data */
    refresh_player_list();

    /* Player list */
    list_box_draw(&player_list);

    /* Ready button */
    large_label_draw(READY_BUTTON_X, READY_BUTTON_Y,
        BUTTON_WIDTH / 16, data.focus_button_id == 1 ? 1 : 0);
    translation_key ready_key = data.is_ready ? TR_MP_LOBBY_BUTTON_NOT_READY : TR_MP_LOBBY_BUTTON_READY;
    lang_text_draw_centered(CUSTOM_TRANSLATION, ready_key,
        READY_BUTTON_X, READY_BUTTON_Y + 5, BUTTON_WIDTH, FONT_NORMAL_GREEN);

    /* Start button (host only) */
    if (net_session_is_host()) {
        int all_ready = net_session_all_peers_ready();
        large_label_draw(START_BUTTON_X, START_BUTTON_Y,
            BUTTON_WIDTH / 16, data.focus_button_id == 2 ? 1 : 0);
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_LOBBY_BUTTON_START,
            START_BUTTON_X, START_BUTTON_Y + 5, BUTTON_WIDTH,
            all_ready ? FONT_NORMAL_GREEN : FONT_NORMAL_PLAIN);
    } else {
        /* Show waiting message for clients */
        const uint8_t *waiting = translation_for(TR_MP_LOBBY_WAITING_HOST);
        text_draw_centered(waiting, START_BUTTON_X, START_BUTTON_Y + 5,
            BUTTON_WIDTH, FONT_NORMAL_PLAIN, 0);
    }

    /* Leave button */
    large_label_draw(LEAVE_BUTTON_X, LEAVE_BUTTON_Y,
        BUTTON_WIDTH / 16, data.focus_button_id == 3 ? 1 : 0);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_LOBBY_BUTTON_LEAVE,
        LEAVE_BUTTON_X, LEAVE_BUTTON_Y + 5, BUTTON_WIDTH, FONT_NORMAL_GREEN);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    /* Check session state transitions */
    net_session_state state = net_session_get_state();
    if (state == NET_SESSION_HOSTING_GAME || state == NET_SESSION_CLIENT_GAME) {
        /* Game started — the game tick system will handle transitioning to WINDOW_CITY */
        return;
    }
    if (state == NET_SESSION_IDLE) {
        /* Disconnected */
        window_go_back();
        return;
    }

    int num_buttons = net_session_is_host() ? 3 : 2;
    /* For clients, buttons are index 0 (ready) and 2 (leave), skip start */
    if (!net_session_is_host()) {
        /* Remap: button 0 = ready, button 2 = leave */
        generic_button client_buttons[2];
        client_buttons[0] = buttons[0];
        client_buttons[1] = buttons[2];
        if (generic_buttons_handle_mouse(m_dialog, 0, 0, client_buttons, 2, &data.focus_button_id)) {
            return;
        }
    } else {
        if (generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, num_buttons, &data.focus_button_id)) {
            return;
        }
    }

    if (list_box_handle_input(&player_list, m_dialog, 1)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        button_leave(0);
    }

    list_box_request_refresh(&player_list);
}

void window_multiplayer_lobby_show(void)
{
    memset(&data, 0, sizeof(data));

    refresh_player_list();
    list_box_init(&player_list, data.player_count);

    window_type window = {
        WINDOW_MULTIPLAYER_LOBBY,
        draw_background,
        draw_foreground,
        handle_input,
        0,
        0
    };
    window_show(&window);
}

#endif /* ENABLE_MULTIPLAYER */
