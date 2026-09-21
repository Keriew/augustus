#include "panel.h"

#include "assets/assets.h"
#include "graphics/button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"

#define INNER_PANEL_MIN_SIZE (2 * BLOCK_SIZE)
#define SCROLL_PANEL_WIDTH 24

void outer_panel_draw(int x, int y, int width_blocks, int height_blocks)
{
    int image_base = image_group(GROUP_DIALOG_BACKGROUND);
    int image_id;
    int image_y = 0;
    int y_add = 0;
    for (int yy = 0; yy < height_blocks; yy++) {
        int image_x = 0;
        for (int xx = 0; xx < width_blocks; xx++) {
            if (yy == 0) {
                if (xx == 0) {
                    image_id = 0;
                } else if (xx < width_blocks - 1) {
                    image_id = 1 + image_x++;
                } else {
                    image_id = 11;
                }
                y_add = 0;
            } else if (yy < height_blocks - 1) {
                if (xx == 0) {
                    image_id = 12 + image_y;
                } else if (xx < width_blocks - 1) {
                    image_id = 13 + image_y + image_x++;
                } else {
                    image_id = 23 + image_y;
                }
                y_add = 12;
            } else {
                if (xx == 0) {
                    image_id = 132;
                } else if (xx < width_blocks - 1) {
                    image_id = 133 + image_x++;
                } else {
                    image_id = 143;
                }
                y_add = 0;
            }
            image_draw(image_base + image_id, x + BLOCK_SIZE * xx, y + BLOCK_SIZE * yy, COLOR_MASK_NONE, SCALE_NONE);
            if (image_x >= 10) {
                image_x = 0;
            }
        }
        image_y += y_add;
        if (image_y >= 120) {
            image_y = 0;
        }
    }
}

