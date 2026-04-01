#ifndef BUILDING_BUILDING_RUNTIME_API_H
#define BUILDING_BUILDING_RUNTIME_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct building building;

// Transitional runtime layer: callers use this instead of routing behavior through the type/XML registry.
void building_runtime_reset(void);
void building_runtime_apply_graphic(building *b);
void building_runtime_spawn_figure(building *b);

#ifdef __cplusplus
}
#endif

#endif // BUILDING_BUILDING_RUNTIME_API_H
