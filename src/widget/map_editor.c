#include "map_editor.h"

#include "assets/assets.h"
#include "city/view.h"
#include "city/warning.h"
#include "core/config.h"
#include "core/lang.h"
#include "core/string.h"
#include "editor/tool.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/menu.h"
#include "graphics/panel.h"
#include "graphics/renderer.h"
#include "graphics/window.h"
#include "input/scroll.h"
#include "input/zoom.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/point.h"
#include "map/property.h"
#include "sound/city.h"
#include "sound/effect.h"
#include "translation/translation.h"
#include "widget/city_figure.h"
#include "widget/map_editor_pause_menu.h"
#include "widget/map_editor_tool.h"


static struct {
    map_tile current_tile;
    int selected_grid_offset;
    int new_start_grid_offset;
    int capture_input;
    int cursor_grid_offset;
} data;

static struct {
    time_millis last_water_animation_time;
    int advance_water_animation;

    int image_id_water_first;
    int image_id_water_last;
    float scale;
} draw_context;

static void init_draw_context(void)
{
    draw_context.advance_water_animation = 0;
    time_millis now = time_get_millis();
    if (now - draw_context.last_water_animation_time > 60) {
        draw_context.last_water_animation_time = now;
        draw_context.advance_water_animation = 1;
    }
    draw_context.image_id_water_first = image_group(GROUP_TERRAIN_WATER);
    draw_context.image_id_water_last = 5 + draw_context.image_id_water_first;
    draw_context.scale = city_view_get_scale() / 100.0f;
}

static void draw_footprint(int x, int y, int grid_offset)
{
    if (grid_offset < 0 || !map_property_is_draw_tile(grid_offset)) {
        return;
    }
    // Valid grid_offset and leftmost tile -> draw
    color_t color_mask = 0;
    int image_id = map_image_at(grid_offset);
    if (draw_context.advance_water_animation &&
        image_id >= draw_context.image_id_water_first &&
        image_id <= draw_context.image_id_water_last) {
        image_id++;
        if (image_id > draw_context.image_id_water_last) {
            image_id = draw_context.image_id_water_first;
        }
        map_image_set(grid_offset, image_id);
    }
    image_draw_isometric_footprint_from_draw_tile(image_id, x, y, color_mask, draw_context.scale);
    if (config_get(CONFIG_UI_SHOW_GRID) && draw_context.scale <= 2.0f) {
        //grid is drawn by the renderer directly at zoom > 200%
        static int grid_id = 0;
        if (!grid_id) {
            grid_id = assets_get_image_id("UI", "Grid_Full");
        }
        image_draw(grid_id, x, y, COLOR_GRID, draw_context.scale);
    }
}

static void draw_top(int x, int y, int grid_offset)
{
    if (!map_property_is_draw_tile(grid_offset)) {
        return;
    }
    int image_id = map_image_at(grid_offset);
    color_t color_mask = 0;
    image_draw_isometric_top_from_draw_tile(image_id, x, y, color_mask, draw_context.scale);
}

static void draw_flags(int x, int y, int grid_offset)
{
    int figure_id = map_figure_at(grid_offset);
    while (figure_id) {
        figure *f = figure_get(figure_id);
        if (!f->is_ghost) {
            city_draw_figure(f, x, y, draw_context.scale, 0);
        }
        figure_id = f->next_figure_id_on_same_tile;
    }
}

static void display_zoom_warning(int zoom)
{
    static uint8_t zoom_string[100];
    static int warning_id;
    if (!*zoom_string) {
        uint8_t *cursor = string_copy(lang_get_string(CUSTOM_TRANSLATION, TR_ZOOM), zoom_string, 100);
        string_copy(string_from_ascii(" "), cursor, (int) (cursor - zoom_string));
    }
    int position = string_length(lang_get_string(CUSTOM_TRANSLATION, TR_ZOOM)) + 1;
    position += string_from_int(zoom_string + position, zoom, 0);
    string_copy(string_from_ascii("%"), zoom_string + position, 100 - position);
    warning_id = city_warning_show_custom(zoom_string, warning_id);
}

