#ifndef MAP_TILE_RUNTIME_API_H
#define MAP_TILE_RUNTIME_API_H

#ifdef __cplusplus
extern "C" {
#endif

void tile_runtime_reset(void);
void tile_runtime_clear(int grid_offset);
void tile_runtime_set_plaza_image_index(int grid_offset, int image_index);

#ifdef __cplusplus
}
#endif

#endif // MAP_TILE_RUNTIME_API_H
