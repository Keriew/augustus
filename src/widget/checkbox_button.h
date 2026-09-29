#ifndef WIDGET_CHECKBOX_BUTTON_H
#define WIDGET_CHECKBOX_BUTTON_H

#include "graphics/color.h"
#include "graphics/font.h"
#include "graphics/lang_text.h"
#include "graphics/tooltip.h"
#include "input/mouse.h"

typedef struct checkbox_button {
    // dimensions
    short x;
    short y;
    short width;
    short height;

    // UI standard properties
    lang_sequence sequence;     // sequence of text to draw on button
    font_t font; // font of the text next to the checkbox, the checkbox font is fixed
    color_t color_mask;
    tooltip_context tooltip_c;

    // user flags
    short fill_bg; // 1 = fill background, 0 = transparent
    short box_on_right; // box on right side of text/image instead of left

    // function pointers
    void (*left_click_handler)(struct checkbox_button *button);
    void (*hover_handler)(struct checkbox_button *button);

    // other properties
    int image_before; // optional image to draw before the text
    int image_after;  // optional image to draw after the text

    // cache and state properties
    short is_hovered;
    short is_checked;
    short is_ellipsized;          // 1 = text was ellipsized on last draw, 0 = full text shown
} checkbox_button;

// Main public API:
void checkbox_button_draw(const checkbox_button *button);
void checkbox_button_draw_array(const checkbox_button *buttons, unsigned int num_buttons);

int checkbox_button_handle_mouse(checkbox_button *btn, const mouse *m);
int checkbox_button_handle_mouse_array(checkbox_button *buttons, const mouse *m, unsigned int num_buttons);

int checkbox_button_handle_tooltip(const checkbox_button *button, tooltip_context *c);
int checkbox_button_handle_tooltip_array(const checkbox_button *buttons, tooltip_context *c, unsigned int num_buttons);

// helpers:

/// @brief Check if the checkbox button is currently checked
/// @param button The checkbox button to query
/// @return 1 if the button is checked, 0 otherwise
int checkbox_button_is_checked(const checkbox_button *button);

/// @brief Check the checkbox button
/// @param button The checkbox button to check
/// @return 1 if the button's state changed, 0 if it was already checked
int checkbox_button_check(checkbox_button *button);

/// @brief Uncheck the checkbox button
/// @param button The checkbox button to uncheck
/// @return 1 if the button's state changed, 0 if it was already unchecked
int checkbox_button_uncheck(checkbox_button *button);

/// @brief Toggle the checkbox button's state
/// @param button The checkbox button to toggle
/// @return 1 if the button is now checked, 0 if it is now unchecked
int checkbox_button_toggle(checkbox_button *button);

#endif // WIDGET_CHECKBOX_BUTTON_H
