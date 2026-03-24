#ifdef ENABLE_MULTIPLAYER

#include "multiplayer_p2p_trade.h"

#include "core/image_group.h"
#include "core/string.h"
#include "empire/city.h"
#include "empire/empire.h"
#include "empire/trade_prices.h"
#include "empire/trade_route.h"
#include "game/resource.h"
#include "graphics/arrow_button.h"
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

#include <stdio.h>
#include <string.h>

#define DIALOG_X 32
#define DIALOG_Y 16
#define DIALOG_W_BLOCKS 38
#define DIALOG_H_BLOCKS 28
#define DIALOG_W (DIALOG_W_BLOCKS * 16)
#define DIALOG_H (DIALOG_H_BLOCKS * 16)

#define RESOURCE_ROW_H 22
#define VISIBLE_ROWS 12
#define LIST_Y 100
#define LIST_CONTENT_Y (LIST_Y + 8)

/* Column positions */
#define LEFT_PANEL_X 12
#define RIGHT_PANEL_X (DIALOG_W / 2 + 4)
#define PANEL_W_BLOCKS 18
#define PANEL_W (PANEL_W_BLOCKS * 16)
#define PANEL_H_BLOCKS ((VISIBLE_ROWS * RESOURCE_ROW_H + 16) / 16 + 1)

#define LIMIT_STEP 1 
#define DEFAULT_LIMIT 1500
#define MAX_LIMIT 500

/* Forward declarations */
static void button_close(int param1, int param2);
static void button_toggle_export(const generic_button *btn);
static void button_limit_change(int param1, int param2);

static image_button close_button[] = {
    {DIALOG_W - 38, DIALOG_H - 38, 24, 24, IB_NORMAL, GROUP_CONTEXT_ICONS, 4,
     button_close, button_none, 0, 0, 1},
};

/* Dynamic toggle buttons for visible export rows */
#define MAX_VIS_BUTTONS 14
static generic_button export_toggle_buttons[MAX_VIS_BUTTONS];
static int export_toggle_count;

/* Dynamic arrow buttons for visible export rows */
#define MAX_VIS_ARROWS 28
static arrow_button export_arrow_buttons[MAX_VIS_ARROWS];
static int export_arrow_count;

/* Resource list */
static int resource_list[RESOURCE_MAX];
static int resource_count;

static struct {
    int remote_city_id;
    int local_city_id;
    int route_id;
    uint32_t network_route_id;
    int route_exists;
    mp_route_state route_state;
    int prefer_sea;
    int remote_online;
    int local_is_origin;

    /* Remote city capabilities (for display before/after route) */
    int remote_exportable[RESOURCE_MAX];
    int remote_importable[RESOURCE_MAX];

    /* Trade limits from local player's perspective */
    int my_export_limit[RESOURCE_MAX];
    int partner_export_limit[RESOURCE_MAX];

    /* Scroll positions */
    int export_scroll;
    int partner_scroll;

    char owner_name[32];
    unsigned int focus_button_id;
} data;

/* ---- Helpers ---- */

static int find_local_player_city(void)
{
    /* Try player registry first (assigned during bootstrap) */
    mp_player *local = mp_player_registry_get_local();
    if (local) {
        if (local->assigned_city_id >= 0) {
            return local->assigned_city_id;
        }
        if (local->empire_city_id >= 0) {
            return local->empire_city_id;
        }
    }

    /* Fallback: search empire cities for one owned by this player */
    uint8_t local_id = net_session_get_local_player_id();
    int city_id = empire_get_city_id_for_player(local_id);
    if (city_id >= 0) {
        return city_id;
    }

    /* Last resort: empire_sync (only works for remote perspective) */
    if (local) {
        return mp_empire_sync_get_city_id_for_player(local->player_id);
    }
    return -1;
}

static void build_resource_list(void)
{
    resource_count = 0;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        resource_list[resource_count++] = r;
    }
}

