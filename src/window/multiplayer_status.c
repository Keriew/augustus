#ifdef ENABLE_MULTIPLAYER

#include "multiplayer_status.h"

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
#include "multiplayer/checksum.h"
#include "multiplayer/player_registry.h"
#include "multiplayer/resync.h"
#include "multiplayer/time_sync.h"
#include "network/peer.h"
#include "network/session.h"
#include "translation/translation.h"

#include <string.h>
#include <stdio.h>

#define PANEL_X 80
#define PANEL_Y 48
#define PANEL_WIDTH_BLOCKS 28
#define PANEL_HEIGHT_BLOCKS 22

#define INFO_X (PANEL_X + 24)
#define INFO_Y (PANEL_Y + 48)

#define LIST_X (PANEL_X + 16)
#define LIST_Y (PANEL_Y + 144)
#define LIST_WIDTH_BLOCKS 24
#define LIST_HEIGHT_BLOCKS 8
#define LIST_ITEM_HEIGHT 20

#define RESYNC_BUTTON_X (PANEL_X + 16)
#define RESYNC_BUTTON_Y (PANEL_Y + PANEL_HEIGHT_BLOCKS * 16 - 48)
#define DISCONNECT_BUTTON_X (PANEL_X + 192)
#define DISCONNECT_BUTTON_Y RESYNC_BUTTON_Y
#define CLOSE_BUTTON_X (PANEL_X + 320)
#define CLOSE_BUTTON_Y RESYNC_BUTTON_Y
#define BUTTON_WIDTH 144
#define BUTTON_HEIGHT 24

static void button_resync(const generic_button *button);
static void button_disconnect(const generic_button *button);
static void button_close(const generic_button *button);
static void draw_player_item(const list_box_item *item);
static void select_player(unsigned int index, int is_double_click);

