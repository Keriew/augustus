#ifndef WIDGET_CITY_WITH_OVERLAY_H
#define WIDGET_CITY_WITH_OVERLAY_H

#include "graphics/tooltip.h"
#include "map/point.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Update the internal state after changing overlay
 */
void city_with_overlay_update(void);

void city_with_overlay_draw(const map_tile *tile, unsigned int roamer_preview_building_id);

int city_with_overlay_get_tooltip_text(tooltip_context *c, int grid_offset);

#ifdef __cplusplus
}
#endif

#endif // WIDGET_CITY_WITH_OVERLAY_H