static void clamp_scroll(int *scroll)
{
    int max = resource_count - VISIBLE_ROWS;
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
        data.network_route_id = mp_ownership_get_network_route_id(data.route_id);

        /* Determine perspective: origin exports = direction 0, dest exports = direction 1 */
        uint8_t origin_player = mp_ownership_get_route_origin_player(data.route_id);
        data.local_is_origin = (origin_player == net_session_get_local_player_id());

        int my_dir = data.local_is_origin ? 0 : 1;
        int partner_dir = data.local_is_origin ? 1 : 0;

        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            data.my_export_limit[r] = trade_route_limit(data.route_id, r, my_dir);
            data.partner_export_limit[r] = trade_route_limit(data.route_id, r, partner_dir);
        }
    } else {
        data.route_exists = 0;
        data.route_id = -1;
        data.route_state = MP_ROUTE_STATE_INACTIVE;
        memset(data.my_export_limit, 0, sizeof(data.my_export_limit));
        memset(data.partner_export_limit, 0, sizeof(data.partner_export_limit));
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
    build_resource_list();
    load_remote_trade_view();
    load_route_state();
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

/* ---- Submit export limit command ---- */

static void submit_export_command(int resource, int new_limit)
{
    if (!data.route_exists || data.route_id < 0) {
        return;
    }

    /* Origin exports in direction 0 (selling), dest exports in direction 1 (buying) */
    int is_buying = data.local_is_origin ? 0 : 1;

    mp_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = MP_CMD_SET_ROUTE_LIMIT;
    cmd.player_id = net_session_get_local_player_id();
    cmd.data.route_limit.route_id = data.route_id;
    cmd.data.route_limit.network_route_id = data.network_route_id;
    cmd.data.route_limit.resource = resource;
    cmd.data.route_limit.is_buying = is_buying;
    cmd.data.route_limit.amount = new_limit;
    mp_command_bus_submit(&cmd);

    /* Optimistic update */
    data.my_export_limit[resource] = new_limit;
    window_invalidate();
}

/* ---- Build buttons for visible export rows ---- */

static void build_export_buttons(void)
{
    export_toggle_count = 0;
    export_arrow_count = 0;

    if (!data.route_exists || data.route_state == MP_ROUTE_STATE_INACTIVE) {
        return;
    }

    for (int i = 0; i < VISIBLE_ROWS && i + data.export_scroll < resource_count; i++) {
        int r = resource_list[i + data.export_scroll];
        int row_y = LIST_CONTENT_Y + i * RESOURCE_ROW_H;

        if (export_toggle_count >= MAX_VIS_BUTTONS) {
            break;
        }

        /* On/Off toggle button */
        generic_button *btn = &export_toggle_buttons[export_toggle_count];
        btn->x = LEFT_PANEL_X + 8;
        btn->y = row_y;
        btn->width = 30;
        btn->height = RESOURCE_ROW_H - 2;
        btn->left_click_handler = button_toggle_export;
        btn->parameter1 = r;
        btn->parameter2 = 0;
        export_toggle_count++;

        /* Arrow buttons for limit adjustment (only if active) */
        if (data.my_export_limit[r] > 0 && export_arrow_count + 1 < MAX_VIS_ARROWS) {
            arrow_button *down = &export_arrow_buttons[export_arrow_count];
            down->x_offset = LEFT_PANEL_X + 176;
            down->y_offset = row_y + 1;
            down->image_id = 17;
            down->size = 24;
            down->left_click_handler = button_limit_change;
            down->parameter1 = r;
            down->parameter2 = 1; /* down */
            down->pressed = 0;
            down->repeats = 0;
            down->last_time = 0;
            export_arrow_count++;

            arrow_button *up = &export_arrow_buttons[export_arrow_count];
            up->x_offset = LEFT_PANEL_X + 198;
            up->y_offset = row_y + 1;
            up->image_id = 15;
            up->size = 24;
            up->left_click_handler = button_limit_change;
            up->parameter1 = r;
            up->parameter2 = 0; /* up */
            up->pressed = 0;
            up->repeats = 0;
            up->last_time = 0;
            export_arrow_count++;
        }
    }
}

/* ---- Draw a single export row (left column, configurable) ---- */

static void draw_export_row(int cx, int cy, int y, int resource)
{
    int ax = cx + LEFT_PANEL_X + 8;
    int limit = data.my_export_limit[resource];
    int is_active = (limit > 0);

    /* On/Off toggle */
    font_t toggle_font = is_active ? FONT_NORMAL_GREEN : FONT_NORMAL_RED;
    translation_key toggle_key = is_active ? TR_MP_P2P_TRADE_ON : TR_MP_P2P_TRADE_OFF;
    button_border_draw(ax, cy + y, 30, RESOURCE_ROW_H - 2, 0);
    text_draw_centered(translation_for(toggle_key), ax, cy + y + 4, 30, toggle_font, 0);

    /* Resource icon + name */
    image_draw(resource_get_data(resource)->image.icon, ax + 34, cy + y + 1,
               COLOR_MASK_NONE, SCALE_NONE);
    text_draw(resource_get_data(resource)->text, ax + 54, cy + y + 4,
              is_active ? FONT_NORMAL_BLACK : FONT_NORMAL_BROWN, 0);

    if (is_active) {
        /* Limit value */
        int loads = limit / 100;
        char lbuf[16];
        snprintf(lbuf, sizeof(lbuf), "%d", loads);
        uint8_t ltext[16];
        string_copy(string_from_ascii(lbuf), ltext, 16);
        text_draw(ltext, ax + 148, cy + y + 4, FONT_NORMAL_GREEN, 0);

        /* Price */
        int price = trade_price_base_sell(resource);
        char pbuf[16];
        snprintf(pbuf, sizeof(pbuf), "%d Dn", price);
        uint8_t ptext[16];
        string_copy(string_from_ascii(pbuf), ptext, 16);
        text_draw(ptext, ax + 222, cy + y + 4, FONT_SMALL_PLAIN, 0);
    } else {
        /* "Wanted" indicator if partner city imports this resource */
        if (data.remote_importable[resource]) {
            text_draw(translation_for(TR_MP_P2P_TRADE_WANTED),
                      ax + 148, cy + y + 4, FONT_NORMAL_GREEN, 0);
        } else {
            text_draw(translation_for(TR_MP_P2P_TRADE_NOT_TRADING),
                      ax + 148, cy + y + 4, FONT_NORMAL_BROWN, 0);
        }
    }
}

/* ---- Draw a single partner export row (right column, read-only) ---- */

static void draw_partner_row(int cx, int cy, int y, int resource)
{
    int ax = cx + RIGHT_PANEL_X + 8;
    int limit = data.partner_export_limit[resource];
    int is_active = (limit > 0);

    /* Status indicator */
    font_t status_font = is_active ? FONT_NORMAL_GREEN : FONT_NORMAL_RED;
    translation_key status_key = is_active ? TR_MP_P2P_TRADE_ON : TR_MP_P2P_TRADE_OFF;
    text_draw_centered(translation_for(status_key), ax, cy + y + 4, 26, status_font, 0);

    /* Resource icon + name */
    image_draw(resource_get_data(resource)->image.icon, ax + 30, cy + y + 1,
               COLOR_MASK_NONE, SCALE_NONE);
    font_t name_font = is_active ? FONT_NORMAL_BLACK : FONT_NORMAL_BROWN;
    text_draw(resource_get_data(resource)->text, ax + 50, cy + y + 4, name_font, 0);

    if (is_active) {
        /* Limit */
        int loads = limit / 100;
        char lbuf[16];
        snprintf(lbuf, sizeof(lbuf), "%d", loads);
        uint8_t ltext[16];
        string_copy(string_from_ascii(lbuf), ltext, 16);
        text_draw(ltext, ax + 148, cy + y + 4, FONT_NORMAL_GREEN, 0);

        /* Price */
        int price = trade_price_base_buy(resource);
        char pbuf[16];
        snprintf(pbuf, sizeof(pbuf), "%d Dn", price);
        uint8_t ptext[16];
        string_copy(string_from_ascii(pbuf), ptext, 16);
        text_draw(ptext, ax + 188, cy + y + 4, FONT_SMALL_PLAIN, 0);
    } else {
        text_draw(translation_for(TR_MP_P2P_TRADE_NOT_TRADING),
                  ax + 148, cy + y + 4, FONT_NORMAL_BROWN, 0);
    }
}

/* ---- Draw "no route" view (city capabilities) ---- */

static void draw_no_route_list(int cx, int cy, int col_x, int resources[])
{
    int ax = cx + col_x + 8;
    int iy = cy + LIST_CONTENT_Y;

    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (!resources[r]) {
            continue;
        }
        image_draw(resource_get_data(r)->image.icon, ax + 4, iy + 1,
                   COLOR_MASK_NONE, SCALE_NONE);
        text_draw(resource_get_data(r)->text, ax + 24, iy + 4, FONT_NORMAL_BLACK, 0);
        iy += RESOURCE_ROW_H;

        /* Don't overflow the panel */
        if (iy > cy + LIST_Y + PANEL_H_BLOCKS * 16 - 16) {
            break;
        }
    }
}

