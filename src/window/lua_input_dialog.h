#ifndef WINDOW_LUA_INPUT_DIALOG_H
#define WINDOW_LUA_INPUT_DIALOG_H

#define LUA_INPUT_DIALOG_MAX_BUTTONS 4
#define LUA_INPUT_DIALOG_MAX_TEXT_LENGTH 512

#include <stdint.h>

typedef struct {
    uint8_t title[LUA_INPUT_DIALOG_MAX_TEXT_LENGTH];
    uint8_t subtitle[LUA_INPUT_DIALOG_MAX_TEXT_LENGTH];
    uint8_t text[LUA_INPUT_DIALOG_MAX_TEXT_LENGTH];
    uint8_t image[LUA_INPUT_DIALOG_MAX_TEXT_LENGTH];
    struct {
        uint8_t label[LUA_INPUT_DIALOG_MAX_TEXT_LENGTH];
        int has_callback;
    } buttons[LUA_INPUT_DIALOG_MAX_BUTTONS];
    int num_buttons;
    void (*on_button_click)(int button_index);
} lua_input_dialog_data_t;

/**
 * Get the dialog data struct to populate before calling show.
 */
lua_input_dialog_data_t *window_lua_input_dialog_data(void);

/**
 * Show the input dialog. Data must be set up via window_lua_input_dialog_data() first.
 * If the city window is not yet active, defers showing until it is.
 */
void window_lua_input_dialog_show(void);

/**
 * Check for and show any pending input dialog.
 * Called from the city window's draw cycle (alongside city_message_process_queue).
 */
void window_lua_input_dialog_process_pending(void);

#endif // WINDOW_LUA_INPUT_DIALOG_H
