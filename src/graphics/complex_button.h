#ifndef GRAPHICS_COMPLEX_BUTTON_H
#define GRAPHICS_COMPLEX_BUTTON_H

#include "graphics/tooltip.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/text.h"
#include "input/mouse.h"
#include "widget/text_block.h"

#define MAX_COMPLEX_BUTTON_PARAMETERS 10 // arbitrary 
#define MAX_CYCLE_BUTTON_STATES 10 // arbitrary

typedef enum {
    COMPLEX_BUTTON_STYLE_DEFAULT,          // Basic: white/red border, default plain background fill
    COMPLEX_BUTTON_STYLE_DEFAULT_SMALL,    // like default but small font and less padding
    COMPLEX_BUTTON_STYLE_NO_FILL,          // No fill background, only border
    COMPLEX_BUTTON_STYLE_GRAY,             // main-menu-like style
    COMPLEX_BUTTON_STYLE_GRAY_NO_FILL,     // mainmenu border, but no fill background
    COMPLEX_BUTTON_STYLE_BROWN,            // Inner panel brown fill, white border, brown text
    COMPLEX_BUTTON_STYLE_RAW,              // No border, no fill. Content-only.
    COMPLEX_BUTTON_STYLE_CUSTOM            // custom style - bypasses the default selection of colors/fonts
} complex_button_style;

typedef enum {
    CYCLING_BUTTON_STYLE_DEFAULT,            // Basic: white/red border, default plain background fill
    CYCLING_BUTTON_STYLE_DEFAULT_SMALL,      // like default but small font and less padding
    CYCLING_BUTTON_STYLE_NO_FILL,            // No fill background, only border
    CYCLING_BUTTON_STYLE_GRAY,               // main-menu-like style
    CYCLING_BUTTON_STYLE_RAW,                // No border, no fill. Content-only.
    CYCLING_BUTTON_STYLE_GRAY_NO_FILL,       // mainmenu border, but no fill background
} cycling_button_style;

typedef struct btn_img {
    int id;
    unsigned char auto_center;
    int image_x_offset;
    int image_y_offset;
} btn_img;

typedef struct complex_button {
    short x;
    short y;
    short width;
    short height;
    unsigned char is_focused;             // bad wording - is_hovered would be more accurate
    unsigned char is_clicked;
    unsigned char is_active;              // persists toggle/selected/checked/expanded state
    unsigned char is_hidden;              // 1 = hidden, 0 = visible
    unsigned char is_disabled;            // 1 = disabled, 0 = enabled
    unsigned char state;                  // special parameter for custom behaviours
    unsigned char is_ellipsized;          // 1 = text was ellipsized on last draw, 0 = full text shown
    void (*left_click_handler)(struct complex_button *button);
    void (*right_click_handler)(struct complex_button *button);
    void (*hover_handler)(struct complex_button *button); // not const - hover fnc needs to modify properties
    tooltip_context tooltip_c;
    const lang_fragment *sequence;     // sequence of text to draw on button
    sequence_positioning sequence_position;
    unsigned short sequence_size;
    int parameters[MAX_COMPLEX_BUTTON_PARAMETERS];
    int image_before; //img id
    int image_after; //img id
    btn_img image; // if specified, will be drawn INSTEAD of text
    unsigned char flush_with_background; // if set, bottom border is not drawn
    unsigned char shade_on_hover; // 0-7, if set, button is graphics_shade_rect with this value
    unsigned char dont_enlarge_font; // if set, the fontsize override to large wont be applied
    color_t color_mask; // not font mask - background mask. If set, overrides the style
    font_t font; // if set, overrides the style properties
    color_t font_color; // if set, overrides the style properties
    complex_button_style style;
    unsigned char expanded_hitbox_radius; //not yet fully implemented 
    void *user_data; // custom user data pointer, e.g. can point to a parent struct
} complex_button;

typedef struct checkbox_button {
    short x;
    short y;
    short width;
    short height;
    short is_hovered;
    short is_checked;
    short fill_bg; // 1 = fill background, 0 = transparent
    void (*left_click_handler)(struct checkbox_button *button);
    void (*hover_handler)(struct checkbox_button *button);
    tooltip_context tooltip_c;
    font_t font; // font of the text next to the checkbox, the checkbox font is fixed
    short box_on_right; // box on right side of text/image instead of left
    const lang_fragment *sequence;     // sequence of text to draw on button
    int image_before; // optional image to draw before the text
    int image_after;  // optional image to draw after the text
    int sequence_size;
    color_t color_mask;
    short is_ellipsized;          // 1 = text was ellipsized on last draw, 0 = full text shown
} checkbox_button;

typedef struct cycling_button_state {
    const lang_fragment *sequence;
    int sequence_size;
    int image_before;
    int image_after;
    color_t color_mask;
    font_t font;
    tooltip_context tooltip_c;
} cycling_button_state;

typedef struct cycling_button {
    short x;
    short y;
    short width;
    short height;
    short is_hovered;
    short fill_bg; // 1 = fill background, 0 = transparent
    cycling_button_style style;
    void (*left_click_handler)(struct cycling_button *button);
    void (*right_click_handler)(struct cycling_button *button);
    void (*hover_handler)(struct cycling_button *button);

    cycling_button_state states[MAX_CYCLE_BUTTON_STATES];
    int state_index;
    int state_count; // =< MAX_CYCLE_BUTTON_STATES
    short is_ellipsized;          // 1 = text was ellipsized on last draw, 0 = full text shown
} cycling_button;

color_t complex_button_basic_colors(int id);
font_t complex_button_font_for_style(complex_button_style style);
color_t complex_button_mask_for_style(complex_button_style style);

// Complex Buttons
// drawing
void complex_button_draw(const complex_button *button);
void complex_button_draw_array(const complex_button *buttons, unsigned int num_buttons);
// input
int complex_button_handle_mouse(complex_button *btn, const mouse *m);
int complex_button_handle_mouse_array(complex_button *buttons, const mouse *m, unsigned int num_buttons);
// tooltip
int complex_button_handle_tooltip(const complex_button *button, tooltip_context *c);
int complex_button_handle_tooltip_array(const complex_button *buttons, tooltip_context *c, unsigned int num_buttons);

// Checkbox Buttons
// drawing
void checkbox_button_draw(const checkbox_button *button);
void checkbox_button_draw_array(const checkbox_button *buttons, unsigned int num_buttons);
// input
int checkbox_button_handle_mouse(checkbox_button *btn, const mouse *m);
int checkbox_button_handle_mouse_array(checkbox_button *buttons, const mouse *m, unsigned int num_buttons);
// tooltip
int checkbox_button_handle_tooltip(const checkbox_button *button, tooltip_context *c);
int checkbox_button_handle_tooltip_array(const checkbox_button *buttons, tooltip_context *c, unsigned int num_buttons);

// Cycling Buttons
// drawing
void cycling_button_draw(const cycling_button *button);
void cycling_button_draw_array(const cycling_button *buttons, unsigned int num_buttons);
// input
int cycling_button_handle_mouse(cycling_button *btn, const mouse *m);
int cycling_button_handle_mouse_array(cycling_button *buttons, const mouse *m, unsigned int num_buttons);
// tooltip
int cycling_button_handle_tooltip(const cycling_button *button, tooltip_context *c);
int cycling_button_handle_tooltip_array(const cycling_button *buttons, tooltip_context *c, unsigned int num_buttons);


#endif // GRAPHICS_COMPLEX_BUTTON_H
