#ifndef GRAPHICS_DROPDOWN_BUTTON_H
#define GRAPHICS_DROPDOWN_BUTTON_H

#include "graphics/complex_button.h"

/**
 * @brief Maximum allowed dropdown width in pixels when auto-sizing.
 */
#define DROPDOWN_BUTTON_MAX_WIDTH 400

 /**
  * @brief A dropdown widget built on top of complex_button.
  *
  * The first button in the array is the origin (the clickable dropdown header).
  * The remaining buttons are the options shown when expanded.
  */
typedef struct {
    complex_button *buttons;     /**< Buttons array: [0] = origin, [1..] = options */
    unsigned int num_buttons;    /**< Total count (origin + options) */
    short expanded;              /**< 1 = expanded, 0 = collapsed */
    int selected_index;          /**< Index of selected option (>=1), -1 if none */

    /* Layout configuration */
    int width;                   /**< Dropdown width: 0 = auto (based on longest text) */
    int spacing;                 /**< Vertical spacing between option buttons (px) */
    int padding;                 /**< Horizontal padding added to text width (px) */

    /* Cached layout values */
    int calculated_width;        /**< Final calculated width */
    int calculated_height;       /**< Option button height (all options same) */
} dropdown_button;

/**
 * @brief Initialize a dropdown and calculate its geometry.
 *
 * @param dd           Pointer to dropdown_button to initialize.
 * @param buttons      Array of complex_buttons (0 = origin, 1..N-1 = options).
 * @param num_buttons  Number of buttons in the array.
 * @param width        Desired width in px (0 = auto based on longest text + padding).
 * @param spacing      Vertical spacing in px between option buttons.
 * @param padding      Horizontal padding in px applied around text for auto-width.
 */
void dropdown_button_init(dropdown_button *dd, complex_button *buttons,
    unsigned int num_buttons, int width, int spacing, int padding);

/**
 * @brief Draw a dropdown (origin button + expanded options if expanded).
 *
 * @param dd Pointer to the dropdown_button to draw.
 */
void dropdown_button_draw(const dropdown_button *dd);

/**
 * @brief Handle mouse input for a dropdown.
 *
 * Processes input for the origin button and, if expanded, all option buttons.
 * Updates expanded state and selected option index.
 *
 * @param m  Pointer to mouse state.
 * @param dd Pointer to dropdown_button to process.
 * @return 1 if any button handled input, 0 otherwise.
 */
int dropdown_button_handle_mouse(const mouse *m, dropdown_button *dd);

#endif // GRAPHICS_DROPDOWN_BUTTON_H
