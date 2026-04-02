#pragma once

#include "core/image.h"
#include "graphics/renderer.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int legacy_image_extractor_extract_climate(
    const image *images,
    int image_count,
    const uint16_t *group_image_ids,
    int group_count,
    const char *source_name,
    const image_atlas_data *atlas_data);

#ifdef __cplusplus
}
#endif
