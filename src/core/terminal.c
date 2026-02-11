#include "core/terminal.h"

#include "core/log.h"
#include "graphics/color.h"
#include "graphics/graphics.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "scenario/lua/lua_state.h"

#include "lua/lua.h"
#include "lua/lauxlib.h"

#include <stdio.h>
#include <string.h>

#define MAX_LINES 200
#define MAX_LINE_LENGTH 200
#define MAX_INPUT_LENGTH 512
#define MAX_HISTORY 50
#define LINE_HEIGHT 13
#define INPUT_HEIGHT 16
#define PADDING 4
#define SEPARATOR_HEIGHT 2
#define BG_COLOR 0xCC000000
#define INPUT_BG_COLOR 0xFF1A1A1A
#define COLOR_STDOUT 0xFFB3B3B3
#define COLOR_INPUT 0xFFCC9955
#define COLOR_RETURN 0xFF77BBDD

static struct {
    char lines[MAX_LINES][MAX_LINE_LENGTH];
    int write_index;
    int total_lines;
    int scroll_offset;
    int active;
    // Input line
    char input[MAX_INPUT_LENGTH];
    int input_len;
    int cursor_pos;
    // Command history
    char history[MAX_HISTORY][MAX_INPUT_LENGTH];
    int history_count;
    int history_write;
    int history_browse;  // -1 = not browsing
    char saved_input[MAX_INPUT_LENGTH];
    int saved_input_len;
} data;

void terminal_init(void)
{
    memset(&data, 0, sizeof(data));
    data.history_browse = -1;
}

void terminal_add_line(const char *text)
{
    if (!text) {
        return;
    }
    strncpy(data.lines[data.write_index], text, MAX_LINE_LENGTH - 1);
    data.lines[data.write_index][MAX_LINE_LENGTH - 1] = '\0';
    data.write_index = (data.write_index + 1) % MAX_LINES;
    if (data.total_lines < MAX_LINES) {
        data.total_lines++;
    }
    data.scroll_offset = 0;
}

static void execute_lua(const char *code)
{
    char echo[MAX_LINE_LENGTH];
    snprintf(echo, MAX_LINE_LENGTH, "> %s", code);
    terminal_add_line(echo);

    if (!scenario_lua_is_active()) {
        terminal_add_line("[error] No Lua state active (load a scenario with a .lua script)");
        return;
    }

    lua_State *L = scenario_lua_get_state();
    if (!L) {
        terminal_add_line("[error] Lua state is NULL");
        return;
    }

    int top = lua_gettop(L);

    // Try "return <code>" first so expressions show their value
    char return_buf[MAX_INPUT_LENGTH + 8];
    snprintf(return_buf, sizeof(return_buf), "return %s", code);
    int result = luaL_loadstring(L, return_buf);
    if (result != LUA_OK) {
        lua_pop(L, 1);
        result = luaL_dostring(L, code);
    } else {
        result = lua_pcall(L, 0, LUA_MULTRET, 0);
    }

    if (result != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        if (err) {
            char msg[MAX_LINE_LENGTH];
            snprintf(msg, MAX_LINE_LENGTH, "[error] %s", err);
            terminal_add_line(msg);
        }
        lua_settop(L, top);
    } else {
        int new_top = lua_gettop(L);
        for (int i = top + 1; i <= new_top; i++) {
            const char *val = luaL_tolstring(L, i, NULL);
            if (val) {
                char msg[MAX_LINE_LENGTH];
                snprintf(msg, MAX_LINE_LENGTH, "= %s", val);
                terminal_add_line(msg);
            }
            lua_pop(L, 1);
        }
        lua_settop(L, top);
    }
}

void terminal_submit(void)
{
    if (data.input_len == 0) {
        return;
    }
    // Save to history
    strncpy(data.history[data.history_write], data.input, MAX_INPUT_LENGTH - 1);
    data.history[data.history_write][MAX_INPUT_LENGTH - 1] = '\0';
    data.history_write = (data.history_write + 1) % MAX_HISTORY;
    if (data.history_count < MAX_HISTORY) {
        data.history_count++;
    }
    data.history_browse = -1;

    execute_lua(data.input);
    data.input[0] = '\0';
    data.input_len = 0;
    data.cursor_pos = 0;
}

void terminal_toggle(void)
{
    data.active = !data.active;
    if (data.active) {
        data.scroll_offset = 0;
    }
    window_invalidate();
}

int terminal_is_active(void)
{
    return data.active;
}

void terminal_scroll_up(void)
{
    int height = screen_height() * 2 / 5;
    int visible_lines = (height - PADDING * 2 - SEPARATOR_HEIGHT - INPUT_HEIGHT) / LINE_HEIGHT;
    int max_scroll = data.total_lines - visible_lines;
    if (max_scroll < 0) {
        max_scroll = 0;
    }
    data.scroll_offset += 5;
    if (data.scroll_offset > max_scroll) {
        data.scroll_offset = max_scroll;
    }
    window_invalidate();
}

void terminal_scroll_down(void)
{
    data.scroll_offset -= 5;
    if (data.scroll_offset < 0) {
        data.scroll_offset = 0;
    }
    window_invalidate();
}

// Scancode constants matching SDL_SCANCODE values
#define SCAN_BACKSPACE 0x2A
#define SCAN_DELETE 0x4C
#define SCAN_RIGHT 0x4F
#define SCAN_LEFT 0x50
#define SCAN_UP 0x52
#define SCAN_DOWN 0x51
#define SCAN_HOME 0x4A
#define SCAN_END 0x4D

static void set_input(const char *text)
{
    strncpy(data.input, text, MAX_INPUT_LENGTH - 1);
    data.input[MAX_INPUT_LENGTH - 1] = '\0';
    data.input_len = (int)strlen(data.input);
    data.cursor_pos = data.input_len;
}

