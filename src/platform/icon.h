#pragma once

#include "SDL.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Get the window icon's pixels
 * @return A pointer to the icon's pixels
 */
void *platform_icon_get_pixels(void);

#ifdef __cplusplus
}
#endif
