#ifdef ENABLE_MULTIPLAYER

#include "multiplayer_resume_lobby.h"

#include "core/string.h"
#include "multiplayer/bootstrap.h"
#include "multiplayer/player_registry.h"
#include "multiplayer/ownership.h"
#include "graphics/button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/list_box.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
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
#define LIST_Y (PANEL_Y + 80)
#define LIST_WIDTH_BLOCKS 26
#define LIST_HEIGHT_BLOCKS 10
#define LIST_ITEM_HEIGHT 24

#define RESUME_BUTTON_X (PANEL_X + 120)
#define RESUME_BUTTON_Y (PANEL_Y + PANEL_HEIGHT_BLOCKS * 16 - 48)
#define LEAVE_BUTTON_X (PANEL_X + 320)
#define LEAVE_BUTTON_Y RESUME_BUTTON_Y
#define BUTTON_WIDTH 160
#define BUTTON_HEIGHT 24

static void button_resume(const generic_button *button);
static void button_leave(const generic_button *button);
static void draw_player_item(const list_box_item *item);
static void select_player(unsigned int index, int is_double_click);

static generic_button buttons[] = {
    {RESUME_BUTTON_X, RESUME_BUTTON_Y, BUTTON_WIDTH, BUTTON_HEIGHT, button_resume, 0, 0},
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

    /* Reconnection status */
    if (player->status == MP_PLAYER_IN_GAME || player->connection_state == MP_CONNECTION_CONNECTED) {
        const uint8_t *status_text = translation_for(TR_MP_RESUME_RECONNECTED);
        text_draw(status_text, item->x + 240, item->y + 4, FONT_NORMAL_GREEN, 0);
    } else if (player->status == MP_PLAYER_AWAITING_RECONNECT) {
        const uint8_t *status_text = translation_for(TR_MP_RESUME_AWAITING);
        text_draw(status_text, item->x + 240, item->y + 4, FONT_NORMAL_PLAIN, 0);
    } else {
        const uint8_t *status_text = translation_for(TR_MP_RESUME_WILL_BE_OFFLINE);
        text_draw(status_text, item->x + 240, item->y + 4, FONT_NORMAL_RED, 0);
    }

    if (item->is_focused) {
        button_border_draw(item->x, item->y, item->width, item->height, 1);
    }
}

static void select_player(unsigned int index, int is_double_click)
{
    window_invalidate();
}

static void button_resume(const generic_button *button)
{
    if (!net_session_is_host()) {
        return;
    }
    mp_bootstrap_host_launch_resumed_game();
}

static void button_leave(const generic_button *button)
{
    net_session_disconnect();
    net_session_clear_join_status();
    window_go_back();
}

static void draw_background(void)
{
    graphics_in_dialog();
    outer_panel_draw(PANEL_X, PANEL_Y, PANEL_WIDTH_BLOCKS, PANEL_HEIGHT_BLOCKS);

    /* Title */
    const uint8_t *title = translation_for(TR_MP_RESUME_TITLE);
    text_draw_centered(title, PANEL_X, PANEL_Y + 14,
        PANEL_WIDTH_BLOCKS * 16, FONT_LARGE_BLACK, 0);

    /* Save info */
    {
        const char *scenario = mp_bootstrap_get_scenario_name();
        if (!scenario || !scenario[0]) {
            scenario = "Unknown";
        }
        char info_str[128];
        snprintf(info_str, sizeof(info_str), "Save: %s", scenario);
        uint8_t info_buf[128];
        string_copy(string_from_ascii(info_str), info_buf, 128);
        text_draw_centered(info_buf, PANEL_X, PANEL_Y + 42,
            PANEL_WIDTH_BLOCKS * 16, FONT_NORMAL_BLACK, 0);
    }

    /* Column headers */
    int header_y = LIST_Y - 18;
    const uint8_t *players_label = translation_for(TR_MP_RESUME_PLAYERS);
    text_draw(players_label, LIST_X + 8, header_y, FONT_NORMAL_BLACK, 0);
    const uint8_t *status_label = translation_for(TR_MP_RESUME_STATUS);
    text_draw(status_label, LIST_X + 240, header_y, FONT_NORMAL_BLACK, 0);

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    refresh_player_list();
    list_box_draw(&player_list);

    /* Resume button (host only) */
    large_label_draw(RESUME_BUTTON_X, RESUME_BUTTON_Y,
        BUTTON_WIDTH / 16, data.focus_button_id == 1 ? 1 : 0);
    const uint8_t *resume_text = translation_for(TR_MP_RESUME_BUTTON_RESUME);
    text_draw_centered(resume_text, RESUME_BUTTON_X, RESUME_BUTTON_Y + 5,
        BUTTON_WIDTH, FONT_NORMAL_GREEN, 0);

    /* Leave button */
    large_label_draw(LEAVE_BUTTON_X, LEAVE_BUTTON_Y,
        BUTTON_WIDTH / 16, data.focus_button_id == 2 ? 1 : 0);
    const uint8_t *leave_text = translation_for(TR_MP_RESUME_BUTTON_LEAVE);
    text_draw_centered(leave_text, LEAVE_BUTTON_X, LEAVE_BUTTON_Y + 5,
        BUTTON_WIDTH, FONT_NORMAL_GREEN, 0);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    net_session_state state = net_session_get_state();
    if (state == NET_SESSION_HOSTING_GAME || state == NET_SESSION_CLIENT_GAME) {
        return;
    }
    if (state == NET_SESSION_IDLE) {
        window_go_back();
        return;
    }

    if (generic_buttons_handle_mouse(m_dialog, 0, 0, buttons, 2, &data.focus_button_id)) {
        return;
    }

    if (list_box_handle_input(&player_list, m_dialog, 1)) {
        return;
    }

    if (input_go_back_requested(m, h)) {
        button_leave(0);
    }

    list_box_request_refresh(&player_list);
}

void window_multiplayer_resume_lobby_show(void)
{
    memset(&data, 0, sizeof(data));

    refresh_player_list();
    list_box_init(&player_list, data.player_count);

    window_type window = {
        WINDOW_MULTIPLAYER_RESUME_LOBBY,
        draw_background,
        draw_foreground,
        handle_input,
        0,
        0
    };
    window_show(&window);
}

#endif /* ENABLE_MULTIPLAYER */
