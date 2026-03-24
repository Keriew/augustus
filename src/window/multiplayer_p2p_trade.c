#ifdef ENABLE_MULTIPLAYER

#include "multiplayer_p2p_trade.h"

#include "core/string.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "game/resource.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "multiplayer/command_bus.h"
#include "multiplayer/command_types.h"
#include "multiplayer/empire_sync.h"
#include "multiplayer/ownership.h"
#include "multiplayer/player_registry.h"
#include "network/session.h"
#include "translation/translation.h"
#include "window/advisors.h"

#include <stdio.h>
#include <string.h>

#define DIALOG_X 32
#define DIALOG_Y 28
#define DIALOG_W_BLOCKS 38
#define DIALOG_H_BLOCKS 24
#define DIALOG_W (DIALOG_W_BLOCKS * 16)
#define DIALOG_H (DIALOG_H_BLOCKS * 16)

#define RESOURCE_ROW_H 24
#define VISIBLE_ROWS 10
#define LIST_Y 90
#define LIST_CONTENT_Y (LIST_Y + 8)

/* Column positions */
#define LEFT_PANEL_X 12
#define RIGHT_PANEL_X (DIALOG_W / 2 + 4)
#define PANEL_W_BLOCKS 18
#define PANEL_W (PANEL_W_BLOCKS * 16)
#define PANEL_H_BLOCKS ((VISIBLE_ROWS * RESOURCE_ROW_H + 16) / 16 + 1)

/* Forward declarations */
static void button_close(int param1, int param2);

static image_button close_button[] = {
    {DIALOG_W - 38, DIALOG_H - 38, 24, 24, IB_NORMAL, GROUP_CONTEXT_ICONS, 4,
     button_close, button_none, 0, 0, 1},
};

/* Filtered resource lists for each column */
static int sell_list[RESOURCE_MAX];
static int sell_count;
static int buy_list[RESOURCE_MAX];
static int buy_count;

static struct {
    int remote_city_id;
    int local_city_id;
    int route_id;
    int route_exists;
    mp_route_state route_state;
    int prefer_sea;
    int remote_online;

    /* Remote city trade settings */
    int remote_exportable[RESOURCE_MAX];
    int remote_importable[RESOURCE_MAX];

    /* Scroll positions */
    int sell_scroll;
    int buy_scroll;

    char owner_name[32];
    unsigned int focus_button_id;
} data;

/* ---- Helpers ---- */

static int find_local_player_city(void)
{
    int city_id = mp_ownership_find_local_city();
    if (city_id >= 0) {
        return city_id;
    }

    mp_player *local = mp_player_registry_get_local();
    if (local) {
        if (local->assigned_city_id >= 0) {
            return local->assigned_city_id;
        }
        if (local->empire_city_id >= 0) {
            return local->empire_city_id;
        }
    }

    return -1;
}

static void build_filtered_lists(void)
{
    sell_count = 0;
    buy_count = 0;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (data.remote_exportable[r]) {
            sell_list[sell_count++] = r;
        }
        if (data.remote_importable[r]) {
            buy_list[buy_count++] = r;
        }
    }
}

static void clamp_scroll(int *scroll, int total)
{
    int max = total - VISIBLE_ROWS;
    if (max < 0) {
        max = 0;
    }
    if (*scroll > max) {
        *scroll = max;
    }
    if (*scroll < 0) {
        *scroll = 0;
    }
}

static void load_remote_trade_view(void)
{
    data.remote_online = mp_ownership_is_city_online(data.remote_city_id);

    const mp_city_trade_view *view = mp_empire_sync_get_trade_view(data.remote_city_id);
    if (view) {
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            data.remote_exportable[r] = view->exportable[r];
            data.remote_importable[r] = view->importable[r];
        }
    } else {
        const empire_city *city = empire_city_get(data.remote_city_id);
        if (city) {
            for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                data.remote_exportable[r] = city->sells_resource[r];
                data.remote_importable[r] = city->buys_resource[r];
            }
        }
    }
}

static void load_route_state(void)
{
    data.route_id = mp_ownership_find_route_between(data.local_city_id, data.remote_city_id);
    if (data.route_id >= 0 && trade_route_is_valid(data.route_id)) {
        data.route_exists = 1;
        data.route_state = mp_ownership_get_route_state(data.route_id);
    } else {
        data.route_exists = 0;
        data.route_id = -1;
        data.route_state = MP_ROUTE_STATE_INACTIVE;
    }
}

