#include "lua_input_dialog.h"

#include "assets/assets.h"
#include "core/dir.h"
#include "core/encoding.h"
#include "core/file.h"
#include "core/image.h"
#include "core/string.h"
#include "graphics/complex_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/panel.h"
#include "graphics/rich_text.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/mouse.h"

#include <string.h>

#define DIALOG_X 80
#define DIALOG_WIDTH_BLOCKS 30
#define DIALOG_WIDTH (DIALOG_WIDTH_BLOCKS * BLOCK_SIZE)  // 480
#define BUTTON_WIDTH 250
#define BUTTON_HEIGHT 30
#define BUTTON_SPACING 8
#define TEXT_MARGIN 20
#define MAX_IMAGE_WIDTH (DIALOG_WIDTH - 2 * TEXT_MARGIN)
#define MAX_IMAGE_HEIGHT 200

static lua_input_dialog_data_t data;
static int pending_show;
static int image_id;
static int buttons_initialized;

static lang_fragment button_fragments[LUA_INPUT_DIALOG_MAX_BUTTONS];
static complex_button dialog_buttons[LUA_INPUT_DIALOG_MAX_BUTTONS];

static void button_click_handler(const complex_button *button);

lua_input_dialog_data_t *window_lua_input_dialog_data(void)
{
    return &data;
}

static void init_buttons(void)
{
    int button_x = DIALOG_X + (DIALOG_WIDTH - BUTTON_WIDTH) / 2;
    for (int i = 0; i < data.num_buttons; i++) {
        button_fragments[i].type = LANG_FRAG_TEXT;
        button_fragments[i].text = data.buttons[i].label;

        memset(&dialog_buttons[i], 0, sizeof(complex_button));
        dialog_buttons[i].x = button_x;
        dialog_buttons[i].width = BUTTON_WIDTH;
        dialog_buttons[i].height = BUTTON_HEIGHT;
        dialog_buttons[i].sequence = &button_fragments[i];
        dialog_buttons[i].sequence_size = 1;
        dialog_buttons[i].sequence_position = SEQUENCE_POSITION_CENTER;
        dialog_buttons[i].style = COMPLEX_BUTTON_STYLE_DEFAULT;
        dialog_buttons[i].left_click_handler = button_click_handler;
        dialog_buttons[i].parameters[0] = i;
    }
    buttons_initialized = 1;
}

static void get_image_draw_size(int *draw_w, int *draw_h)
{
    const image *img = image_get(image_id);
    int w = img->width;
    int h = img->height;
    if (w <= 0 || h <= 0) {
        *draw_w = 0;
        *draw_h = 0;
        return;
    }
    // Scale down to fit within max bounds, preserving aspect ratio
    if (w > MAX_IMAGE_WIDTH) {
        h = h * MAX_IMAGE_WIDTH / w;
        w = MAX_IMAGE_WIDTH;
    }
    if (h > MAX_IMAGE_HEIGHT) {
        w = w * MAX_IMAGE_HEIGHT / h;
        h = MAX_IMAGE_HEIGHT;
    }
    *draw_w = w;
    *draw_h = h;
}

static int calculate_dialog_height(void)
{
    int y = 20; // top padding

    if (data.title[0]) {
        y += 30;
    }
    if (data.subtitle[0]) {
        y += 20;
    }
    if (image_id) {
        int img_w, img_h;
        get_image_draw_size(&img_w, &img_h);
        if (img_h > 0) {
            y += img_h + 10;
        }
    }
    if (data.text[0]) {
        int unused_width;
        int lines = text_measure_multiline(data.text, DIALOG_WIDTH - 2 * TEXT_MARGIN, FONT_NORMAL_BLACK, &unused_width);
        y += lines * 20 + 10;
    }

    // buttons area
    y += data.num_buttons * (BUTTON_HEIGHT + BUTTON_SPACING);
    y += 15; // bottom padding

    return y;
}

