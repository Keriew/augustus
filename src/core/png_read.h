#ifndef CORE_PNG_H
#define CORE_PNG_H

#include "graphics/color.h"

int png_get_image_size(const char *path, int *width, int *height);

int png_read(const char *path, color_t *pixels, int x_offset, int y_offset, int width, int height);

void png_release(void);

#endif // CORE_PNG_H