static void update_zoom_level(void)
{
    int zoom = city_view_get_scale();
    pixel_offset offset;
    city_view_get_camera_in_pixels(&offset.x, &offset.y);
    if (zoom_update_value(&zoom, city_view_get_max_scale(), &offset)) {
        city_view_set_scale(zoom);
        city_view_set_camera_from_pixel_position(offset.x, offset.y);
        display_zoom_warning(zoom);
        sound_city_decay_views();
    }
}

void widget_map_editor_draw(void)
{
    update_zoom_level();
    set_city_clip_rectangle();

    init_draw_context();
    int x, y, width, height;
    city_view_get_viewport(&x, &y, &width, &height);
    graphics_fill_rect(x, y, width, height, COLOR_BLACK);
    city_view_foreach_valid_map_tile(draw_footprint);
    city_view_foreach_valid_map_tile_row(draw_flags, draw_top, 0);
    map_editor_tool_draw(&data.current_tile);
    graphics_reset_clip_rectangle();
}

static void update_city_view_coords(int x, int y, map_tile *tile)
{
    view_tile view;
    if (city_view_pixels_to_view_tile(x, y, &view)) {
        tile->grid_offset = city_view_tile_to_grid_offset(&view);
        city_view_set_selected_view_tile(&view);
        tile->x = map_grid_offset_to_x(tile->grid_offset);
        tile->y = map_grid_offset_to_y(tile->grid_offset);
    } else {
        tile->grid_offset = tile->x = tile->y = 0;
    }
    data.cursor_grid_offset = tile->grid_offset;
}

static void scroll_map(const mouse *m)
{
    pixel_offset delta;
    if (scroll_get_delta(m, &delta, SCROLL_TYPE_CITY)) {
        city_view_scroll(delta.x, delta.y);
        sound_city_decay_views();
    }
}

static int input_coords_in_map(int x, int y)
{
    int x_offset, y_offset, width, height;
    city_view_get_viewport(&x_offset, &y_offset, &width, &height);

    x -= x_offset;
    y -= y_offset;

    return (x >= 0 && x < width && y >= 0 && y < height);
}

static void handle_touch_scroll(const touch *t)
{
    if (editor_tool_is_active()) {
        if (t->has_started) {
            int x_offset, y_offset, width, height;
            city_view_get_viewport(&x_offset, &y_offset, &width, &height);
            scroll_set_custom_margins(x_offset, y_offset, width, height);
        }
        if (t->has_ended) {
            scroll_restore_margins();
        }
        return;
    }
    scroll_restore_margins();

    if (!data.capture_input) {
        return;
    }
    int was_click = touch_was_click(touch_get_latest());
    if (t->has_started || was_click) {
        scroll_drag_start(1);
        return;
    }

    if (!touch_not_click(t)) {
        return;
    }

    if (t->has_ended) {
        scroll_drag_end();
    }
}

static void handle_touch_zoom(const touch *first, const touch *last)
{
    if (touch_not_click(first)) {
        zoom_update_touch(first, last, city_view_get_scale());
    }
    if (first->has_ended || last->has_ended) {
        zoom_end_touch();
    }
}

static void handle_last_touch(void)
{
    const touch *last = touch_get_latest();
    if (!last->in_use) {
        return;
    }
    if (touch_was_click(last)) {
        editor_tool_deactivate();
    } else if (touch_not_click(last)) {
        handle_touch_zoom(touch_get_earliest(), last);
    }
}

static int handle_cancel_construction_button(const touch *t)
{
    if (!editor_tool_is_active()) {
        return 0;
    }
    int x, y, width, height;
    city_view_get_viewport(&x, &y, &width, &height);
    int box_size = 5 * BLOCK_SIZE;
    width -= box_size;

    if (t->current_point.x < width || t->current_point.x >= width + box_size ||
        t->current_point.y < 24 || t->current_point.y >= 40 + box_size) {
        return 0;
    }
    editor_tool_deactivate();
    return 1;
}

