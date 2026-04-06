#ifndef BUILDING_BUILDING_RUNTIME_API_H
#define BUILDING_BUILDING_RUNTIME_API_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct building building;
// Lifecycle hooks that remain callable from legacy C systems while runtime graphics move through C++-only APIs.
void building_runtime_reset(void);
// After save load/new city init, prime runtime wrappers for all live building instances and precompute their cached sandwiches.
void building_runtime_initialize_city_graphics_cache(void);
void building_runtime_apply_graphic(building *b);
void building_runtime_spawn_figure(building *b);

#ifdef __cplusplus
}
#endif

#endif // BUILDING_BUILDING_RUNTIME_API_H
