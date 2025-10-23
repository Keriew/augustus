#ifndef WIDGET_MAP_EDITOR_H
#define WIDGET_MAP_EDITOR_H

#include "input/hotkey.h"
#include "input/mouse.h"

typedef struct {
    int grid_offset;
    int event_id;
} map_editor_event_tile;

void widget_map_editor_draw(void);

void widget_map_editor_handle_input(const mouse *m, const hotkeys *h);

void widget_map_editor_clear_current_tile(void);

int widget_map_editor_get_grid_offset(void);

#endif // WIDGET_MAP_EDITOR_H

void widget_map_editor_clear_draw_context_event_tiles(void);

int widget_map_editor_add_draw_context_event_tile(int grid_offset, int event_id);
