#ifndef GRAPHICS_SCROLLBAR_H
#define GRAPHICS_SCROLLBAR_H

#include "graphics/color.h"
#include "graphics/font.h"
#include "graphics/image_button.h"
#include "input/mouse.h"

#define LEGACY_SCROLL_BUTTON_HEIGHT 26
#define LEGACY_SCROLL_BUTTON_WIDTH 39
#define LEGACY_SCROLL_DOT_SIZE 25
#define SCROLL_BUTTON_SIDE 24 // square
#define SCROLL_BOX_HEIGHT 48
#define SCROLL_BOX_WIDTH 24
#define LEGACY_TOTAL_SCROLL_BUTTON_HEIGHT (2 * LEGACY_SCROLL_BUTTON_HEIGHT + LEGACY_SCROLL_DOT_SIZE)
#define TOTAL_SCROLL_BUTTON_HEIGHT (2 * SCROLL_BUTTON_SIDE + SCROLL_BOX_HEIGHT)

typedef struct {
    int x;
    int y;
    int height;
    int scrollable_width;
    unsigned int elements_in_view;
    void (*on_scroll_callback)(void);
    int has_y_margin;
    int dot_padding;
    int always_visible;
    unsigned int max_scroll_position;
    unsigned int scroll_position;
    int is_dragging_scrollbar_dot;
    int scrollbar_dot_drag_offset; // Exact position of the top of the thumb within its allowed travel
    int scrollbar_dot_mouse_offset; // Exact position of the mouse within the scrollbar thumb while dragging
    int scrollbar_dot_offset_from_mouse; // Keep a mouse-placed thumb pixel-stable after release
    unsigned int scrollbar_dot_offset_scroll_position;
    unsigned int scrollbar_dot_offset_max_scroll_position;
    int touch_drag_state;
    int position_on_touch;
    int legacy; // use legacy scrollbar graphics - config-managed
    int decorate_scrollbar; // draw the bg panel behind the scrollbar

    image_button image_button_scroll_up;
    image_button image_button_scroll_down;
    image_button image_button_scroll_dot;
} scrollbar_type;

/**
 * Initializes the scrollbar
 * @param scrollbar Scrollbar
 * @param scroll_position Scroll position to set
 * @param total_elements The number of elements to scroll
 */
void scrollbar_init(scrollbar_type *scrollbar, unsigned int scroll_position, unsigned int total_elements);

/**
 * Resets the text to the specified scroll position and forces recalculation of lines
 * @param scrollbar Scrollbar
 * @param scroll_position Scroll position to set
 */
void scrollbar_reset(scrollbar_type *scrollbar, unsigned int scroll_position);

/**
 * Update the number of total elements, adjusting the scroll position if necessary
 * @param scrollbar Scrollbar
 * @param total_elements New number of total elements
 */
void scrollbar_update_total_elements(scrollbar_type *scrollbar, unsigned int total_elements);

/**
 * Draws the scrollbar
 * @param scrollbar Scrollbar
 */
void scrollbar_draw(scrollbar_type *scrollbar);

/**
 * Handles mouse interaction with the scrollbar and scroll wheel
 * @param scrollbar Scrollbar
 * @param m Mouse state
 * @param in_dialog Whether we are inside a centered dialog box
 * @return True if any interaction was handled
 */
int scrollbar_handle_mouse(scrollbar_type *scrollbar, const mouse *m, int in_dialog);

#endif // GRAPHICS_SCROLLBAR_H
