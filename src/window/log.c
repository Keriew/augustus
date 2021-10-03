#include "log.h"

#include "core/log.h"
#include "graphics/graphics.h"
#include "graphics/screen.h"
#include "graphics/text.h"

const unsigned int LOG_LINE_HEIGHT = 12;

void log_window_draw() {
    graphics_fill_rect(0, 0, screen_width(), log_history_capacity() * LOG_LINE_HEIGHT, COLOR_WINDOW_LOG_BACKGROUND);

    for (int i = 0; i < log_history_capacity(); i++) {
        text_draw(log_history_get_line(i), 5, i * LOG_LINE_HEIGHT, FONT_SMALL_PLAIN, COLOR_FONT_LIGHT_GRAY);
    }
}