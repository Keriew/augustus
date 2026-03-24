#ifdef ENABLE_MULTIPLAYER

#include "multiplayer_p2p_trade.h"

#include "core/image_group.h"
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

#include <stdio.h>
#include <string.h>

#define DIALOG_X 48
#define DIALOG_Y 32
#define DIALOG_W_BLOCKS 35
#define DIALOG_H_BLOCKS 24
#define DIALOG_W (DIALOG_W_BLOCKS * 16)
#define DIALOG_H (DIALOG_H_BLOCKS * 16)

#define MAX_TRADEABLE 16
#define RESOURCE_ROW_H 24
#define RESOURCE_ICON_SIZE 20

static void button_close(int param1, int param2);
static void button_create_route(int param1, int param2);
static void button_delete_route(int param1, int param2);
static void button_toggle_route(int param1, int param2);
static void button_toggle_sea(int param1, int param2);
static void button_toggle_resource(const generic_button *btn);
static void button_increase_limit(const generic_button *btn);
static void button_decrease_limit(const generic_button *btn);

static image_button close_button[] = {
    {DIALOG_W - 38, DIALOG_H - 38, 24, 24, IB_NORMAL, GROUP_CONTEXT_ICONS, 4,
     button_close, button_none, 0, 0, 1},
};

/* Action buttons: create, delete, enable/disable, sea/land toggle */
static generic_button action_buttons[] = {
    {16, DIALOG_H - 52, 160, 24, button_toggle_resource},    /* placeholder — overridden */
    {16, DIALOG_H - 52, 160, 24, button_toggle_resource},
};

/* Resource toggle buttons (generated dynamically) */
#define MAX_RESOURCE_BUTTONS 40
static generic_button resource_buttons[MAX_RESOURCE_BUTTONS];
static int resource_button_count;

static struct {
    int remote_city_id;
    int local_city_id;
    int route_id;
    uint32_t network_route_id;
    int route_exists;
    mp_route_state route_state;
    int prefer_sea;
    int remote_online;

    /* Remote city capabilities */
    int remote_exportable[RESOURCE_MAX];
    int remote_importable[RESOURCE_MAX];

    /* Tradeable resources (intersection of capabilities) */
    int can_export[RESOURCE_MAX];     /* We can sell to them (they import) */
    int can_import[RESOURCE_MAX];     /* We can buy from them (they export) */
    int export_count;
    int import_count;

    /* Current route config (if route exists) */
    int export_limit[RESOURCE_MAX];
    int import_limit[RESOURCE_MAX];

    /* Owner info */
    char owner_name[32];
    unsigned int focus_button_id;
} data;

static int find_local_player_city(void)
{
    mp_player *local = mp_player_registry_get_local();
    if (!local) {
        return -1;
    }
    return mp_empire_sync_get_city_id_for_player(local->player_id);
}

static void load_remote_trade_view(void)
{
    const mp_city_trade_view *view = mp_empire_sync_get_trade_view(data.remote_city_id);
    if (view) {
        data.remote_online = view->online;
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            data.remote_exportable[r] = view->exportable[r];
            data.remote_importable[r] = view->importable[r];
        }
    } else {
        /* Fallback: use empire city data directly */
        const empire_city *city = empire_city_get(data.remote_city_id);
        if (city) {
            data.remote_online = mp_ownership_is_city_online(data.remote_city_id);
            for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
                data.remote_exportable[r] = city->sells_resource[r];
                data.remote_importable[r] = city->buys_resource[r];
            }
        }
    }

    /* Compute tradeable resources */
    data.export_count = 0;
    data.import_count = 0;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        /* We can export to them if they import that resource */
        data.can_export[r] = data.remote_importable[r];
        if (data.can_export[r]) {
            data.export_count++;
        }
        /* We can import from them if they export that resource */
        data.can_import[r] = data.remote_exportable[r];
        if (data.can_import[r]) {
            data.import_count++;
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

        /* Load current limits */
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            data.export_limit[r] = trade_route_limit(data.route_id, r, 0);
            data.import_limit[r] = trade_route_limit(data.route_id, r, 1);
        }
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

    /* Get owner name */
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
}

/* ---- Drawing helpers ---- */

static void draw_resource_icons(int x, int y, int resources[], int max_width)
{
    int cx = x;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (!resources[r]) {
            continue;
        }
        if (cx + RESOURCE_ICON_SIZE > x + max_width) {
            break;
        }
        image_draw(resource_get_data(r)->image.icon, cx, y, COLOR_MASK_NONE, SCALE_NONE);
        cx += RESOURCE_ICON_SIZE + 2;
    }
}

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

