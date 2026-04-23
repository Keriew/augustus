#ifndef GRAPHICS_RUNTIME_OVERLAY_IMAGES_H
#define GRAPHICS_RUNTIME_OVERLAY_IMAGES_H

#include "core/image.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RUNTIME_OVERLAY_IMAGE_WATER_RANGE,
    RUNTIME_OVERLAY_IMAGE_MAX
} runtime_overlay_image;

int runtime_overlay_images_init_or_reload(void);
void runtime_overlay_images_reset(void);
const image *runtime_overlay_image_get(runtime_overlay_image type);

#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_RUNTIME_OVERLAY_IMAGES_H