/* ---- Draw scroll indicator arrows ---- */

static void draw_scroll_indicator(int cx, int cy, int col_x, int scroll, int total, int visible)
{
    if (total <= visible) {
        return;
    }

    int ax = cx + col_x + PANEL_W - 24;

    /* Up arrow indicator */
    if (scroll > 0) {
        char buf[8];
        snprintf(buf, sizeof(buf), "^");
        uint8_t tbuf[8];
        string_copy(string_from_ascii(buf), tbuf, 8);
        text_draw(tbuf, ax, cy + LIST_Y + 2, FONT_NORMAL_BLACK, 0);
    }

    /* Down arrow indicator */
    if (scroll + visible < total) {
        char buf[8];
        snprintf(buf, sizeof(buf), "v");
        uint8_t tbuf[8];
        string_copy(string_from_ascii(buf), tbuf, 8);
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
    clamp_scroll(&data.export_scroll);
    clamp_scroll(&data.partner_scroll);
    build_export_buttons();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    int cx = DIALOG_X;
    int cy = DIALOG_Y;

    outer_panel_draw(cx, cy, DIALOG_W_BLOCKS, DIALOG_H_BLOCKS);

    /* ---- Title ---- */
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

    /* Online/Offline */
    {
        translation_key status_key = data.remote_online ? TR_MP_EMPIRE_ONLINE : TR_MP_EMPIRE_OFFLINE;
        font_t status_font = data.remote_online ? FONT_NORMAL_GREEN : FONT_NORMAL_RED;
        text_draw_centered(translation_for(status_key), cx, cy + 34, DIALOG_W, status_font, 0);
    }

    /* ---- Route status or "no route" ---- */
    int status_y = cy + 52;
    if (data.route_exists) {
        /* Route status label */
        int sw = text_draw(translation_for(TR_MP_P2P_TRADE_STATUS),
            cx + 16, status_y, FONT_NORMAL_BLACK, 0);
        text_draw(route_state_text(data.route_state),
            cx + 16 + sw, status_y, route_state_font(data.route_state), 0);

        /* Enable/Disable button */
        int btn_x = cx + DIALOG_W - 220;
        if (data.route_state == MP_ROUTE_STATE_ACTIVE) {
            button_border_draw(btn_x, status_y - 4, 100, 22, data.focus_button_id == 10);
            text_draw_centered(translation_for(TR_MP_P2P_TRADE_DISABLE_ROUTE),
                btn_x, status_y, 100, FONT_NORMAL_RED, 0);
        } else if (data.route_state == MP_ROUTE_STATE_DISABLED) {
            button_border_draw(btn_x, status_y - 4, 100, 22, data.focus_button_id == 10);
            text_draw_centered(translation_for(TR_MP_P2P_TRADE_ENABLE_ROUTE),
                btn_x, status_y, 100, FONT_NORMAL_GREEN, 0);
        }

        /* Delete button */
        int del_x = cx + DIALOG_W - 110;
        button_border_draw(del_x, status_y - 4, 120, 22, data.focus_button_id == 11);
        text_draw_centered(translation_for(TR_MP_P2P_TRADE_DELETE_ROUTE),
            del_x, status_y, 96, FONT_NORMAL_RED, 0);
    } else {
        text_draw_centered(translation_for(TR_MP_P2P_TRADE_NO_ROUTE),
            cx, status_y, DIALOG_W, FONT_NORMAL_BROWN, 0);
    }

    /* ---- Divider ---- */
    int div_y = cy + 72;
    graphics_draw_line(cx + 16, cx + DIALOG_W - 16, div_y, div_y, COLOR_BLACK);

    /* ---- Column headers ---- */
    int header_y = cy + 78;

    if (data.route_exists && data.route_state != MP_ROUTE_STATE_INACTIVE) {
        /* With route: Your Exports / Partner Exports */
        text_draw(translation_for(TR_MP_P2P_TRADE_YOUR_EXPORTS),
            cx + LEFT_PANEL_X + 8, header_y, FONT_NORMAL_BLACK, 0);
        text_draw(translation_for(TR_MP_P2P_TRADE_PRICE),
            cx + LEFT_PANEL_X + PANEL_W - 50, header_y, FONT_SMALL_PLAIN, 0);

        text_draw(translation_for(TR_MP_P2P_TRADE_PARTNER_EXPORTS),
            cx + RIGHT_PANEL_X + 8, header_y, FONT_NORMAL_BLACK, 0);
        text_draw(translation_for(TR_MP_P2P_TRADE_PRICE),
            cx + RIGHT_PANEL_X + PANEL_W - 50, header_y, FONT_SMALL_PLAIN, 0);

        /* Inner panels for lists */
        int panel_y = cy + LIST_Y;
        inner_panel_draw(cx + LEFT_PANEL_X, panel_y, PANEL_W_BLOCKS, PANEL_H_BLOCKS);
        inner_panel_draw(cx + RIGHT_PANEL_X, panel_y, PANEL_W_BLOCKS, PANEL_H_BLOCKS);

        /* Export rows (left column, scrollable) */
        for (int i = 0; i < VISIBLE_ROWS && i + data.export_scroll < resource_count; i++) {
            int r = resource_list[i + data.export_scroll];
            draw_export_row(cx, cy, LIST_CONTENT_Y + i * RESOURCE_ROW_H, r);
        }

        /* Partner export rows (right column, scrollable) */
        for (int i = 0; i < VISIBLE_ROWS && i + data.partner_scroll < resource_count; i++) {
            int r = resource_list[i + data.partner_scroll];
            draw_partner_row(cx, cy, LIST_CONTENT_Y + i * RESOURCE_ROW_H, r);
        }

        /* Arrow buttons for limits */
        arrow_buttons_draw(cx, cy, export_arrow_buttons, export_arrow_count);

        /* Scroll indicators */
        draw_scroll_indicator(cx, cy, LEFT_PANEL_X, data.export_scroll, resource_count, VISIBLE_ROWS);
        draw_scroll_indicator(cx, cy, RIGHT_PANEL_X, data.partner_scroll, resource_count, VISIBLE_ROWS);
    } else {
        /* No route: show city capabilities */
        text_draw(translation_for(TR_MP_P2P_TRADE_THEY_CAN_EXPORT),
            cx + LEFT_PANEL_X + 8, header_y, FONT_NORMAL_BLACK, 0);
        text_draw(translation_for(TR_MP_P2P_TRADE_THEY_CAN_IMPORT),
            cx + RIGHT_PANEL_X + 8, header_y, FONT_NORMAL_BLACK, 0);

        /* Inner panels */
        int panel_y = cy + LIST_Y;
        inner_panel_draw(cx + LEFT_PANEL_X, panel_y, PANEL_W_BLOCKS, PANEL_H_BLOCKS);
        inner_panel_draw(cx + RIGHT_PANEL_X, panel_y, PANEL_W_BLOCKS, PANEL_H_BLOCKS);

        /* Resource lists (what the remote city can export/import) */
        draw_no_route_list(cx, cy, LEFT_PANEL_X, data.remote_exportable);
        draw_no_route_list(cx, cy, RIGHT_PANEL_X, data.remote_importable);
    }

    /* ---- Bottom: Create route button (if no route) ---- */
    if (!data.route_exists) {
        int btn_y = cy + DIALOG_H - 56;

        if (data.remote_online && data.local_city_id >= 0) {
            int btn_x = cx + (DIALOG_W - 200) / 2;
            button_border_draw(btn_x, btn_y, 200, 26, data.focus_button_id == 1);
            text_draw_centered(translation_for(TR_MP_P2P_TRADE_CREATE_ROUTE),
                btn_x, btn_y + 5, 200, FONT_NORMAL_BLACK, 0);

            /* Sea/Land toggle */
            int tog_x = btn_x + 210;
            button_border_draw(tog_x, btn_y, 60, 26, data.focus_button_id == 2);
            translation_key sea_land = data.prefer_sea ? TR_MP_P2P_TRADE_SEA_PREF : TR_MP_P2P_TRADE_LAND_PREF;
            text_draw_centered(translation_for(sea_land),
                tog_x, btn_y + 5, 60, FONT_NORMAL_BLACK, 0);
        } else if (!data.remote_online) {
            text_draw_centered(translation_for(TR_MP_P2P_TRADE_CITY_OFFLINE),
                cx, btn_y + 5, DIALOG_W, FONT_NORMAL_RED, 0);
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
            data.export_scroll += scroll_delta;
            clamp_scroll(&data.export_scroll);
        } else {
            data.partner_scroll += scroll_delta;
            clamp_scroll(&data.partner_scroll);
        }
        window_invalidate();
        return;
    }

    if (!data.route_exists) {
        /* Create route button */
        if (data.remote_online && data.local_city_id >= 0) {
            int btn_x = cx + (DIALOG_W - 200) / 2;
            int btn_y = cy + DIALOG_H - 56;

            if (m_dialog->x >= btn_x && m_dialog->x < btn_x + 200 &&
                m_dialog->y >= btn_y && m_dialog->y < btn_y + 26) {
                data.focus_button_id = 1;
                if (m_dialog->left.went_up) {
                    /* Create route */
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
    } else {
        /* Route action buttons */
        int status_y = cy + 52;

        /* Enable/Disable button */
        int btn_x = cx + DIALOG_W - 220;
        if (m_dialog->x >= btn_x && m_dialog->x < btn_x + 100 &&
            m_dialog->y >= status_y - 4 && m_dialog->y < status_y + 18) {
            data.focus_button_id = 10;
            if (m_dialog->left.went_up) {
                if (data.route_state == MP_ROUTE_STATE_ACTIVE ||
                    data.route_state == MP_ROUTE_STATE_DISABLED) {
                    mp_command cmd;
                    memset(&cmd, 0, sizeof(cmd));
                    cmd.player_id = net_session_get_local_player_id();
                    if (data.route_state == MP_ROUTE_STATE_ACTIVE) {
                        cmd.command_type = MP_CMD_DISABLE_TRADE_ROUTE;
                        cmd.data.disable_route.route_id = data.route_id;
                        cmd.data.disable_route.network_route_id = data.network_route_id;
                    } else {
                        cmd.command_type = MP_CMD_ENABLE_TRADE_ROUTE;
                        cmd.data.enable_route.route_id = data.route_id;
                        cmd.data.enable_route.network_route_id = data.network_route_id;
                    }
                    mp_command_bus_submit(&cmd);
                    window_invalidate();
                }
                return;
            }
        }

        /* Delete button */
        int del_x = cx + DIALOG_W - 110;
        if (m_dialog->x >= del_x && m_dialog->x < del_x + 96 &&
            m_dialog->y >= status_y - 4 && m_dialog->y < status_y + 18) {
            data.focus_button_id = 11;
            if (m_dialog->left.went_up) {
                mp_command cmd;
                memset(&cmd, 0, sizeof(cmd));
                cmd.command_type = MP_CMD_DELETE_TRADE_ROUTE;
                cmd.player_id = net_session_get_local_player_id();
                cmd.data.delete_route.route_id = data.route_id;
                cmd.data.delete_route.network_route_id = data.network_route_id;
                mp_command_bus_submit(&cmd);
                window_invalidate();
                return;
            }
        }

        /* Arrow buttons for limit adjustment */
        if (export_arrow_count > 0) {
            unsigned int arrow_focus = 0;
            if (arrow_buttons_handle_mouse(m_dialog, cx, cy, export_arrow_buttons,
                                           export_arrow_count, &arrow_focus)) {
                return;
            }
        }

        /* Export toggle buttons */
        for (int i = 0; i < export_toggle_count; i++) {
            generic_button *btn = &export_toggle_buttons[i];
            if (m_dialog->x >= cx + btn->x && m_dialog->x < cx + btn->x + btn->width &&
                m_dialog->y >= cy + btn->y && m_dialog->y < cy + btn->y + btn->height) {
                if (m_dialog->left.went_up) {
                    button_toggle_export(btn);
                    return;
                }
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

static void button_toggle_export(const generic_button *btn)
{
    if (!data.route_exists || data.route_id < 0) {
        return;
    }

    int resource = btn->parameter1;
    int current = data.my_export_limit[resource];

    /* Toggle: OFF (0) -> ON (default limit), ON (>0) -> OFF (0) */
    int new_limit = (current > 0) ? 0 : DEFAULT_LIMIT;
    submit_export_command(resource, new_limit);
}

static void button_limit_change(int param1, int param2)
{
    if (!data.route_exists || data.route_id < 0) {
        return;
    }

    int resource = param1;
    int is_down = param2;
    int current = data.my_export_limit[resource];

    int new_limit;
    if (is_down) {
        new_limit = current - LIMIT_STEP;
        if (new_limit < 100) {
            new_limit = 100; /* Minimum 1 load */
        }
    } else {
        new_limit = current + LIMIT_STEP;
        if (new_limit > MAX_LIMIT) {
            new_limit = MAX_LIMIT;
        }
    }

    if (new_limit != current) {
        submit_export_command(resource, new_limit);
    }
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
