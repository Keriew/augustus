#ifndef WINDOW_OVERLAY_MENU_H
#define WINDOW_OVERLAY_MENU_H
#include <stdint.h>

void window_overlay_menu_show(void);

void window_overlay_menu_update(void);

const uint8_t *get_current_overlay_text(void);

#endif // WINDOW_OVERLAY_MENU_H
