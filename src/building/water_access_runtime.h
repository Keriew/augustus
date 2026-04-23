#ifndef BUILDING_WATER_ACCESS_RUNTIME_H
#define BUILDING_WATER_ACCESS_RUNTIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "building/type.h"

typedef struct building building;

enum {
    WATER_ACCESS_RUNTIME_TYPE_NONE = 0,
    WATER_ACCESS_RUNTIME_TYPE_WELL = 1,
    WATER_ACCESS_RUNTIME_TYPE_FOUNTAIN = 2,
    WATER_ACCESS_RUNTIME_TYPE_RESERVOIR = 3
};

void water_access_runtime_reset(void);
void water_access_runtime_refresh(void);
void water_access_runtime_refresh_large_statue(building *b);

int water_access_runtime_range_for_building(building_type type);
int water_access_runtime_tile_has_access(int grid_offset, int access_type);
int water_access_runtime_building_area_has_access(const building *b, int access_type);
int water_access_runtime_reservoir_has_network_access(int grid_offset);

void water_access_runtime_begin_preview(building_type type, int primary_grid_offset, int secondary_grid_offset);
void water_access_runtime_end_preview(void);
int water_access_runtime_tile_has_preview_highlight(int grid_offset);
int water_access_runtime_should_draw_overlay_at(int grid_offset);

#ifdef __cplusplus
}
#endif

#endif // BUILDING_WATER_ACCESS_RUNTIME_H