/* ---- Build resource buttons for export/import sections ---- */

static void build_resource_buttons(void)
{
    resource_button_count = 0;

    if (!data.route_exists) {
        return;
    }

    /* Export buttons (sell) */
    int y = 168;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (!data.can_export[r] || resource_button_count >= MAX_RESOURCE_BUTTONS) {
            continue;
        }
        generic_button *btn = &resource_buttons[resource_button_count];
        btn->x = 24;
        btn->y = y;
        btn->width = DIALOG_W / 2 - 40;
        btn->height = RESOURCE_ROW_H;
        btn->left_click_handler = button_toggle_resource;
        btn->parameter1 = r;
        btn->parameter2 = 0; /* 0 = export */
        resource_button_count++;
        y += RESOURCE_ROW_H;
    }

    /* Import buttons (buy) */
    y = 168;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (!data.can_import[r] || resource_button_count >= MAX_RESOURCE_BUTTONS) {
            continue;
        }
        generic_button *btn = &resource_buttons[resource_button_count];
        btn->x = DIALOG_W / 2 + 8;
        btn->y = y;
        btn->width = DIALOG_W / 2 - 40;
        btn->height = RESOURCE_ROW_H;
        btn->left_click_handler = button_toggle_resource;
        btn->parameter1 = r;
        btn->parameter2 = 1; /* 1 = import */
        resource_button_count++;
        y += RESOURCE_ROW_H;
    }
}

/* ---- Window callbacks ---- */

static void draw_background(void)
{
    window_draw_underlying_window();

    /* Refresh state each frame to catch host updates */
    load_route_state();
    load_remote_trade_view();
    build_resource_buttons();
}

