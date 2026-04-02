#ifndef MAP_TILE_TYPE_REGISTRY_H
#define MAP_TILE_TYPE_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

const char *tile_type_registry_get_tile_type_path(void);
int tile_type_registry_validate_mod(void);
int tile_type_registry_load(void);

#ifdef __cplusplus
}
#endif

#endif // MAP_TILE_TYPE_REGISTRY_H
