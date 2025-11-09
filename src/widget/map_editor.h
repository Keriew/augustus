#ifndef WIDGET_MAP_EDITOR_H
#define WIDGET_MAP_EDITOR_H

#include "graphics/tooltip.h"
#include "input/hotkey.h"
#include "input/mouse.h"

void widget_map_editor_draw(void);

void widget_map_editor_get_tooltip(tooltip_context *c);

void widget_map_editor_handle_input(const mouse *m, const hotkeys *h);

void widget_map_editor_clear_current_tile(void);

int widget_map_editor_get_grid_offset(void);

void widget_map_editor_clear_draw_context_event_tiles(void);

int widget_map_editor_add_draw_context_event_tile(int grid_offset, int event_id);

int widget_map_editor_get_event_id_at_grid_offset(int grid_offset);

void widget_map_editor_custom_earthquake_request_refresh(void);

#endif // WIDGET_MAP_EDITOR_H