static void draw_foreground(void)
{
    graphics_in_dialog();

    outer_panel_draw(DIALOG_X, DIALOG_Y, DIALOG_W_BLOCKS, DIALOG_H_BLOCKS);

    int cx = DIALOG_X;
    int cy = DIALOG_Y;

    /* ---- Title: "Trade with PlayerName" ---- */
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

    /* Online/Offline indicator */
    {
        translation_key status_key = data.remote_online ? TR_MP_EMPIRE_ONLINE : TR_MP_EMPIRE_OFFLINE;
        font_t status_font = data.remote_online ? FONT_NORMAL_GREEN : FONT_NORMAL_RED;
        text_draw_centered(translation_for(status_key), cx, cy + 36, DIALOG_W, status_font, 0);
    }

    /* ---- Route status ---- */
    int status_y = cy + 56;
    if (data.route_exists) {
        int sw = text_draw(translation_for(TR_MP_P2P_TRADE_STATUS),
            cx + 16, status_y, FONT_NORMAL_BLACK, 0);
        text_draw(route_state_text(data.route_state),
            cx + 16 + sw, status_y, route_state_font(data.route_state), 0);

        /* Enable/Disable button */
        int btn_x = cx + DIALOG_W - 200;
        if (data.route_state == MP_ROUTE_STATE_ACTIVE) {
            button_border_draw(btn_x, status_y - 4, 80, 22, data.focus_button_id == 10);
            text_draw_centered(translation_for(TR_MP_P2P_TRADE_DISABLE_ROUTE),
                btn_x, status_y, 80, FONT_NORMAL_RED, 0);
        } else if (data.route_state == MP_ROUTE_STATE_DISABLED) {
            button_border_draw(btn_x, status_y - 4, 80, 22, data.focus_button_id == 10);
            text_draw_centered(translation_for(TR_MP_P2P_TRADE_ENABLE_ROUTE),
                btn_x, status_y, 80, FONT_NORMAL_GREEN, 0);
        }

        /* Delete button */
        int del_x = cx + DIALOG_W - 110;
        button_border_draw(del_x, status_y - 4, 90, 22, data.focus_button_id == 11);
        text_draw_centered(translation_for(TR_MP_P2P_TRADE_DELETE_ROUTE),
            del_x, status_y, 90, FONT_NORMAL_RED, 0);
    } else {
        text_draw_centered(translation_for(TR_MP_P2P_TRADE_NO_ROUTE),
            cx, status_y, DIALOG_W, FONT_NORMAL_BROWN, 0);
    }

    /* ---- Divider line ---- */
    int div_y = cy + 76;
    graphics_draw_line(cx + 16, cx + DIALOG_W - 16, div_y, div_y, COLOR_BLACK);

    /* ---- Their capabilities ---- */
    int cap_y = div_y + 6;
    text_draw(translation_for(TR_MP_P2P_TRADE_THEIR_EXPORTS),
        cx + 16, cap_y, FONT_NORMAL_BLACK, 0);
    draw_resource_icons(cx + 130, cap_y, data.remote_exportable, DIALOG_W / 2 - 140);

    text_draw(translation_for(TR_MP_P2P_TRADE_THEIR_IMPORTS),
        cx + DIALOG_W / 2 + 8, cap_y, FONT_NORMAL_BLACK, 0);
    draw_resource_icons(cx + DIALOG_W / 2 + 120, cap_y, data.remote_importable, DIALOG_W / 2 - 140);

    /* ---- Divider ---- */
    int div2_y = cap_y + 28;
    graphics_draw_line(cx + 16, cx + DIALOG_W - 16, div2_y, div2_y, COLOR_BLACK);

    /* ---- Resource configuration (only if route exists) ---- */
    if (data.route_exists && data.route_state != MP_ROUTE_STATE_INACTIVE) {
        int section_y = div2_y + 6;

        /* Column headers */
        text_draw(translation_for(TR_MP_P2P_TRADE_EXPORTS),
            cx + 16, section_y, FONT_NORMAL_BLACK, 0);
        text_draw(translation_for(TR_MP_P2P_TRADE_IMPORTS),
            cx + DIALOG_W / 2 + 8, section_y, FONT_NORMAL_BLACK, 0);

        /* Export resources (left column) */
        int ey = section_y + 20;
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            if (!data.can_export[r]) {
                continue;
            }
            image_draw(resource_get_data(r)->image.icon, cx + 24, ey, COLOR_MASK_NONE, SCALE_NONE);
            text_draw(resource_get_data(r)->text, cx + 48, ey + 3, FONT_NORMAL_BLACK, 0);

            /* Limit display */
            int limit = data.export_limit[r];
            if (limit > 0) {
                char lbuf[16];
                snprintf(lbuf, sizeof(lbuf), "%d", limit);
                uint8_t ltext[16];
                string_copy(string_from_ascii(lbuf), ltext, 16);
                text_draw(ltext, cx + 180, ey + 3, FONT_NORMAL_GREEN, 0);
            } else {
                text_draw(translation_for(TR_MP_P2P_TRADE_UNLIMITED),
                    cx + 180, ey + 3, FONT_NORMAL_BROWN, 0);
            }
            ey += RESOURCE_ROW_H;
        }

        /* Import resources (right column) */
        int iy = section_y + 20;
        int half = DIALOG_W / 2;
        for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
            if (!data.can_import[r]) {
                continue;
            }
            image_draw(resource_get_data(r)->image.icon,
                cx + half + 16, iy, COLOR_MASK_NONE, SCALE_NONE);
            text_draw(resource_get_data(r)->text,
                cx + half + 40, iy + 3, FONT_NORMAL_BLACK, 0);

            int limit = data.import_limit[r];
            if (limit > 0) {
                char lbuf[16];
                snprintf(lbuf, sizeof(lbuf), "%d", limit);
                uint8_t ltext[16];
                string_copy(string_from_ascii(lbuf), ltext, 16);
                text_draw(ltext, cx + half + 170, iy + 3, FONT_NORMAL_GREEN, 0);
            } else {
                text_draw(translation_for(TR_MP_P2P_TRADE_UNLIMITED),
                    cx + half + 170, iy + 3, FONT_NORMAL_BROWN, 0);
            }
            iy += RESOURCE_ROW_H;
        }
    }

    /* ---- Bottom: Create route button (if no route) ---- */
    if (!data.route_exists) {
        int btn_y = cy + DIALOG_H - 56;

        if (data.remote_online && data.local_city_id >= 0) {
            /* Create route button */
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

    if (!data.route_exists) {
        /* Create route button */
        if (data.remote_online && data.local_city_id >= 0) {
            int btn_x = cx + (DIALOG_W - 200) / 2;
            int btn_y = cy + DIALOG_H - 56;

            if (m_dialog->x >= btn_x && m_dialog->x < btn_x + 200 &&
                m_dialog->y >= btn_y && m_dialog->y < btn_y + 26) {
                data.focus_button_id = 1;
                if (m_dialog->left.went_up) {
                    button_create_route(0, 0);
                    return;
                }
            }

            /* Sea/Land toggle */
            int tog_x = btn_x + 210;
            if (m_dialog->x >= tog_x && m_dialog->x < tog_x + 60 &&
                m_dialog->y >= btn_y && m_dialog->y < btn_y + 26) {
                data.focus_button_id = 2;
                if (m_dialog->left.went_up) {
                    button_toggle_sea(0, 0);
                    return;
                }
            }
        }
    } else {
        /* Enable/Disable and Delete buttons */
        int status_y = cy + 56;

        int btn_x = cx + DIALOG_W - 200;
        if (m_dialog->x >= btn_x && m_dialog->x < btn_x + 80 &&
            m_dialog->y >= status_y - 4 && m_dialog->y < status_y + 18) {
            data.focus_button_id = 10;
            if (m_dialog->left.went_up) {
                button_toggle_route(0, 0);
                return;
            }
        }

        int del_x = cx + DIALOG_W - 110;
        if (m_dialog->x >= del_x && m_dialog->x < del_x + 90 &&
            m_dialog->y >= status_y - 4 && m_dialog->y < status_y + 18) {
            data.focus_button_id = 11;
            if (m_dialog->left.went_up) {
                button_delete_route(0, 0);
                return;
            }
        }

        /* Resource row clicks (toggle export/import limits) */
        for (int i = 0; i < resource_button_count; i++) {
            generic_button *btn = &resource_buttons[i];
            if (m_dialog->x >= cx + btn->x && m_dialog->x < cx + btn->x + btn->width &&
                m_dialog->y >= cy + btn->y && m_dialog->y < cy + btn->y + btn->height) {
                if (m_dialog->left.went_up) {
                    button_toggle_resource(btn);
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

static void button_create_route(int param1, int param2)
{
    if (data.local_city_id < 0 || !data.remote_online) {
        return;
    }

    mp_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = MP_CMD_CREATE_TRADE_ROUTE;
    cmd.player_id = net_session_get_local_player_id();
    cmd.data.create_route.origin_city_id = data.local_city_id;
    cmd.data.create_route.dest_city_id = data.remote_city_id;
    cmd.data.create_route.prefer_sea = data.prefer_sea;
    mp_command_bus_submit(&cmd);
    window_invalidate();
}

static void button_delete_route(int param1, int param2)
{
    if (!data.route_exists || data.route_id < 0) {
        return;
    }

    mp_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = MP_CMD_DELETE_TRADE_ROUTE;
    cmd.player_id = net_session_get_local_player_id();
    cmd.data.delete_route.route_id = data.route_id;
    cmd.data.delete_route.network_route_id = data.network_route_id;
    mp_command_bus_submit(&cmd);
    window_invalidate();
}

static void button_toggle_route(int param1, int param2)
{
    if (!data.route_exists || data.route_id < 0) {
        return;
    }

    mp_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.player_id = net_session_get_local_player_id();

    if (data.route_state == MP_ROUTE_STATE_ACTIVE) {
        cmd.command_type = MP_CMD_DISABLE_TRADE_ROUTE;
        cmd.data.disable_route.route_id = data.route_id;
        cmd.data.disable_route.network_route_id = data.network_route_id;
    } else if (data.route_state == MP_ROUTE_STATE_DISABLED) {
        cmd.command_type = MP_CMD_ENABLE_TRADE_ROUTE;
        cmd.data.enable_route.route_id = data.route_id;
        cmd.data.enable_route.network_route_id = data.network_route_id;
    } else {
        return;
    }

    mp_command_bus_submit(&cmd);
    window_invalidate();
}

static void button_toggle_sea(int param1, int param2)
{
    data.prefer_sea = !data.prefer_sea;
    window_invalidate();
}

static void button_toggle_resource(const generic_button *btn)
{
    if (!data.route_exists || data.route_id < 0) {
        return;
    }

    int resource = btn->parameter1;
    int is_import = btn->parameter2;

    /* Toggle the limit: if > 0, set to 0 (unlimited). If 0, set to 1500 (15 loads). */
    int current;
    if (is_import) {
        current = data.import_limit[resource];
    } else {
        current = data.export_limit[resource];
    }

    int new_limit = (current > 0) ? 0 : 1500;

    mp_command cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.command_type = MP_CMD_SET_ROUTE_LIMIT;
    cmd.player_id = net_session_get_local_player_id();
    cmd.data.route_limit.route_id = data.route_id;
    cmd.data.route_limit.network_route_id = data.network_route_id;
    cmd.data.route_limit.resource = resource;
    cmd.data.route_limit.is_buying = is_import;
    cmd.data.route_limit.amount = new_limit;
    mp_command_bus_submit(&cmd);

    /* Optimistic update */
    if (is_import) {
        data.import_limit[resource] = new_limit;
    } else {
        data.export_limit[resource] = new_limit;
    }
    window_invalidate();
}

static void button_increase_limit(const generic_button *btn)
{
    /* Reserved for future arrow button usage */
}

static void button_decrease_limit(const generic_button *btn)
{
    /* Reserved for future arrow button usage */
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
