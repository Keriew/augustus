#include "core/log.h"
#include "graphics/graphics.h"
#include "graphics/screen.h"
#include "graphics/text.h"

#include "SDL.h"

#include <stdio.h>
#include <string.h>

#define MSG_SIZE 1000

static char log_buffer[MSG_SIZE];

static log_history_index = 0;
static char log_history_buf[40][256];

static const char *build_message(const char *msg, const char *param_str, int param_int)
{
    int index = 0;
    index += snprintf(&log_buffer[index], MSG_SIZE - index, "%s", msg);
    if (param_str) {
        index += snprintf(&log_buffer[index], MSG_SIZE - index, "  %s", param_str);
    }
    if (param_int) {
        index += snprintf(&log_buffer[index], MSG_SIZE - index, "  %d", param_int);
    }

    strncpy(log_history_buf[log_history_index], log_buffer, 256);
    log_history_index++;
    log_history_index %= 40;

    return log_buffer;
}

void log_info(const char *msg, const char *param_str, int param_int)
{
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", build_message(msg, param_str, param_int));
}

void log_error(const char *msg, const char *param_str, int param_int)
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", build_message(msg, param_str, param_int));
}

void log_window_init() {
    int i = 0;
    for (; i < 40; i++)
        memset(log_history_buf[i], 0, 256);
}
void log_window_draw() {
    int i = 0;
    int y_offset = 0;
    graphics_fill_rect(0, 0, screen_width(), 40 * 12, 0x800055ff);

    for (; i < 40; i++) {
        text_draw(log_history_buf[(i + log_history_index)%40], 5, y_offset, FONT_NORMAL_PLAIN, COLOR_FONT_LIGHT_GRAY);
        y_offset += 12;
    }
}
