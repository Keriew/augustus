#ifndef WINDOW_SELECT_LIST_H
#define WINDOW_SELECT_LIST_H

#include "graphics/generic_button.h"

#include <stdint.h>

typedef enum {
    SELECT_LIST_STYLE_DEFAULT = 0,  // Basic style: single rectangle with text highlight
    SELECT_LIST_STYLE_BUTTONS,     // Bordered style: adds borders around focused items
} select_list_style;

/**
 * @brief Show dropdown using translation group entries (uses default style)
 * @param callback Function called with selected 0-based index
 */
void window_select_list_show(int x, int y, const generic_button *button, int group, int num_items,
    void (*callback)(int));

/**
 * @brief Show dropdown using text array (uses default style)
 * @param callback Function called with selected 0-based index
 */
void window_select_list_show_text(int x, int y, const generic_button *button, const uint8_t **items, int num_items,
    void (*callback)(int));

/**
 * @brief Show dropdown using translation group entries with custom style
 * @param style Visual style from select_list_style enum
 * @param callback Function called with selected 0-based index
 */
void window_select_list_show_styled(int x, int y, const generic_button *button, int group, int num_items,
    void (*callback)(int), select_list_style style);

/**
 * @brief Show dropdown using text array with custom style
 * @param style Visual style from select_list_style enum
 * @param callback Function called with selected 0-based index
 */
void window_select_list_show_text_styled(int x, int y, const generic_button *button, const uint8_t **items, int num_items,
    void (*callback)(int), select_list_style style);

#endif // WINDOW_SELECT_LIST_H
