#ifndef MAP_IMAGE_H
#define MAP_IMAGE_H

#include "core/buffer.h"

unsigned int map_image_at(int grid_offset);

void map_image_set(int grid_offset, int image_id);

// Set both the live image and the undo backup image for a single tile. Used by
// auto-clear of vegetation so that, after we lay a road/aqueduct/building on the
// cleared tile, the undo backup still shows grass (not the original tree image
// and not the just-placed road/building image).
void map_image_set_with_backup(int grid_offset, int image_id);

void map_image_backup(void);

void map_image_restore(void);

void map_image_restore_at(int grid_offset);

void map_image_clear(void);
void map_image_init_edges(void);
void map_image_update_all(void);

void map_image_save_state_legacy(buffer *buf);

void map_image_load_state_legacy(buffer *buf);

#endif // MAP_IMAGE_H
