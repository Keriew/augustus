#ifndef WIDGET_TEXT_BLOCK_H
#define WIDGET_TEXT_BLOCK_H

#include "graphics/lang_sequence.h"
#include "graphics/tooltip.h"
#include "input/mouse.h"

#include <stdint.h>

typedef enum {
    SEQUENCE_POSITION_TOP_LEFT = 1,      /*         ┌───┬───┬───┐         */
    SEQUENCE_POSITION_TOP_CENTER = 2,    /*         │ 1 │ 2 │ 3 │         */
    SEQUENCE_POSITION_TOP_RIGHT = 3,     /*         ├───┼───┼───┤         */
    SEQUENCE_POSITION_CENTER_LEFT = 4,   /*         │ 4 │ 5 │ 6 │         */
    SEQUENCE_POSITION_CENTER = 5,        /*         ├───┼───┼───┤         */
    SEQUENCE_POSITION_CENTER_RIGHT = 6,  /*         │ 7 │ 8 │ 9 │         */
    SEQUENCE_POSITION_BOTTOM_LEFT = 7,   /*         └───┴───┴───┘         */
    SEQUENCE_POSITION_BOTTOM_CENTER = 8, /*    just like phone keypad     */
    SEQUENCE_POSITION_BOTTOM_RIGHT = 9,  /*  OOB values will be centered  */
} sequence_positioning;

typedef struct text_block {
    lang_sequence sequence; // text fragments to display in the box
    sequence_positioning position; // where to position the text inside the block
    font_t font; // font to use for the text, defaults to FONT_NORMAL_BLACK if not set
    color_t font_primary; // primary color for the text, defaults to COLOR_MASK_NONE if not set
    int x;
    int y;
    int width;
    int height;
    int inner_padding_x; // defaults to 2px
    int inner_padding_y; // defaults to 2px
    uint8_t *raw_text; // optional raw text if you dont want to deal with lang_fragment
    tooltip_context tooltip_c; // optional tooltip context for the text block
    unsigned short draw_border;
    unsigned short draw_background;
    unsigned short is_disabled; // uninteractable, grayed out
    unsigned short is_hidden; // disabled and invisible, does not handle mouse events at all
    // cache and state properties - do not set externally, managed by the text_block's own module
    unsigned short state_is_hovered; // mouse is in bounds of the text block
} text_block;


int widget_text_block_init_simple(text_block *block, int x, int y, int width, int height,
     const lang_sequence *sequence, sequence_positioning position);
void text_block_draw(const text_block *block);
int text_block_handle_mouse(text_block *block, const mouse *m);
int text_block_handle_tooltip(const text_block *block, tooltip_context *c);

#endif // WIDGET_TEXT_BLOCK_H
