#ifndef GRAPHICS_COMPLEX_BUTTON_H
#define GRAPHICS_COMPLEX_BUTTON_H

#include "core/time.h"
#include "graphics/tooltip.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/text.h"
#include "input/mouse.h"
#include "widget/text_block.h"

#define MAX_COMPLEX_BUTTON_PARAMETERS 10 // arbitrary 
#define MAX_CYCLE_BUTTON_STATES 10 // arbitrary
#define DEFAULT_ANIMATION_FRAME_DURATION 100 // milliseconds

typedef enum {
    COMPLEX_BUTTON_STYLE_DEFAULT,          // Basic: white/red border, default plain background fill
    COMPLEX_BUTTON_STYLE_SUNKEN,           // Sunken Sidebar-like style with a gray texture
    COMPLEX_BUTTON_STYLE_GRAY,             // main-menu-like style
    COMPLEX_BUTTON_STYLE_BROWN,            // Inner panel brown fill, white border, brown text
    COMPLEX_BUTTON_STYLE_RAW,              // Content-only defaults.
    COMPLEX_BUTTON_STYLE_IMAGE,            // Image-only defaults. RECOMMENDED for animated buttons.
    COMPLEX_BUTTON_STYLE_CUSTOM            // custom style - bypasses the default selection of colors/fonts
} complex_button_style;

typedef enum {
    CYCLING_BUTTON_STYLE_DEFAULT,            // Basic: white/red border, default plain background fill
    CYCLING_BUTTON_STYLE_GRAY,               // main-menu-like style
    CYCLING_BUTTON_STYLE_RAW,                // No border, no fill. Content-only.
} cycling_button_style;

typedef struct btn_img {
    int id;
    unsigned char auto_center; // 0 = draw at x,y; 1 = center in button
    int image_x_offset; // offsets are applied after auto-center
    int image_y_offset;
} btn_img;

typedef enum {
    BUTTON_ANIMATION_TRIGGER_NONE,  // always animate, regardless of hover/click state
    BUTTON_ANIMATION_TRIGGER_HOVER, // start animation on hover
    BUTTON_ANIMATION_TRIGGER_CLICK, // start animation on click
    BUTTON_ANIMATION_TRIGGER_CUSTOM // animation is started/stopped by user code 
} animation_trigger;

typedef enum {
    BUTTON_ANIMATION_ONCE = 0,    // play the animation once and stop on the last frame
    BUTTON_ANIMATION_LOOP = 1,    // play the animation from 0 to last frame, then back to 0, repeat
    BUTTON_ANIMATION_PINGPONG = 2 // play the animation from 0 to last frame, then reverse from last frame to 0, repeat
} animation_mode;

typedef struct complex_button_animation {
    btn_img *frames;                    // additional animation images - 0th frame is the button->image;
    unsigned short frame_count;         // number of frames in the supplied frames array
    unsigned short frame_duration;      // duration of each frame of animation in milliseconds
    unsigned char allow_immediate_stop; // 1 = when stopped, animation immediately returns to frame 0 and pauses.
    unsigned char max_loop_count;       // number of times the animation should loop/pingpong. 0 = infinite loop
    unsigned char skip_zero_frame;      // 1 = skip the 0th frame when looping/pingponging. 0 = include the 0th frame in the loop
    animation_trigger trigger;          // event that triggers animation start
    animation_mode loop_mode;           // how the animation loops

    // internal state variables:
    unsigned char is_reversed;          // flag for pingpong mode     
    unsigned char is_active;            // 1 = animation is  running, 0 = animation is paused/uninitialized
    unsigned char is_disabled;
    unsigned char is_looping;           // animation is set to loop infinitely, until this parameter changes.
    unsigned char is_holding;           // 1 = completed animation is holding its final frame until trigger releases
    unsigned char loops_left;           // number of loops remaining
    unsigned short current_frame;       // index of the current frame being displayed
    time_millis last_change;            // timestamp of the last frame change
} complex_button_animation;

typedef struct complex_button {
    // dimensions
    short x;
    short y;
    short width;
    short height;

    // UI standard properties
    lang_sequence sequence;     // sequence of text to draw on button
    sequence_positioning sequence_position; // where to position the text inside the block, defaults to center/center
    complex_button_style style;
    font_t font; // if set, overrides the style-set properties
    color_t font_primary; // if set, overrides the style-set properties
    color_t bg_primary; // primary color mask for background drawing
    tooltip_context tooltip_c;

    // user flags
    unsigned char draw_border;              // 1 = draw style border, 0 = no border
    unsigned char draw_hover_state;         // 1 = draw hover effects, 0 = no hover visuals
    unsigned char draw_background;          // 1 = draw style background, 0 = no fill
    unsigned char is_disabled;              // 1 = disabled, 0 = enabled
    unsigned char is_hidden;                // 1 = hidden, 0 = visible
    unsigned char flush_with_background;    // if set, bottom border is not drawn
    unsigned char shade_on_hover;           // 0-7, if set, button is graphics_shade_rect with this value
    unsigned char light_on_hover;           // 0-7, if set, button is graphics_light_up_rect with this value
    unsigned char border_on_hover;          // if set, border switches to hover state when focused
    unsigned char dont_enlarge_font;        // if set, the fontsize override to large wont be applied
    unsigned char expanded_hitbox_radius;   // not yet fully implemented 
    unsigned char has_animation;            // if set, button will animate using the embedded animation state

    // function pointers
    void (*left_click_handler)(struct complex_button *button);
    void (*right_click_handler)(struct complex_button *button);
    void (*hover_handler)(struct complex_button *button); // not const - hover fnc needs to modify properties
    void (*unclick_handler)(struct complex_button *button); // called after clicked state returns to 0 from 1. 

    // other properties
    int image_before; // img id
    int image_after; // img id
    btn_img image; // if specified, will be drawn INSTEAD of text
    int parameters[MAX_COMPLEX_BUTTON_PARAMETERS];
    void *user_data; // custom user data pointer, e.g. can point to a parent struct
    unsigned char state; // special parameter for user's custom behaviours
    complex_button_animation animation;

    // cache and state properties
    unsigned char is_hovered;             // mouse is in bounds of the button
    unsigned char is_clicked;
    unsigned char is_active;              // persists toggle/selected/checked/expanded state
    unsigned char is_ellipsized;          // 1 = text was ellipsized on last draw, 0 = full text shown
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
    lang_sequence sequence;     // sequence of text to draw on button
    int image_before; // optional image to draw before the text
    int image_after;  // optional image to draw after the text
    color_t color_mask;
    short is_ellipsized;          // 1 = text was ellipsized on last draw, 0 = full text shown
} checkbox_button;

typedef struct cycling_button_state {
    lang_sequence sequence;
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
void complex_button_init_style(complex_button *button, complex_button_style style);


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


int complex_button_animation_init(complex_button *button, const btn_img *frames, unsigned short frame_count,
     animation_trigger trigger, animation_mode mode);
void complex_button_animation_destroy(complex_button *button);
void complex_button_animation_start(complex_button *button);
void complex_button_animation_stop(complex_button *button);

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


