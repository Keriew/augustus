#include "map.h"

#include "city/view.h"
#include "core/config.h"
#include "editor/editor.h"
#include "editor/tool.h"
#include "game/game.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "scenario/event/controller.h"
#include "widget/map_editor.h"
#include "widget/top_menu_editor.h"
#include "widget/sidebar/editor.h"
#include "window/file_dialog.h"
#include "window/popup_dialog.h"
#include "window/editor/attributes.h"

static void init(void)
{
    scenario_events_fetch_event_tiles_to_editor();
}

static void draw_background(void)
{
    graphics_clear_screen();
    widget_sidebar_editor_draw_background();
    widget_top_menu_editor_draw();
}

static void draw_cancel_construction(void)
{
    if (!mouse_get()->is_touch || !editor_tool_is_active()) {
        return;
    }
    int x, y, width, height;
    city_view_get_viewport(&x, &y, &width, &height);
    width = screen_pixel_to_ui(width);
    width -= 4 * BLOCK_SIZE;
    inner_panel_draw(width - 4, 40, 3, 2);
    image_draw(image_group(GROUP_OK_CANCEL_SCROLL_BUTTONS) + 4, width, 44, COLOR_MASK_NONE, SCALE_NONE);
}

static void draw_foreground(void)
{
    widget_sidebar_editor_draw_foreground();
    screen_set_pixel_render_scale();
    widget_map_editor_draw();
    screen_set_ui_render_scale();
    widget_top_menu_editor_draw_panels();
    if (window_is(WINDOW_EDITOR_MAP)) {
        draw_cancel_construction();
    }
}

static void handle_hotkeys(const hotkeys *h)
{
    if (h->load_file) {
        window_file_dialog_show(FILE_TYPE_SCENARIO, FILE_DIALOG_LOAD);
    }
    if (h->save_file) {
        window_file_dialog_show(FILE_TYPE_SCENARIO, FILE_DIALOG_SAVE);
    }
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_pixel = mouse_get_pixel();
    handle_hotkeys(h);
    if (widget_top_menu_editor_handle_input(m, h)) {
        return;
    }
    if (widget_sidebar_editor_handle_mouse(m)) {
        return;
    }
    widget_map_editor_handle_input(m_pixel, h);
}

static void get_tooltip(tooltip_context *c)
{
    const mouse *m_pixel = mouse_get_pixel();
    c->mouse_x = m_pixel->x;
    c->mouse_y = m_pixel->y;
    screen_set_pixel_render_scale();
    widget_map_editor_get_tooltip(c);
    if (c->type == TOOLTIP_NONE) {
        screen_set_ui_render_scale();
    }
}

void window_editor_map_draw_all(void)
{
    screen_set_ui_render_scale();
    draw_background();
    draw_foreground();
}

void window_editor_map_draw_panels(void)
{
    screen_set_ui_render_scale();
    draw_background();
}

void window_editor_map_draw(void)
{
    screen_set_pixel_render_scale();
    widget_map_editor_draw();
    screen_set_ui_render_scale();
}

void window_editor_map_show(void)
{
    init();
    window_type window = {
        WINDOW_EDITOR_MAP,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    window_show(&window);
}
