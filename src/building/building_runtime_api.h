#ifndef BUILDING_BUILDING_RUNTIME_API_H
#define BUILDING_BUILDING_RUNTIME_API_H

#include "core/image.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct building building;
// Transitional runtime layer: callers use this instead of routing behavior through the type/XML registry.
void building_runtime_reset(void);
void building_runtime_apply_graphic(building *b);
void building_runtime_spawn_figure(building *b);
const image *building_runtime_get_graphic_image(building *b);
const image *building_runtime_get_graphic_animation_frame(building *b);
int building_runtime_owns_graphics(building *b);
int building_runtime_owns_graphic_animation(building *b);
int building_runtime_get_graphic_animation_layer_count(building *b);
const image *building_runtime_get_graphic_animation_layer_image(building *b, int layer_index);
const image *building_runtime_get_graphic_animation_layer_frame(building *b, int layer_index);

#ifdef __cplusplus
}
#endif

#endif // BUILDING_BUILDING_RUNTIME_API_H
