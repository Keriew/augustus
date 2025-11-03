#ifndef WIDGET_INPUT_BOX_H
#define WIDGET_INPUT_BOX_H

#include "graphics/font.h"
#include "input/mouse.h"

// Common predefined character sets
#define INPUT_BOX_CHARS_NUMERIC       "0123456789 "
#define INPUT_BOX_CHARS_ALPHA         "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz "
#define INPUT_BOX_CHARS_ALPHANUMERIC  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 "
#define INPUT_BOX_CHARS_FILENAME      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-() "
#define INPUT_BOX_CHARS_FORMULAS      "01234567890+-*/()[],{}" 
#define INPUT_BOX_CHARS_ALL_SUPPORTED "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-()!\"\'*+,./:;=?[]_ "

typedef struct {
    int x;
    int y;
    int width_blocks;
    int height_blocks;
    font_t font;
    int allow_punctuation;
    uint8_t *text;
    int text_length;
    int put_clear_button_outside_box;
    const uint8_t *placeholder;
    void (*on_change)(int is_addition_at_end);
    uint8_t *old_text;
    const char *allowed_chars;
} input_box;

/**
 * This will start text input. The `text` variable of the box will be used to capture
 * input until @link input_box_stop @endlink is called
 * @param box Input box
 */
void input_box_start(input_box *box);
void input_box_stop(input_box *box);

void input_box_refresh_text(input_box *box);
int input_box_is_accepted(void);

int input_box_handle_mouse(const mouse *m, const input_box *box);
void input_box_draw(const input_box *box);

#endif // WIDGET_INPUT_BOX_H
