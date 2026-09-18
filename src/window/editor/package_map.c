#include "package_map.h"

#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "graphics/window.h"
#include "input/input.h"

#define WINDOW_WIDTH 30
#define WINDOW_HEIGHT 20

static struct {
    int exporting;
} data;

static void init(void)
{
    
}

static void draw_foreground(void)
{
    graphics_in_dialog_with_size(WINDOW_WIDTH * BLOCK_SIZE, WINDOW_HEIGHT * BLOCK_SIZE);

    outer_panel_draw(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    graphics_reset_dialog();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    if (input_go_back_requested(m, h) && !data.exporting) {
        window_go_back();
    }
}


void window_map_editor_package_map_show(void)
{
    init();
    window_type window = {
        WINDOW_EDITOR_PACKAGE_MAP,
        window_draw_underlying_window,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}