static void handle_first_touch(map_tile *tile)
{
    const touch *first = touch_get_earliest();

    if (touch_was_click(first)) {
        if (handle_cancel_construction_button(first)) {
            return;
        }
    }

    handle_touch_scroll(first);

    if (!input_coords_in_map(first->current_point.x, first->current_point.y)) {
        return;
    }

    if (editor_tool_is_updatable()) {
        if (!editor_tool_is_in_use()) {
            if (first->has_started) {
                editor_tool_start_use(tile);
                data.new_start_grid_offset = 0;
            }
        } else {
            if (first->has_started) {
                if (data.selected_grid_offset != tile->grid_offset) {
                    data.new_start_grid_offset = tile->grid_offset;
                }
            }
            if (touch_not_click(first) && data.new_start_grid_offset) {
                data.new_start_grid_offset = 0;
                data.selected_grid_offset = 0;
                editor_tool_deactivate();
                editor_tool_start_use(tile);
            }
            editor_tool_update_use(tile);
            if (data.selected_grid_offset != tile->grid_offset) {
                data.selected_grid_offset = 0;
            }
            if (first->has_ended) {
                if (data.selected_grid_offset == tile->grid_offset) {
                    editor_tool_end_use(tile);
                    widget_map_editor_clear_current_tile();
                    data.new_start_grid_offset = 0;
                } else {
                    data.selected_grid_offset = tile->grid_offset;
                }
            }
        }
        return;
    }

    if (editor_tool_is_brush()) {
        if (first->has_started) {
            editor_tool_start_use(tile);
        }
        editor_tool_update_use(tile);
        if (first->has_ended) {
            editor_tool_end_use(tile);
        }
        return;
    }

    if (touch_was_click(first) && first->has_ended && data.capture_input &&
        data.selected_grid_offset == tile->grid_offset) {
        editor_tool_start_use(tile);
        editor_tool_update_use(tile);
        editor_tool_end_use(tile);
        widget_map_editor_clear_current_tile();
    } else if (first->has_ended) {
        data.selected_grid_offset = tile->grid_offset;
    }
}

static void handle_touch(void)
{
    const touch *first = touch_get_earliest();
    if (!first->in_use) {
        scroll_restore_margins();
        return;
    }

    map_tile *tile = &data.current_tile;
    if (!editor_tool_is_in_use() || input_coords_in_map(first->current_point.x, first->current_point.y)) {
        update_city_view_coords(first->current_point.x, first->current_point.y, tile);
    }

    if (first->has_started && input_coords_in_map(first->current_point.x, first->current_point.y)) {
        data.capture_input = 1;
        scroll_restore_margins();
    }

    handle_last_touch();
    handle_first_touch(tile);

    if (first->has_ended) {
        data.capture_input = 0;
    }
}

void widget_map_editor_handle_input(const mouse *m, const hotkeys *h)
{
    scroll_map(m);

    if (m->is_touch) {
        handle_touch();
    } else {
        if (m->right.went_down && input_coords_in_map(m->x, m->y) && !editor_tool_is_active()) {
            scroll_drag_start(0);
        }
        if (m->right.went_up) {
            if (!editor_tool_is_active()) {
                int has_scrolled = scroll_drag_end();
                if (!has_scrolled) {
                    editor_tool_deactivate();
                }
            } else {
                editor_tool_deactivate();
            }
        }
    }

    if (h->escape_pressed) {
        if (editor_tool_is_active()) {
            editor_tool_deactivate();
        } else {
            window_map_editor_pause_menu_show();
        }
        return;
    }

    map_tile *tile = &data.current_tile;
    update_city_view_coords(m->x, m->y, tile);
    zoom_map(m, h, city_view_get_scale());

    if (tile->grid_offset) {
        if (m->left.went_down) {
            if (!editor_tool_is_in_use()) {
                editor_tool_start_use(tile);
            }
            editor_tool_update_use(tile);
        } else if (m->left.is_down || editor_tool_is_in_use()) {
            editor_tool_update_use(tile);
        }
    }
    if (m->left.went_up && editor_tool_is_in_use()) {
        editor_tool_end_use(tile);
        sound_effect_play(SOUND_EFFECT_BUILD);
    }
}

void widget_map_editor_clear_current_tile(void)
{
    data.selected_grid_offset = 0;
    data.current_tile.grid_offset = 0;
}

int widget_map_editor_get_grid_offset(void)
{
    return data.cursor_grid_offset;
}
