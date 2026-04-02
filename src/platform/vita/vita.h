#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __vita__

#include <stdint.h>

#define VITA_PATH_PREFIX "ux0:/data/julius/"
#define VITA_DISPLAY_WIDTH 960
#define VITA_DISPLAY_HEIGHT 544

#define PLATFORM_ENABLE_INIT_CALLBACK
void platform_init_callback(void);

#define PLATFORM_ENABLE_PER_FRAME_CALLBACK
void platform_per_frame_callback(void);

#define PLATFORM_USE_VIRTUAL_KEYBOARD
void platform_show_virtual_keyboard(void);
void platform_hide_virtual_keyboard(void);

#define PLATFORM_USE_SOFTWARE_CURSOR

#define PLATFORM_NO_USER_DIRECTORIES

#endif // __vita__

#ifdef __cplusplus
}
#endif