static generic_button buttons[] = {
    {RESYNC_BUTTON_X, RESYNC_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, button_resync, 0, 0},
    {DISCONNECT_BUTTON_X, DISCONNECT_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, button_disconnect, 0, 0},
    {CLOSE_BUTTON_X, CLOSE_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, button_close, 0, 0},
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

static translation_key quality_translation(net_peer_quality quality)
{
    switch (quality) {
        case NET_PEER_QUALITY_GOOD: return TR_MP_STATUS_QUALITY_GOOD;
        case NET_PEER_QUALITY_DEGRADED: return TR_MP_STATUS_QUALITY_DEGRADED;
        case NET_PEER_QUALITY_POOR: return TR_MP_STATUS_QUALITY_POOR;
        case NET_PEER_QUALITY_CRITICAL: return TR_MP_STATUS_QUALITY_CRITICAL;
        default: return TR_MP_STATUS_QUALITY_UNKNOWN;
    }
}

static font_t quality_font(net_peer_quality quality)
{
    switch (quality) {
        case NET_PEER_QUALITY_GOOD: return FONT_NORMAL_GREEN;
        case NET_PEER_QUALITY_DEGRADED: return FONT_NORMAL_BROWN;
        case NET_PEER_QUALITY_POOR: return FONT_NORMAL_RED;
        case NET_PEER_QUALITY_CRITICAL: return FONT_NORMAL_RED;
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
    text_draw(name_buf, item->x + 4, item->y + 2, name_font, 0);

    /* Host badge */
    if (player->is_host) {
        const uint8_t *host_label = translation_for(TR_MP_HOST);
        text_draw(host_label, item->x + 160, item->y + 2, FONT_NORMAL_BROWN, 0);
    }

    /* Ping / quality for remote players */
    if (!player->is_local) {
        const net_peer *peer = 0;
        int peer_count = net_session_get_peer_count();
        for (int i = 0; i < peer_count; i++) {
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
            text_draw(ping_buf, item->x + 220, item->y + 2, FONT_NORMAL_PLAIN, 0);

            translation_key qkey = quality_translation(peer->quality);
            const uint8_t *qtext = translation_for(qkey);
            text_draw(qtext, item->x + 300, item->y + 2, quality_font(peer->quality), 0);
        }
    } else {
        /* Local player — show "Local" */
        char local_str[] = "Local";
        uint8_t local_buf[16];
        string_copy(string_from_ascii(local_str), local_buf, 16);
        text_draw(local_buf, item->x + 220, item->y + 2, FONT_NORMAL_PLAIN, 0);
    }

    if (item->is_focused) {
        button_border_draw(item->x, item->y, item->width, item->height, 1);
    }
}

static void select_player(unsigned int index, int is_double_click)
{
    window_invalidate();
}

static void button_resync(const generic_button *button)
{
    if (net_session_is_client()) {
        mp_resync_request();
    }
}

static void button_disconnect(const generic_button *button)
{
    net_session_disconnect();
    window_go_back();
}

static void button_close(const generic_button *button)
{
    window_go_back();
}

static void draw_background(void)
{
    window_draw_underlying_window();
    graphics_in_dialog();
    outer_panel_draw(PANEL_X, PANEL_Y, PANEL_WIDTH_BLOCKS, PANEL_HEIGHT_BLOCKS);

    /* Title */
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_STATUS_TITLE,
        PANEL_X, PANEL_Y + 14, PANEL_WIDTH_BLOCKS * 16, FONT_LARGE_BLACK);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    int y = INFO_Y;
    int label_x = INFO_X;
    int value_x = INFO_X + 140;

    /* Connection quality */
    lang_text_draw(CUSTOM_TRANSLATION, TR_MP_STATUS_CONNECTION, label_x, y, FONT_NORMAL_BLACK);
    if (net_session_is_host()) {
        /* Host shows own quality as "Host" */
        const uint8_t *host_text = translation_for(TR_MP_HOST);
        text_draw(host_text, value_x, y, FONT_NORMAL_GREEN, 0);
    } else {
        const net_peer *host_peer = net_session_get_host_peer();
        if (host_peer && host_peer->active) {
            translation_key qkey = quality_translation(host_peer->quality);
            const uint8_t *qtext = translation_for(qkey);
            text_draw(qtext, value_x, y, quality_font(host_peer->quality), 0);
        }
    }
    y += 20;

    /* Sync status */
    lang_text_draw(CUSTOM_TRANSLATION, TR_MP_STATUS_SYNC, label_x, y, FONT_NORMAL_BLACK);
    int has_desync = mp_checksum_has_desync();
    translation_key sync_key = has_desync ? TR_MP_STATUS_DESYNCED : TR_MP_STATUS_SYNCED;
    const uint8_t *sync_text = translation_for(sync_key);
    text_draw(sync_text, value_x, y, has_desync ? FONT_NORMAL_RED : FONT_NORMAL_GREEN, 0);
    y += 20;

    /* Game tick */
    lang_text_draw(CUSTOM_TRANSLATION, TR_MP_STATUS_TICK, label_x, y, FONT_NORMAL_BLACK);
    uint32_t auth_tick = net_session_get_authoritative_tick();
    char tick_str[32];
    snprintf(tick_str, sizeof(tick_str), "%u", auth_tick);
    uint8_t tick_buf[32];
    string_copy(string_from_ascii(tick_str), tick_buf, 32);
    text_draw(tick_buf, value_x, y, FONT_NORMAL_PLAIN, 0);
    y += 24;

    /* Players header */
    lang_text_draw(CUSTOM_TRANSLATION, TR_MP_STATUS_PLAYERS_ONLINE, LIST_X, LIST_Y - 18, FONT_NORMAL_BLACK);

    /* Player list */
    refresh_player_list();
    list_box_draw(&player_list);

    /* Resync button (client only) */
    if (net_session_is_client()) {
        large_label_draw(RESYNC_BUTTON_X, RESYNC_BUTTON_Y,
            BUTTON_WIDTH / 16, data.focus_button_id == 1 ? 1 : 0);
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_STATUS_BUTTON_RESYNC,
            RESYNC_BUTTON_X, RESYNC_BUTTON_Y + 5, BUTTON_WIDTH, FONT_NORMAL_GREEN);
    }

    /* Disconnect button */
    large_label_draw(DISCONNECT_BUTTON_X, DISCONNECT_BUTTON_Y,
        BUTTON_WIDTH / 16, data.focus_button_id == 2 ? 1 : 0);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_STATUS_BUTTON_DISCONNECT,
        DISCONNECT_BUTTON_X, DISCONNECT_BUTTON_Y + 5, BUTTON_WIDTH, FONT_NORMAL_GREEN);

    /* Close button */
    large_label_draw(CLOSE_BUTTON_X, CLOSE_BUTTON_Y,
        BUTTON_WIDTH / 16, data.focus_button_id == 3 ? 1 : 0);
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_MP_STATUS_BUTTON_CLOSE,
        CLOSE_BUTTON_X, CLOSE_BUTTON_Y + 5, BUTTON_WIDTH, FONT_NORMAL_GREEN);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    /* Check if we got disconnected */
    if (!net_session_is_active()) {
        window_go_back();
        return;
    }

    if (generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, 3, &data.focus_button_id)) {
        return;
    }

    if (list_box_handle_input(&player_list, m_dialog, 1)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        window_go_back();
    }

    list_box_request_refresh(&player_list);
}

void window_multiplayer_status_show(void)
{
    memset(&data, 0, sizeof(data));

    refresh_player_list();
    list_box_init(&player_list, data.player_count);

    window_type window = {
        WINDOW_MULTIPLAYER_STATUS,
        draw_background,
        draw_foreground,
        handle_input,
        0,
        0
    };
    window_show(&window);
}

#endif /* ENABLE_MULTIPLAYER */