static void init(int remote_city_id)
{
    memset(&data, 0, sizeof(data));
    data.remote_city_id = remote_city_id;
    data.route_id = -1;

    uint8_t owner_pid = mp_ownership_get_city_player_id(remote_city_id);
    mp_player *owner = mp_player_registry_get(owner_pid);
    if (owner && owner->active) {
        snprintf(data.owner_name, sizeof(data.owner_name), "%s", owner->name);
    } else {
        snprintf(data.owner_name, sizeof(data.owner_name), "???");
    }

    data.local_city_id = find_local_player_city();
    load_remote_trade_view();
    load_route_state();
    build_filtered_lists();
}

/* ---- Drawing helpers ---- */

static const uint8_t *route_state_text(mp_route_state state)
{
    switch (state) {
        case MP_ROUTE_STATE_PENDING:  return translation_for(TR_MP_EMPIRE_ROUTE_PENDING);
        case MP_ROUTE_STATE_ACTIVE:   return translation_for(TR_MP_EMPIRE_ROUTE_ACTIVE);
        case MP_ROUTE_STATE_DISABLED: return translation_for(TR_MP_EMPIRE_ROUTE_DISABLED);
        case MP_ROUTE_STATE_OFFLINE:  return translation_for(TR_MP_EMPIRE_ROUTE_OFFLINE);
        default:                      return translation_for(TR_MP_P2P_TRADE_NO_ROUTE);
    }
}

static font_t route_state_font(mp_route_state state)
{
    switch (state) {
        case MP_ROUTE_STATE_ACTIVE:   return FONT_NORMAL_GREEN;
        case MP_ROUTE_STATE_PENDING:  return FONT_NORMAL_BROWN;
        case MP_ROUTE_STATE_DISABLED:
        case MP_ROUTE_STATE_OFFLINE:  return FONT_NORMAL_RED;
        default:                      return FONT_NORMAL_BLACK;
    }
}

/* Draw a resource row: icon + name */
static void draw_resource_row(int x, int y, int resource)
{
    image_draw(resource_get_data(resource)->image.icon, x, y + 2,
               COLOR_MASK_NONE, SCALE_NONE);
    text_draw(resource_get_data(resource)->text, x + 22, y + 4,
              FONT_NORMAL_BLACK, 0);
}

/* Draw scroll indicator arrows */
static void draw_scroll_indicator(int cx, int cy, int col_x, int scroll, int total, int visible)
{
    if (total <= visible) {
        return;
    }

    int ax = cx + col_x + PANEL_W - 24;

    if (scroll > 0) {
        uint8_t tbuf[8];
        string_copy(string_from_ascii("^"), tbuf, 8);
        text_draw(tbuf, ax, cy + LIST_Y + 2, FONT_NORMAL_BLACK, 0);
    }

    if (scroll + visible < total) {
        uint8_t tbuf[8];
        string_copy(string_from_ascii("v"), tbuf, 8);
        text_draw(tbuf, ax, cy + LIST_Y + PANEL_H_BLOCKS * 16 - 16, FONT_NORMAL_BLACK, 0);
    }
}

/* ---- Window callbacks ---- */