static int dialog_y;
static int dialog_height_blocks;

static void draw_background(void)
{
    window_draw_underlying_window();
    graphics_in_dialog();

    int content_height = calculate_dialog_height();
    dialog_height_blocks = (content_height + BLOCK_SIZE - 1) / BLOCK_SIZE;
    dialog_y = (480 - dialog_height_blocks * BLOCK_SIZE) / 2;
    if (dialog_y < 40) {
        dialog_y = 40;
    }

    outer_panel_draw(DIALOG_X, dialog_y, DIALOG_WIDTH_BLOCKS, dialog_height_blocks);

    int y = dialog_y + 20;

    if (data.title[0]) {
        text_draw_centered(data.title, DIALOG_X, y, DIALOG_WIDTH, FONT_LARGE_BLACK, 0);
        y += 30;
    }
    if (data.subtitle[0]) {
        text_draw_centered(data.subtitle, DIALOG_X, y, DIALOG_WIDTH, FONT_NORMAL_BLACK, 0);
        y += 20;
    }
    if (image_id) {
        int img_w, img_h;
        get_image_draw_size(&img_w, &img_h);
        if (img_h > 0) {
            const image *img = image_get(image_id);
            int img_x = DIALOG_X + (DIALOG_WIDTH - img->width) / 2;
            image_draw(image_id, img_x, y, COLOR_MASK_NONE, SCALE_NONE);
            y += img_h + 10;
        }
    }
    if (data.text[0]) {
        y += text_draw_multiline(data.text, DIALOG_X + TEXT_MARGIN, y, DIALOG_WIDTH - 2 * TEXT_MARGIN,
            0, FONT_NORMAL_BLACK, 0);
        y += 10;
    }

    // Update button Y positions (without resetting state)
    for (int i = 0; i < data.num_buttons; i++) {
        dialog_buttons[i].y = y + i * (BUTTON_HEIGHT + BUTTON_SPACING);
    }

    graphics_reset_dialog();
}

static void draw_foreground(void)
{
    graphics_in_dialog();
    complex_button_array_draw(dialog_buttons, data.num_buttons);
    graphics_reset_dialog();
}

static void button_click_handler(const complex_button *button)
{
    int index = button->parameters[0];
    if (data.on_button_click) {
        data.on_button_click(index);
    }
    window_go_back();
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);
    complex_button_array_handle_mouse(m_dialog, dialog_buttons, data.num_buttons);
    // No ESC handling - player must click a button
}

static void resolve_image(void)
{
    image_id = 0;
    if (!data.image[0]) {
        return;
    }
    // First try the standard rich_text paths (campaigns/image/, image/)
    const uint8_t *img_text = data.image;
    image_id = rich_text_parse_image_id(&img_text, 0, 1);
    if (image_id) {
        return;
    }
    // Fallback: search in the scenario directory
    int length = string_length(data.image) + 1;
    char filename[FILE_NAME_MAX];
    encoding_to_utf8(data.image, filename, length, encoding_system_uses_decomposed());
    const char *found = dir_get_file_at_location(filename, PATH_LOCATION_SCENARIO);
    if (found) {
        image_id = assets_get_external_image(found, 0);
    }
}

static void show_dialog(void)
{
    if (window_is(WINDOW_LUA_INPUT_DIALOG)) {
        return;
    }
    resolve_image();
    buttons_initialized = 0;
    init_buttons();
    window_type window = {
        WINDOW_LUA_INPUT_DIALOG,
        draw_background,
        draw_foreground,
        handle_input
    };
    window_show(&window);
}

void window_lua_input_dialog_show(void)
{
    pending_show = 1;
    // If city window is already active, show immediately
    if (window_is(WINDOW_CITY)) {
        pending_show = 0;
        show_dialog();
    }
}

void window_lua_input_dialog_process_pending(void)
{
    if (pending_show) {
        pending_show = 0;
        show_dialog();
    }
}