void outer_panel_draw_colored(int x, int y, int width, int height, color_t color)
{
    int width_blocks = (width + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int height_blocks = (height + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int image_base = image_group(GROUP_DIALOG_BACKGROUND);
    int image_id;
    int image_y = 0;
    int y_add = 0;
    for (int yy = 0; yy < height_blocks; yy++) {
        int image_x = 0;
        for (int xx = 0; xx < width_blocks; xx++) {
            if (yy == 0) {
                if (xx == 0) {
                    image_id = 0;
                } else if (xx < width_blocks - 1) {
                    image_id = 1 + image_x++;
                } else {
                    image_id = 11;
                }
                y_add = 0;
            } else if (yy < height_blocks - 1) {
                if (xx == 0) {
                    image_id = 12 + image_y;
                } else if (xx < width_blocks - 1) {
                    image_id = 13 + image_y + image_x++;
                } else {
                    image_id = 23 + image_y;
                }
                y_add = 12;
            } else {
                if (xx == 0) {
                    image_id = 132;
                } else if (xx < width_blocks - 1) {
                    image_id = 133 + image_x++;
                } else {
                    image_id = 143;
                }
                y_add = 0;
            }
            image_draw(image_base + image_id, x + BLOCK_SIZE * xx, y + BLOCK_SIZE * yy, color, SCALE_NONE);
            if (image_x >= 10) {
                image_x = 0;
            }
        }
        image_y += y_add;
        if (image_y >= 120) {
            image_y = 0;
        }
    }
}

void unbordered_panel_draw_colored(int x, int y, int width_blocks, int height_blocks, color_t color)
{
    int image_base = image_group(GROUP_DIALOG_BACKGROUND);
    int image_y = 0;
    for (int yy = 0; yy < height_blocks; yy++) {
        int image_x = 0;
        for (int xx = 0; xx < width_blocks; xx++) {
            int image_id = 13 + image_y + image_x++;
            image_draw(image_base + image_id, x + BLOCK_SIZE * xx, y + BLOCK_SIZE * yy, color, SCALE_NONE);
            if (image_x >= 10) {
                image_x = 0;
            }
        }
        image_y += 12;
        if (image_y >= 120) {
            image_y = 0;
        }
    }
}

void unbordered_panel_draw(int x, int y, int width_blocks, int height_blocks)
{
    unbordered_panel_draw_colored(x, y, width_blocks, height_blocks, COLOR_MASK_NONE);
}

void unbordered_panel_draw_px(int x, int y, int width_px, int height_px)
{
    graphics_set_clip_rectangle(x, y, width_px, height_px);
    int width_blocks = (width_px + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int height_blocks = (height_px + BLOCK_SIZE - 1) / BLOCK_SIZE;
    unbordered_panel_draw(x, y, width_blocks, height_blocks);
    graphics_reset_clip_rectangle();
}

void bordered_panel_draw_colored(int x, int y, int width_px, int height_px, int has_focus, color_t color_bg, color_t color_border)
{
    if (width_px <= 0 || height_px <= 0) {
        return;
    }

    graphics_set_clip_rectangle(x, y, width_px, height_px);
    int width_blocks = (width_px + BLOCK_SIZE - 1) / BLOCK_SIZE;
    int height_blocks = (height_px + BLOCK_SIZE - 1) / BLOCK_SIZE;
    unbordered_panel_draw_colored(x, y, width_blocks, height_blocks, color_bg);
    graphics_reset_clip_rectangle();
    button_border_draw_colored(x, y, width_px, height_px, has_focus, color_border);
}

void scrollbar_panel_draw(int x, int y, int height_px)
{
    if (height_px <= BLOCK_SIZE * 2) { // minimum height to draw the panel is 2 blocks - start and end.
        return;
    }
    graphics_set_clip_rectangle(x, y, SCROLL_PANEL_WIDTH, height_px);
    int main_blocks = (height_px - 2 * BLOCK_SIZE + BLOCK_SIZE - 1) / BLOCK_SIZE;

    int start_id = assets_lookup_image_id(ASSET_UI_SCROLL_BG_01);
    int mid_id = assets_lookup_image_id(ASSET_UI_SCROLL_BG_02);
    int end_id = assets_lookup_image_id(ASSET_UI_SCROLL_BG_03);
    int drawing_y = y + BLOCK_SIZE;
    image_draw(start_id, x, y, COLOR_MASK_NONE, SCALE_NONE);
    for (int yy = 0; yy < main_blocks; yy++) {
        image_draw(mid_id, x, drawing_y, COLOR_MASK_NONE, SCALE_NONE);
        drawing_y += BLOCK_SIZE;
    }
    image_draw(end_id, x, drawing_y, COLOR_MASK_NONE, SCALE_NONE);
    graphics_reset_clip_rectangle();
}

void scrollbar_thumb_draw(int x, int y, int middle_sections, int is_vertical, int frame)
{
    if (middle_sections < 0) {
        middle_sections = 0;
    }

    if (frame < 1 || frame > 4) {
        frame = 1;
    }

    static const asset_id vertical_start_ids[4] = {
        ASSET_UI_SCROLLBAR_MIDDLE_01_END_TOP,
        ASSET_UI_SCROLLBAR_MIDDLE_02_END_TOP,
        ASSET_UI_SCROLLBAR_MIDDLE_03_END_TOP,
        ASSET_UI_SCROLLBAR_MIDDLE_04_END_TOP,
    };
    static const asset_id vertical_end_ids[4] = {
        ASSET_UI_SCROLLBAR_MIDDLE_01_END_BOTTOM,
        ASSET_UI_SCROLLBAR_MIDDLE_02_END_BOTTOM,
        ASSET_UI_SCROLLBAR_MIDDLE_03_END_BOTTOM,
        ASSET_UI_SCROLLBAR_MIDDLE_04_END_BOTTOM,
    };
    static const asset_id vertical_mid_ids[4] = {
        ASSET_UI_SCROLLBAR_MIDDLE_01_TRIMMED,
        ASSET_UI_SCROLLBAR_MIDDLE_02_TRIMMED,
        ASSET_UI_SCROLLBAR_MIDDLE_03_TRIMMED,
        ASSET_UI_SCROLLBAR_MIDDLE_04_TRIMMED,
    };

    static const asset_id horizontal_start_ids[4] = {
        ASSET_UI_SCROLLBAR_MIDDLE_01B_END_LEFT,
        ASSET_UI_SCROLLBAR_MIDDLE_02B_END_LEFT,
        ASSET_UI_SCROLLBAR_MIDDLE_03B_END_LEFT,
        ASSET_UI_SCROLLBAR_MIDDLE_04B_END_LEFT,
    };
    static const asset_id horizontal_end_ids[4] = {
        ASSET_UI_SCROLLBAR_MIDDLE_01B_END_RIGHT,
        ASSET_UI_SCROLLBAR_MIDDLE_02B_END_RIGHT,
        ASSET_UI_SCROLLBAR_MIDDLE_03B_END_RIGHT,
        ASSET_UI_SCROLLBAR_MIDDLE_04B_END_RIGHT,
    };
    static const asset_id horizontal_mid_ids[4] = {
        ASSET_UI_SCROLLBAR_MIDDLE_01B_TRIMMED,
        ASSET_UI_SCROLLBAR_MIDDLE_02B_TRIMMED,
        ASSET_UI_SCROLLBAR_MIDDLE_03B_TRIMMED,
        ASSET_UI_SCROLLBAR_MIDDLE_04B_TRIMMED,
    };

    int start_id;
    int end_id;
    int mid_id;
    const int frame_index = frame - 1;
    if (is_vertical) {
        start_id = assets_lookup_image_id(vertical_start_ids[frame_index]);
        end_id = assets_lookup_image_id(vertical_end_ids[frame_index]);
        mid_id = assets_lookup_image_id(vertical_mid_ids[frame_index]);
    } else {
        start_id = assets_lookup_image_id(horizontal_start_ids[frame_index]);
        end_id = assets_lookup_image_id(horizontal_end_ids[frame_index]);
        mid_id = assets_lookup_image_id(horizontal_mid_ids[frame_index]);
    }

    const image *start_img = image_get(start_id);
    const image *end_img = image_get(end_id);
    const image *mid_img = image_get(mid_id);

    int start_span = is_vertical ? start_img->original.height : start_img->original.width;
    int end_span = is_vertical ? end_img->original.height : end_img->original.width;
    int middle_span = is_vertical ? mid_img->original.height : mid_img->original.width;
    int thumb_width = is_vertical ? mid_img->original.width : start_span + middle_sections * middle_span + end_span;
    int thumb_height = is_vertical ? start_span + middle_sections * middle_span + end_span : mid_img->original.height;

    if (start_span <= 0 || end_span <= 0 || middle_span <= 0 || thumb_width <= 0 || thumb_height <= 0) {
        return;
    }

    graphics_set_clip_rectangle(x, y, thumb_width, thumb_height);

    if (is_vertical) {
        image_draw(start_id, x, y, COLOR_MASK_NONE, SCALE_NONE);

        int middle_y = y + start_span;
        for (int i = 0; i < middle_sections; ++i) {
            image_draw(mid_id, x, middle_y, COLOR_MASK_NONE, SCALE_NONE);
            middle_y += middle_span;
        }

        image_draw(end_id, x, middle_y, COLOR_MASK_NONE, SCALE_NONE);
    } else {
        image_draw(start_id, x, y, COLOR_MASK_NONE, SCALE_NONE);

        int middle_x = x + start_span;
        for (int i = 0; i < middle_sections; ++i) {
            image_draw(mid_id, middle_x, y, COLOR_MASK_NONE, SCALE_NONE);
            middle_x += middle_span;
        }

        image_draw(end_id, middle_x, y, COLOR_MASK_NONE, SCALE_NONE);
    }

    if (middle_sections > 0) {
        int lines_alpha_id = assets_lookup_image_id(ASSET_UI_SCROLLBAR_LINES_ALPHA);
        const image *lines_alpha_img = image_get(lines_alpha_id);
        if (is_vertical) {
            int lines_y = y + (thumb_height - lines_alpha_img->original.height) / 2;
            image_draw(lines_alpha_id, x, lines_y, COLOR_MASK_NONE, SCALE_NONE);
        } else {
            int lines_x = x + (thumb_width - lines_alpha_img->original.width) / 2;
            image_draw(lines_alpha_id, lines_x, y, COLOR_MASK_NONE, SCALE_NONE);
        }
    }

    graphics_reset_clip_rectangle();
}

void inner_panel_draw(int x, int y, int width_blocks, int height_blocks)
{
    int image_base = image_group(GROUP_SUNKEN_TEXTBOX_BACKGROUND);
    int image_y = 0;
    int y_add = 0;
    for (int yy = 0; yy < height_blocks; yy++) {
        int image_x = 0;
        for (int xx = 0; xx < width_blocks; xx++) {
            int image_id;
            if (yy == 0) {
                if (xx == 0) {
                    image_id = 0;
                } else if (xx < width_blocks - 1) {
                    image_id = 1 + image_x++;
                } else {
                    image_id = 6;
                }
                y_add = 0;
            } else if (yy < height_blocks - 1) {
                if (xx == 0) {
                    image_id = 7 + image_y;
                } else if (xx < width_blocks - 1) {
                    image_id = 8 + image_y + image_x++;
                } else {
                    image_id = 13 + image_y;
                }
                y_add = 7;
            } else {
                if (xx == 0) {
                    image_id = 42;
                } else if (xx < width_blocks - 1) {
                    image_id = 43 + image_x++;
                } else {
                    image_id = 48;
                }
                y_add = 0;
            }
            image_draw(image_base + image_id, x + BLOCK_SIZE * xx, y + BLOCK_SIZE * yy, COLOR_MASK_NONE, SCALE_NONE);
            if (image_x >= 5) {
                image_x = 0;
            }
        }
        image_y += y_add;
        if (image_y >= 35) {
            image_y = 0;
        }
    }
}

static int divide_round_up(int value, int divisor)
{
    return (value + divisor - 1) / divisor;
}

void inner_panel_draw_colored(int x, int y, int width, int height, color_t color)
{
    int image_base = image_group(GROUP_SUNKEN_TEXTBOX_BACKGROUND);

    int right_x = x + width - BLOCK_SIZE;
    int bottom_y = y + height - BLOCK_SIZE;

    int inner_width = width - 2 * BLOCK_SIZE;
    int inner_height = height - 2 * BLOCK_SIZE;

    int inner_columns = divide_round_up(inner_width, BLOCK_SIZE);
    int inner_rows = divide_round_up(inner_height, BLOCK_SIZE);
    image_draw(image_base, x, y, color, SCALE_NONE);

    for (int column = 0; column < inner_columns; column++) {
        int image_id = 1 + column % 5;

        image_draw(image_base + image_id, x + BLOCK_SIZE * (column + 1), y, color, SCALE_NONE);
    }

    image_draw(image_base + 6, right_x, y, color, SCALE_NONE);
    for (int row = 0; row < inner_rows; row++) {
        int image_y = (row % 5) * 7;
        int draw_y = y + BLOCK_SIZE * (row + 1);
        image_draw(image_base + 7 + image_y, x, draw_y, color, SCALE_NONE);
        for (int column = 0; column < inner_columns; column++) {
            int image_x = column % 5;
            int image_id = 8 + image_y + image_x;
            image_draw(image_base + image_id, x + BLOCK_SIZE * (column + 1), draw_y, color, SCALE_NONE);
        }
        image_draw(image_base + 13 + image_y, right_x, draw_y, color, SCALE_NONE);
    }
    image_draw(image_base + 42, x, bottom_y, color, SCALE_NONE);

    for (int column = 0; column < inner_columns; column++) {
        int image_id = 43 + column % 5;
        image_draw(image_base + image_id, x + BLOCK_SIZE * (column + 1), bottom_y, color, SCALE_NONE);
    }
    image_draw(image_base + 48, right_x, bottom_y, color, SCALE_NONE);
}

void label_draw(int x, int y, int width_blocks, int type)
{
    int image_base = image_group(GROUP_PANEL_BUTTON);
    for (int i = 0; i < width_blocks; i++) {
        int image_id;
        if (i == 0) {
            image_id = 3 * type + 40;
        } else if (i < width_blocks - 1) {
            image_id = 3 * type + 41;
        } else {
            image_id = 3 * type + 42;
        }
        image_draw(image_base + image_id, x + BLOCK_SIZE * i, y, COLOR_MASK_NONE, SCALE_NONE);
    }
}

void large_label_draw(int x, int y, int width_blocks, int type)
{
    int image_base = image_group(GROUP_PANEL_BUTTON);
    for (int i = 0; i < width_blocks; i++) {
        int image_id;
        if (i == 0) {
            image_id = 3 * type;
        } else if (i < width_blocks - 1) {
            image_id = 3 * type + 1;
        } else {
            image_id = 3 * type + 2;
        }
        image_draw(image_base + image_id, x + BLOCK_SIZE * i, y, COLOR_MASK_NONE, SCALE_NONE);
    }
}

void large_label_draw_custom_size(int x, int y, int width, int height)
{
    if (width < 32 || height < 16) {
        return;
    }
    large_label_draw_bg(x, y, width, height);
    large_label_draw_border(x, y, width, height);
}

void large_label_draw_bg(int x, int y, int width, int height)
{
    graphics_set_clip_rectangle(x, y, width, height);
    const int panel_width_left = 13;
    const int panel_width_middle = 16;
    const int panel_height = 19;

    int panel_base = assets_lookup_image_id(ASSET_UI_BTN_MENU_LEFT_PANEL);
    int panel_mirror_base = assets_lookup_image_id(ASSET_UI_BTN_MENU_LEFT_PANEL_MIRROR_V);
    int panel_rows = (height + panel_height - 1) / panel_height;
    int panel_middle_blocks = (width - 2 * panel_width_left + panel_width_middle - 1) / panel_width_middle;


    // Draw normal panel rows.
    for (int i = 0; i < panel_rows; i++) {
        int row_y = y + i * panel_height;

        image_draw(panel_base, x, row_y, COLOR_MASK_NONE, SCALE_NONE);
        for (int j = 0; j < panel_middle_blocks; j++) {
            image_draw(panel_base + 1, x + panel_width_left + j * panel_width_middle, row_y, COLOR_MASK_NONE, SCALE_NONE);
        }
        image_draw(panel_base + 2, x + width - panel_width_left, row_y, COLOR_MASK_NONE, SCALE_NONE);
    }

    // Draw a mirrored half-opacity row across each seam.
    for (int i = 1; i < panel_rows; i++) {
        int row_y = y + i * panel_height - panel_height / 2;

        image_draw(panel_mirror_base, x, row_y, COLOR_MASK_50_OPACITY, SCALE_NONE);
        for (int j = 0; j < panel_middle_blocks; j++) {
            image_draw(panel_mirror_base + 1, x + panel_width_left + j * panel_width_middle, row_y, COLOR_MASK_50_OPACITY, SCALE_NONE);
        }
        image_draw(panel_mirror_base + 2, x + width - panel_width_left, row_y, COLOR_MASK_50_OPACITY, SCALE_NONE);
    }
    graphics_reset_clip_rectangle();
}

static inline uint32_t color_mask_opacity(int opacity)
{
    if (opacity < 0) {
        opacity = 0;
    } else if (opacity > 100) {
        opacity = 100;
    }

    uint32_t alpha = (uint32_t) ((opacity * 255 + 50) / 100); // rounded
    return (alpha << 24) | 0x00ffffffu;
}

void label_draw_greyout_pattern(int x, int y, int width, int height, int opacity)
{
    graphics_set_clip_rectangle(x, y, width, height);
    int diagonal_lines = assets_lookup_image_id(ASSET_UI_DIAGONAL_LINES_R);
    uint32_t color = color_mask_opacity(opacity);
    for (int yy = 0; yy < height; yy += 8) {
        for (int xx = 0; xx < width; xx += 8) {
            image_draw(diagonal_lines, x + xx, y + yy, color, SCALE_NONE);
        }
    }
    graphics_reset_clip_rectangle();
}

void large_label_draw_border(int x, int y, int width, int height)
{
    graphics_set_clip_rectangle(x, y, width, height);
    const int frame_size = 16;
    int frame_base = assets_lookup_image_id(ASSET_UI_BTN_MENU_FRAME_01);

    int horizontal_blocks = (width - 2 * frame_size + frame_size - 1) / frame_size;
    int vertical_blocks = (height - 2 * frame_size + frame_size - 1) / frame_size;

    // Top
    image_draw(frame_base, x, y, COLOR_MASK_NONE, SCALE_NONE); // left
    for (int i = 0; i < horizontal_blocks; i++) {
        image_draw(frame_base + 1, x + frame_size + i * frame_size, y, COLOR_MASK_NONE, SCALE_NONE); // mid
    }
    image_draw(frame_base + 2, x + width - frame_size, y, COLOR_MASK_NONE, SCALE_NONE); // right

    // Sides
    for (int i = 0; i < vertical_blocks; i++) {
        int frame_y = y + frame_size + i * frame_size;
        image_draw(frame_base + 3, x, frame_y, COLOR_MASK_NONE, SCALE_NONE); // left
        image_draw(frame_base + 4, x + width - frame_size, frame_y, COLOR_MASK_NONE, SCALE_NONE); // right
    }

    // Bottom
    image_draw(frame_base + 5, x, y + height - frame_size, COLOR_MASK_NONE, SCALE_NONE); // left
    for (int i = 0; i < horizontal_blocks; i++) {
        image_draw(frame_base + 6, x + frame_size + i * frame_size, y + height - frame_size, COLOR_MASK_NONE, SCALE_NONE); // mid
    }
    image_draw(frame_base + 7, x + width - frame_size, y + height - frame_size, COLOR_MASK_NONE, SCALE_NONE); // right

    graphics_reset_clip_rectangle();
}

int top_menu_black_panel_draw(int x, int y, int width)
{
    int blocks = ((width + BLACK_PANEL_BLOCK_WIDTH - 1) / BLACK_PANEL_BLOCK_WIDTH) - 2;
    if (blocks < BLACK_PANEL_MIDDLE_BLOCKS) {
        blocks = BLACK_PANEL_MIDDLE_BLOCKS;
    }
    int actual_width = (blocks + 2) * BLACK_PANEL_BLOCK_WIDTH;

    image_draw(image_group(GROUP_TOP_MENU) + 14, x, y, COLOR_MASK_NONE, SCALE_NONE);
    x += BLACK_PANEL_BLOCK_WIDTH;

    int black_panel_base_id = assets_get_image_id("UI", "Top_UI_Panel");

    for (int i = 0; i < blocks; i++) {
        image_draw(black_panel_base_id + (i % BLACK_PANEL_MIDDLE_BLOCKS) + 1, x, y,
            COLOR_MASK_NONE, SCALE_NONE);
        x += BLACK_PANEL_BLOCK_WIDTH;
    }

    image_draw(black_panel_base_id + 5, x, y, COLOR_MASK_NONE, SCALE_NONE);

    return actual_width;
}
