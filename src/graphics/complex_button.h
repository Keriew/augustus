#ifndef GRAPHICS_COMPLEX_BUTTON_H
#define GRAPHICS_COMPLEX_BUTTON_H

#include "graphics/tooltip.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "input/mouse.h"

#define MAX_COMPLEX_BUTTON_PARAMETERS 10 // arbitrary 

typedef enum {
    COMPLEX_BUTTON_STYLE_DEFAULT,  // Basic style: single rectangle with red border and texture fill
    COMPLEX_BUTTON_STYLE_GRAY,     // main-menu-like style
} complex_button_style;

typedef enum {
    SEQUENCE_POSITION_TOP_LEFT = 1,      /*         ┌───┬───┬───┐         */
    SEQUENCE_POSITION_TOP_CENTER = 2,    /*         │ 1 │ 2 │ 3 │         */
    SEQUENCE_POSITION_TOP_RIGHT = 3,     /*         ├───┼───┼───┤         */
    SEQUENCE_POSITION_CENTER_LEFT = 4,   /*         │ 4 │ 5 │ 6 │         */
    SEQUENCE_POSITION_CENTER = 5,        /*         ├───┼───┼───┤         */
    SEQUENCE_POSITION_CENTER_RIGHT = 6,  /*         │ 7 │ 8 │ 9 │         */
    SEQUENCE_POSITION_BOTTOM_LEFT = 7,   /*         └───┴───┴───┘         */
    SEQUENCE_POSITION_BOTTOM_CENTER = 8, /*    mirroring phone keypad     */
    SEQUENCE_POSITION_BOTTOM_RIGHT = 9,  /*  OOB values will be centered  */
} sequence_positioning;

typedef struct complex_button {
    short x;
    short y;
    short width;
    short height;
    short is_focused;
    short is_clicked;
    short is_active; // persists toggle/selected/checked/expanded state
    short visible;
    short enabled; // 0 = ignore input, draw as disabled
    short state; //special parameter for button state, enabling custom behaviours
    void (*left_click_handler)(const struct complex_button *button);
    void (*right_click_handler)(const struct complex_button *button);
    void (*hover_handler)(const struct complex_button *button);
    tooltip_context tooltip_c;
    const lang_fragment *sequence; //sequence of text to draw on button
    sequence_positioning sequence_position; //how to position the text sequence within the button
    int sequence_size; //how many fragments in sequence
    int parameters[MAX_COMPLEX_BUTTON_PARAMETERS];
    int image_before; //id-based image to draw before text
    int image_after;
    complex_button_style style; //style enum value
    short expanded_hitbox_radius; //if >0 hitbox is around the button by this many pixels - for inprecise mouse control
} complex_button;

void complex_button_draw(const complex_button *button);
void complex_button_array_draw(const complex_button *buttons, unsigned int num_buttons);
int complex_button_handle_mouse(const mouse *m, complex_button *btn);
int complex_button_array_handle_mouse(const mouse *m, complex_button *buttons, unsigned int num_buttons);

#endif // GRAPHICS_COMPLEX_BUTTON_H
