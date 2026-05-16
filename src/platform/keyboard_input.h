#ifndef PLATFORM_KEYBOARD_INPUT_H
#define PLATFORM_KEYBOARD_INPUT_H

void platform_handle_key_down(void *event);

void platform_handle_key_up(void *event);

void platform_handle_editing_text(void *event);

void platform_handle_text(void *event);

#endif // PLATFORM_KEYBOARD_INPUT_H
