#ifndef WINDOW_MESSAGE_DIALOG_LAYOUT_H
#define WINDOW_MESSAGE_DIALOG_LAYOUT_H

#include "graphics/font.h"

#include <stdint.h>

typedef struct {
    int width;
    int height;
    int title_x;
    int title_y;
    int title_width;
    int title_height;
    int message_x;
    int message_y;
    int message_width;
    int message_height;
    int message_lines;
} message_dialog_layout;

void message_dialog_layout_calculate(message_dialog_layout *layout,
    const uint8_t *title, const uint8_t *message, int preferred_width,
    int minimum_width, int minimum_height, int footer_height);

#endif // WINDOW_MESSAGE_DIALOG_LAYOUT_H