static void draw_background(void)
{
    window_draw_underlying_window();

    /* Refresh state each frame */
    load_route_state();
    load_remote_trade_view();
    build_filtered_lists();
    clamp_scroll(&data.sell_scroll, sell_count);
    clamp_scroll(&data.buy_scroll, buy_count);
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    int cx = DIALOG_X;
    int cy = DIALOG_Y;

    outer_panel_draw(cx, cy, DIALOG_W_BLOCKS, DIALOG_H_BLOCKS);

    /* ---- Title: "Trade with [PlayerName]" ---- */
    {
        const uint8_t *title_prefix = translation_for(TR_MP_P2P_TRADE_TITLE);
        uint8_t title_buf[80];
        uint8_t name_buf[40];
        string_copy(string_from_ascii(data.owner_name), name_buf, 40);
        int len = string_length(title_prefix);
        string_copy(title_prefix, title_buf, 80);
        string_copy(name_buf, title_buf + len, 80 - len);
        text_draw_centered(title_buf, cx, cy + 12, DIALOG_W, FONT_LARGE_BLACK, 0);
    }

    /* ---- Online/Offline status ---- */
    {
        translation_key status_key = data.remote_online ? TR_MP_EMPIRE_ONLINE : TR_MP_EMPIRE_OFFLINE;
        font_t status_font = data.remote_online ? FONT_NORMAL_GREEN : FONT_NORMAL_RED;
        text_draw_centered(translation_for(status_key), cx, cy + 34, DIALOG_W, status_font, 0);
    }

    /* ---- Route status ---- */
    int status_y = cy + 52;
    if (data.route_exists) {
        int sw = text_draw(translation_for(TR_MP_P2P_TRADE_STATUS),
            cx + 16, status_y, FONT_NORMAL_BLACK, 0);
        text_draw(route_state_text(data.route_state),
            cx + 16 + sw, status_y, route_state_font(data.route_state), 0);
    } else {
        text_draw_centered(translation_for(TR_MP_P2P_TRADE_NO_ROUTE),
            cx, status_y, DIALOG_W, FONT_NORMAL_BROWN, 0);
    }

    /* ---- Divider ---- */
    int div_y = cy + 68;
    graphics_draw_line(cx + 16, cx + DIALOG_W - 16, div_y, div_y, COLOR_BLACK);

    /* ---- Column headers ---- */
    int header_y = cy + 74;

    text_draw(translation_for(TR_MP_P2P_TRADE_THEY_SELL),
        cx + LEFT_PANEL_X + 8, header_y, FONT_NORMAL_BLACK, 0);
    text_draw(translation_for(TR_MP_P2P_TRADE_THEY_BUY),
        cx + RIGHT_PANEL_X + 8, header_y, FONT_NORMAL_BLACK, 0);

    /* ---- Inner panels ---- */
    int panel_y = cy + LIST_Y;
    inner_panel_draw(cx + LEFT_PANEL_X, panel_y, PANEL_W_BLOCKS, PANEL_H_BLOCKS);
    inner_panel_draw(cx + RIGHT_PANEL_X, panel_y, PANEL_W_BLOCKS, PANEL_H_BLOCKS);

    /* ---- Left column: They Sell (partner's exports) ---- */
    if (sell_count > 0) {
        for (int i = 0; i < VISIBLE_ROWS && i + data.sell_scroll < sell_count; i++) {
            int r = sell_list[i + data.sell_scroll];
            draw_resource_row(cx + LEFT_PANEL_X + 16,
                              cy + LIST_CONTENT_Y + i * RESOURCE_ROW_H, r);
        }
        draw_scroll_indicator(cx, cy, LEFT_PANEL_X, data.sell_scroll, sell_count, VISIBLE_ROWS);
    } else {
        text_draw(translation_for(TR_MP_P2P_TRADE_NOTHING),
            cx + LEFT_PANEL_X + 16, cy + LIST_CONTENT_Y + 4, FONT_NORMAL_BROWN, 0);
    }

    /* ---- Right column: They Buy (partner's imports) ---- */
    if (buy_count > 0) {
        for (int i = 0; i < VISIBLE_ROWS && i + data.buy_scroll < buy_count; i++) {
            int r = buy_list[i + data.buy_scroll];
            draw_resource_row(cx + RIGHT_PANEL_X + 16,
                              cy + LIST_CONTENT_Y + i * RESOURCE_ROW_H, r);
        }
        draw_scroll_indicator(cx, cy, RIGHT_PANEL_X, data.buy_scroll, buy_count, VISIBLE_ROWS);
    } else {
        text_draw(translation_for(TR_MP_P2P_TRADE_NOTHING),
            cx + RIGHT_PANEL_X + 16, cy + LIST_CONTENT_Y + 4, FONT_NORMAL_BROWN, 0);
    }

    /* ---- Bottom buttons ---- */
    int bottom_y = cy + DIALOG_H - 52;

    /* "My Trade" button - opens Trade Advisor to configure own city */
    {
        int mt_x = cx + 16;
        button_border_draw(mt_x, bottom_y, 140, 26, data.focus_button_id == 3);
        text_draw_centered(translation_for(TR_MP_P2P_TRADE_MY_TRADE),
            mt_x, bottom_y + 5, 140, FONT_NORMAL_BLACK, 0);
    }

    /* Create route button (if no route) */
    if (!data.route_exists) {
        if (data.remote_online && data.local_city_id >= 0) {
            int btn_x = cx + (DIALOG_W - 200) / 2;
            button_border_draw(btn_x, bottom_y, 200, 26, data.focus_button_id == 1);
            text_draw_centered(translation_for(TR_MP_P2P_TRADE_CREATE_ROUTE),
                btn_x, bottom_y + 5, 200, FONT_NORMAL_BLACK, 0);

            /* Sea/Land toggle */
            int tog_x = btn_x + 210;
            button_border_draw(tog_x, bottom_y, 60, 26, data.focus_button_id == 2);
            translation_key sea_land = data.prefer_sea ? TR_MP_P2P_TRADE_SEA_PREF : TR_MP_P2P_TRADE_LAND_PREF;
            text_draw_centered(translation_for(sea_land),
                tog_x, bottom_y + 5, 60, FONT_NORMAL_BLACK, 0);
        } else if (!data.remote_online) {
            text_draw_centered(translation_for(TR_MP_P2P_TRADE_CITY_OFFLINE),
                cx + 160, bottom_y + 5, DIALOG_W - 176, FONT_NORMAL_RED, 0);
        }
    }

    /* Close button */
    image_buttons_draw(cx, cy, close_button, 1);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    int cx = DIALOG_X;
    int cy = DIALOG_Y;

    data.focus_button_id = 0;

    /* Close button */
    if (image_buttons_handle_mouse(m_dialog, cx, cy, close_button, 1, 0)) {
        return;
    }

    /* Mouse wheel scrolling */
    if (m_dialog->scrolled != SCROLL_NONE) {
        int scroll_delta = (m_dialog->scrolled == SCROLL_DOWN) ? 3 : -3;
        int half = cx + DIALOG_W / 2;

        if (m_dialog->x < half) {
            data.sell_scroll += scroll_delta;
            clamp_scroll(&data.sell_scroll, sell_count);
        } else {
            data.buy_scroll += scroll_delta;
            clamp_scroll(&data.buy_scroll, buy_count);
        }
        window_invalidate();
        return;
    }

    /* "My Trade" button - always available */
    {
        int mt_x = cx + 16;
        int mt_y = cy + DIALOG_H - 52;
        if (m_dialog->x >= mt_x && m_dialog->x < mt_x + 140 &&
            m_dialog->y >= mt_y && m_dialog->y < mt_y + 26) {
            data.focus_button_id = 3;
            if (m_dialog->left.went_up) {
                window_advisors_show_advisor(ADVISOR_TRADE);
                return;
            }
        }
    }

    /* Create route button (if no route) */
    if (!data.route_exists && data.remote_online && data.local_city_id >= 0) {
        int btn_x = cx + (DIALOG_W - 200) / 2;
        int btn_y = cy + DIALOG_H - 52;

        if (m_dialog->x >= btn_x && m_dialog->x < btn_x + 200 &&
            m_dialog->y >= btn_y && m_dialog->y < btn_y + 26) {
            data.focus_button_id = 1;
            if (m_dialog->left.went_up) {
                mp_command cmd;
                memset(&cmd, 0, sizeof(cmd));
                cmd.command_type = MP_CMD_CREATE_TRADE_ROUTE;
                cmd.player_id = net_session_get_local_player_id();
                cmd.data.create_route.origin_city_id = data.local_city_id;
                cmd.data.create_route.dest_city_id = data.remote_city_id;
                cmd.data.create_route.prefer_sea = data.prefer_sea;
                mp_command_bus_submit(&cmd);
                window_invalidate();
                return;
            }
        }

        /* Sea/Land toggle */
        int tog_x = btn_x + 210;
        if (m_dialog->x >= tog_x && m_dialog->x < tog_x + 60 &&
            m_dialog->y >= btn_y && m_dialog->y < btn_y + 26) {
            data.focus_button_id = 2;
            if (m_dialog->left.went_up) {
                data.prefer_sea = !data.prefer_sea;
                window_invalidate();
                return;
            }
        }
    }

    if (input_go_back_requested(m, h)) {
        window_go_back();
    }
}

/* ---- Button callbacks ---- */

static void button_close(int param1, int param2)
{
    window_go_back();
}

/* ---- Public ---- */

void window_multiplayer_p2p_trade_show(int remote_city_id)
{
    init(remote_city_id);

    window_type window = {
        WINDOW_MULTIPLAYER_P2P_TRADE,
        draw_background,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}

#endif /* ENABLE_MULTIPLAYER */
