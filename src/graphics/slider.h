#ifndef GRAPHICS_SLIDER_H
#define GRAPHICS_SLIDER_H

#include "graphics/lang_sequence.h"
#include "graphics/tooltip.h"
#include "input/mouse.h"
#include "widget/text_block.h"

#define SLIDER_PADDING 4

typedef enum slider_display_text { // ____________________________________
    SLIDER_DISPLAY_TEXT_NONE = 0,  //                  3
    SLIDER_DISPLAY_TEXT_LEFT = 1,  //                  ^
    SLIDER_DISPLAY_TEXT_RIGHT = 2, //   1  <--- |<|----O----|>| --->  2
    SLIDER_DISPLAY_TEXT_ABOVE = 3, //                  v
    SLIDER_DISPLAY_TEXT_BELOW = 4  //                  4
} slider_display_text;             //_____________________________________

typedef struct slider {
    short x;
    short y;
    short length; // scrollable dimensions length - can be either height or width depending on orientation
    int min_value;
    int max_value;
    int value_step; // value of the increment/decrement

    unsigned char is_vertical; // 1 = vertical slider, 0 = horizontal slider
    unsigned char is_disabled; // uninteractable, grayed out
    unsigned char is_hidden;  // disabled and invisible, does not handle mouse events at all
    unsigned char show_value_after_sequence; // print ": <value>" after the sequence text
    unsigned char show_percentage_after_sequence; // print ": <(value/max_value)*100>%" after the sequence text
    unsigned char show_plus_minus_buttons; // 1 = +/- buttons, 0 = arrows
    unsigned char force_mini_thumb; // 1 = force the 24x24 square thumb, 0 = scale the thumb based on the range
    unsigned char draw_background; //  1 = draw the dark background for the sliders track
    unsigned char split_tooltips; // if set, slider tooltip and text_block tooltip will be handled separately

    void *slider_variable; // variable that will be automatically updated to match the slider's value
    lang_fragment *(*convert_value_to_string)(struct slider *slider); // converts slider value into lang fragments
    void (*left_click_handler)(struct slider *slider);
    void (*right_click_handler)(struct slider *slider);
    void (*hover_handler)(struct slider *slider);
    void (*on_slide_callback)(struct slider *slider);
    text_block block; // optional text block to display next to the slider
    slider_display_text display_text; // whether to display the slider's value as text, and position of it
    tooltip_context tooltip_c;
    void *user_data;

    // state properties - do not set externally, managed by the slider's own module
    int value; // current value of the slider, will be clamped to min/max and snapped to step
    unsigned char is_hovered; // mouse is in bounds of the slider
    unsigned char is_clicked; // mouse left button was clicked in bounds of the slider
    unsigned char is_dragging; // the thumb is currently being dragged
    unsigned char hovered_element; // 0 = none, 1 = decrease, 2 = increase, 3 = thumb, 4 = track
    unsigned char hovered_element_animation_frame; // index of the animation frame
    // Cache properties - do not set externally, managed by the slider's own module
    int cached_thumb_offset; // pixel offset of the thumb from the top/left of the slider's track
    int cached_thumb_length; // pixel length of the thumb, based on the range and step size
    int cached_thumb_middle_sections; // number of middle sections to draw for the thumb, -1 for mini thumb
    int cached_drag_offset; // pixel offset mouse_current_pos - mouse_drag_start_pos in the relevant dimension
} slider_t;

// returns 1 if successfully initialised
int slider_init(slider_t *slider, int x, int y, int length, int min_value, int max_value,
    int value_step, int initial_value, unsigned char is_vertical, slider_display_text display_text);
int slider_text_block_init(text_block *block, int x, int y, int width, int height,
    const lang_fragment *sequence, unsigned short sequence_size, sequence_positioning position);

void slider_draw(const slider_t *slider);
void slider_draw_array(const slider_t *sliders, unsigned int num_sliders);

int slider_handle_mouse(slider_t *slider, const mouse *m);
int slider_handle_mouse_array(slider_t *sliders, const mouse *m, unsigned int num_sliders);

int slider_handle_tooltip(const slider_t *slider, tooltip_context *c);
int slider_handle_tooltip_array(const slider_t *sliders, tooltip_context *c, unsigned int num_sliders);

#endif // GRAPHICS_SLIDER_H