void terminal_handle_key_down(int scancode, int sym, int mod)
{
    (void)sym;
    (void)mod;
    switch (scancode) {
        case SCAN_BACKSPACE:
            if (data.cursor_pos > 0) {
                data.cursor_pos--;
                memmove(&data.input[data.cursor_pos], &data.input[data.cursor_pos + 1],
                    data.input_len - data.cursor_pos);
                data.input_len--;
            }
            break;
        case SCAN_DELETE:
            if (data.cursor_pos < data.input_len) {
                memmove(&data.input[data.cursor_pos], &data.input[data.cursor_pos + 1],
                    data.input_len - data.cursor_pos);
                data.input_len--;
            }
            break;
        case SCAN_RIGHT:
            if (data.cursor_pos < data.input_len) {
                data.cursor_pos++;
            }
            break;
        case SCAN_LEFT:
            if (data.cursor_pos > 0) {
                data.cursor_pos--;
            }
            break;
        case SCAN_UP:
            if (data.history_count > 0) {
                if (data.history_browse == -1) {
                    // Save current input before browsing
                    strncpy(data.saved_input, data.input, MAX_INPUT_LENGTH - 1);
                    data.saved_input[MAX_INPUT_LENGTH - 1] = '\0';
                    data.saved_input_len = data.input_len;
                    data.history_browse = 0;
                } else if (data.history_browse < data.history_count - 1) {
                    data.history_browse++;
                }
                int idx = (data.history_write - 1 - data.history_browse + MAX_HISTORY) % MAX_HISTORY;
                set_input(data.history[idx]);
            }
            break;
        case SCAN_DOWN:
            if (data.history_browse >= 0) {
                data.history_browse--;
                if (data.history_browse < 0) {
                    // Restore saved input
                    set_input(data.saved_input);
                    data.history_browse = -1;
                } else {
                    int idx = (data.history_write - 1 - data.history_browse + MAX_HISTORY) % MAX_HISTORY;
                    set_input(data.history[idx]);
                }
            }
            break;
        case SCAN_HOME:
            data.cursor_pos = 0;
            break;
        case SCAN_END:
            data.cursor_pos = data.input_len;
            break;
        default:
            break;
    }
}

void terminal_handle_text(const char *text_utf8)
{
    if (!text_utf8) {
        return;
    }
    int len = (int)strlen(text_utf8);
    if (data.input_len + len >= MAX_INPUT_LENGTH - 1) {
        return;
    }
    memmove(&data.input[data.cursor_pos + len], &data.input[data.cursor_pos],
        data.input_len - data.cursor_pos + 1);
    memcpy(&data.input[data.cursor_pos], text_utf8, len);
    data.cursor_pos += len;
    data.input_len += len;
}

void terminal_draw(void)
{
    if (!data.active) {
        return;
    }

    window_request_refresh();

    int width = screen_width();
    int height = screen_height() * 2 / 5;

    // Semi-transparent dark background
    graphics_fill_rect(0, 0, width, height, BG_COLOR);

    // Input line area at the bottom of terminal
    int input_y = height - SEPARATOR_HEIGHT - INPUT_HEIGHT;
    graphics_fill_rect(0, input_y, width, INPUT_HEIGHT, INPUT_BG_COLOR);

    // Separator between log and input
    graphics_fill_rect(0, input_y, width, 1, COLOR_INPUT);

    // Bottom separator line
    graphics_fill_rect(0, height - SEPARATOR_HEIGHT, width, SEPARATOR_HEIGHT, COLOR_INPUT);

    // Draw prompt and input text
    int prompt_width = text_draw((const uint8_t *) "> ",
        PADDING, input_y + 2, FONT_NORMAL_PLAIN, COLOR_INPUT);
    int input_text_x = PADDING + prompt_width;
    if (data.input_len > 0) {
        text_draw((const uint8_t *) data.input,
            input_text_x, input_y + 2, FONT_NORMAL_PLAIN, COLOR_INPUT);
    }

    // Draw cursor
    char before_cursor[MAX_INPUT_LENGTH];
    if (data.cursor_pos > 0) {
        memcpy(before_cursor, data.input, data.cursor_pos);
    }
    before_cursor[data.cursor_pos] = '\0';
    int cursor_x = input_text_x + text_get_width((const uint8_t *) before_cursor, FONT_NORMAL_PLAIN);
    graphics_fill_rect(cursor_x, input_y + 2, 1, LINE_HEIGHT - 2, COLOR_INPUT);

    // Log lines area (above input)
    int log_bottom = input_y;
    int visible_lines = (log_bottom - PADDING) / LINE_HEIGHT;

    graphics_set_clip_rectangle(0, 0, width, log_bottom);

    for (int i = 0; i < visible_lines && i < data.total_lines; i++) {
        int line_index_from_bottom = i + data.scroll_offset;
        if (line_index_from_bottom >= data.total_lines) {
            break;
        }
        int buffer_index = (data.write_index - 1 - line_index_from_bottom + MAX_LINES) % MAX_LINES;
        int y = log_bottom - PADDING - (i + 1) * LINE_HEIGHT;
        if (y < 0) {
            break;
        }
        color_t line_color = COLOR_STDOUT;
        if (data.lines[buffer_index][0] == '>') {
            line_color = COLOR_INPUT;
        } else if (data.lines[buffer_index][0] == '=') {
            line_color = COLOR_RETURN;
        }
        text_draw((const uint8_t *) data.lines[buffer_index],
            PADDING, y, FONT_NORMAL_PLAIN, line_color);
    }

    graphics_reset_clip_rectangle();
}
