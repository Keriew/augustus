#ifndef GRAPHICS_DROPDOWN_BUTTON_H
#define GRAPHICS_DROPDOWN_BUTTON_H

#include "graphics/complex_button.h"

/**
 * @brief Maximum allowed dropdown width in pixels when auto-sizing.
 */
#define DROPDOWN_BUTTON_MAX_WIDTH 400
#define DROPDOWN_BUTTON_MAX_COUNT 32 // arbitrary limit to limit memory print of dropdowns

typedef enum {
    DD_BUTTON_STYLE_DEFAULT,            // Basic: white/red border, default plain background fill
    DD_BUTTON_STYLE_DEFAULT_SMALL,      // like default but small font and less padding
    DD_BUTTON_STYLE_GRAY,               // main-menu-like style
} dropdown_button_style;

typedef struct dropdown_button dropdown_button;  // forward declaration

/**
 * @brief A dropdown widget built on top of complex_button.
 *
 * The first button in the array is the origin (the clickable dropdown header).
 * The remaining buttons are the options shown when expanded.
 */
struct dropdown_button {
    complex_button buttons[DROPDOWN_BUTTON_MAX_COUNT]; /**< Buttons array: [0] = origin, [1..] = options */
    complex_button anchor_backup;
    unsigned int num_buttons;              /**< Total count (origin + options) */
    short expanded;                        /**< 1 = expanded, 0 = collapsed */
    int selected_index;                    /**< Index of selected option (>=1), -1 if none */
    int selected_value;                    /**< Arbitrary value carried by selected option */
    void (*selected_callback)              /**< click handler for options */
        (dropdown_button *button);         /**< The dropdown_button pointer is handed over to the selected_callback*/
    void (*rightclick_expanded_callback)   /**< If null, all rightclicks while expanded will de-expand the dropdown*/
        (dropdown_button *button);         /**< The dropdown_button pointer is handed over to the rightclick_callback*/
    short show_origin;                     /**< 1 = show anchor[0] button on no selection, or while expanded */
    /**< 0 = always show selected index if present, instead of the origin button */
/* Layout configuration */
    int height;                            /**< Dropdown height: 0 = auto (based on font line height + padding) */
    int width;                             /**< Dropdown width: 0 = auto (based on longest text) */
    int spacing;                           /**< Vertical spacing between option buttons (px) */
    int padding;                           /**< Horizontal padding added to text width (px) */
    int reverse_order;                     /**< 1 = options get drawn in reverse order (last option closest to origin)*/
    int drop_up;                           /**< 1 = options get drawn upwards instead of downwards */
    /* Cached layout values */
    int calculated_width;                  /**< Final calculated width */
    int calculated_height;                 /**< Option button height (all options same) */
    /* Internal state and attributes - do not modify */
};

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
    unsigned int num_buttons, int width, int height, int spacing, int padding);

/**
 * @brief Simplified initialization: only x, y, fragment list, and count required.
 *
 * Creates a dropdown where the origin is the first fragment,
 * and all other fragments become options. Buttons are defaulted to
 * visible and enabled, auto-sized based on longest text + padding.
 *
 * @param x      X coordinate of origin button
 * @param y      Y coordinate of origin button
 * @param frags  Array of lang_fragments (size = count)
 * @param count  Number of fragments (>=1)
 * @param dd     Pointer to dropdown_button to initialize
 * @param style  Style to apply to all buttons, 0 for default
 * @param origin_tooltip Optional tooltip context to be applied to the origin/anchor button.
 */
void dropdown_button_init_simple(int x, int y, int width, int height, const lang_fragment *frags,
    unsigned int count, dropdown_button *dd, dropdown_button_style style, tooltip_context *origin_tooltip);

/**
 * @brief Update dropdown geometry only (position and size for origin and options).
 *
 * Does not modify callbacks, style, selection, visibility, or other non-geometry state.
 * Width/height values <= 0 keep the current calculated dimensions.
 */
void dropdown_button_update_dimensions(int x, int y, int width, int height, dropdown_button *dd);

int dropdown_button_handle_tooltip(const dropdown_button *dd, tooltip_context *c);
int dropdown_button_handle_tooltip_array(const dropdown_button *dds, tooltip_context *c, unsigned int num_dropdowns);

/**
 * @brief Draw a dropdown (origin button + expanded options if expanded).
 *
 * @param dd Pointer to the dropdown_button to draw.
 */
void dropdown_button_draw(const dropdown_button *dd);
void dropdown_button_draw_array(const dropdown_button *dds, unsigned int num_dropdowns);

/**
 * @brief Handle mouse input for a dropdown.
 *
 * Processes input for the origin button and, if expanded, all option buttons.
 * Updates expanded state and selected option index.
 *
 * @param dd Pointer to dropdown_button to process.
 * @param m  Pointer to mouse state.
 * @return 1 if any button handled input, 0 otherwise.
 */
int dropdown_button_handle_mouse(dropdown_button *dd, const mouse *m);
int dropdown_button_handle_mouse_array(dropdown_button *dds, const mouse *m, unsigned int num_dropdowns);

/**
 * @brief Default click handler for dropdown options.
 * Sets the selected index and value, collapses the dropdown,
 * and updates the origin button's text to match the selected option.
 * @param btn Pointer to the clicked complex_button (option).
 */
void dropdown_button_default_option_click(complex_button *btn);

/**
 * @brief Exposing internal helper for non-simple init users
 */
void dropdown_button_advanced_update_anchor(dropdown_button *dd);

/**
 * @brief Exposing internal helper for non-simple init users
 */
void dropdown_button_advanced_restore_anchor(dropdown_button *dd);

/**
 * @brief Exposing internal helper for non-simple init users
 */
void dropdown_button_advanced_save_anchor(dropdown_button *dd);


int dropdown_button_get_x_min(dropdown_button *dd);
int dropdown_button_get_x_max(dropdown_button *dd);
int dropdown_button_get_width(dropdown_button *dd);

#endif // GRAPHICS_DROPDOWN_BUTTON_H


