#ifndef WIDGET_CYCLING_BUTTON_H
#define WIDGET_CYCLING_BUTTON_H

#include "graphics/color.h"
#include "graphics/font.h"
#include "graphics/lang_text.h"
#include "graphics/tooltip.h"
#include "input/mouse.h"

#define MAX_CYCLE_BUTTON_STATES 10 // arbitrary

typedef enum {
    CYCLING_BUTTON_STYLE_DEFAULT,            // Basic: white/red border, default plain background fill
    CYCLING_BUTTON_STYLE_GRAY,               // main-menu-like style
    CYCLING_BUTTON_STYLE_RAW,                // No border, no fill. Content-only.
} cycling_button_style;

typedef struct cycling_button_state {
    // UI standard properties
    lang_sequence sequence;
    color_t color_mask;
    font_t font;
    tooltip_context tooltip_c;

    // other properties
    int image_before;
    int image_after;
} cycling_button_state;

typedef struct cycling_button {
    // dimensions
    short x;
    short y;
    short width;
    short height;

    // UI standard properties
    cycling_button_style style;

    // user flags
    short fill_bg; // 1 = fill background, 0 = transparent

    // function pointers
    void (*left_click_handler)(struct cycling_button *button);
    void (*right_click_handler)(struct cycling_button *button);
    void (*hover_handler)(struct cycling_button *button);

    // other properties
    cycling_button_state states[MAX_CYCLE_BUTTON_STATES];
    int state_index;
    int state_count; // =< MAX_CYCLE_BUTTON_STATES

    // cache and state properties
    short is_hovered;
    short is_ellipsized;          // 1 = text was ellipsized on last draw, 0 = full text shown
} cycling_button;

void cycling_button_draw(const cycling_button *button);
void cycling_button_draw_array(const cycling_button *buttons, unsigned int num_buttons);

int cycling_button_handle_mouse(cycling_button *btn, const mouse *m);
int cycling_button_handle_mouse_array(cycling_button *buttons, const mouse *m, unsigned int num_buttons);

int cycling_button_handle_tooltip(const cycling_button *button, tooltip_context *c);
int cycling_button_handle_tooltip_array(const cycling_button *buttons, tooltip_context *c, unsigned int num_buttons);

#endif // WIDGET_CYCLING_BUTTON_H
