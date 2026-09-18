#include "package_map.h"

#include "core/array.h"
#include "core/file.h"
#include "core/zlib_helper.h"
#include "graphics/font.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/window.h"
#include "input/input.h"
#include "translation/translation.h"

#define WINDOW_WIDTH 30
#define WINDOW_HEIGHT 20

static struct {
    int exporting;
    char (*files)[FILE_NAME_MAX]; // A list of files of length FILE_NAME_MAX
    int file_count;
    int capacity;
} data;

int add_file(const char *name)
{
    if (data.file_count == data.capacity) {
        size_t new_cap = data.capacity ? data.capacity * 2 : 8;
        char (*tmp)[FILE_NAME_MAX] = realloc(data.files, new_cap * sizeof *tmp);
        if (!tmp) {
            return -1;
        }
        data.files = tmp;
        data.capacity = new_cap;
    }
    snprintf(data.files[data.file_count], FILE_NAME_MAX, "%s", name);  // safe, always terminated
    data.file_count++;
    return 0;
}

static void find_files(void)
{

}

static void init(void)
{
    find_files();
    data.exporting = 1;
    zip_package_map("", data.files, data.file_count, "", MZ_DEFAULT_LEVEL);
}

static void draw_foreground(void)
{
    graphics_in_dialog_with_size(WINDOW_WIDTH * BLOCK_SIZE, WINDOW_HEIGHT * BLOCK_SIZE);

    outer_panel_draw(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);

    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_EDITOR_PACKAGE_MAP, 0, 24, WINDOW_WIDTH * BLOCK_SIZE, FONT_LARGE_BLACK);

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
