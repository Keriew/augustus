#include "complex_button.h"

#include "graphics/button.h"
#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "input/mouse.h"

#include <stddef.h>

static void draw_default_style(const complex_button *button)
{
    const int inner_margin = 2; // small horizontal margin for text/images
    const font_t font = !button->is_disabled ? FONT_NORMAL_BLACK : FONT_NORMAL_WHITE;

    graphics_set_clip_rectangle(button->x, button->y, button->width, button->height);

    int height_blocks = button->height / BLOCK_SIZE;
    unbordered_panel_draw(button->x, button->y, button->width / BLOCK_SIZE + 1, height_blocks + 1);
    int draw_red_border = !button->is_disabled ? button->is_focused : 0;    // Only draw border if enabled
    button_border_draw(button->x, button->y, button->width, button->height, draw_red_border);

    // Y offset based on positioning enum (row: top, center, bottom)
    int text_height = font_definition_for(font)->line_height;
    int sequence_y_offset = 0;
    switch (button->sequence_position) {
        case SEQUENCE_POSITION_TOP_LEFT:
        case SEQUENCE_POSITION_TOP_CENTER:
        case SEQUENCE_POSITION_TOP_RIGHT:
            sequence_y_offset = button->y + inner_margin;
            break;
        case SEQUENCE_POSITION_CENTER_LEFT:
        case SEQUENCE_POSITION_CENTER:
        case SEQUENCE_POSITION_CENTER_RIGHT:
        default:
            sequence_y_offset = button->y + (button->height - text_height) / 2;
            break;
        case SEQUENCE_POSITION_BOTTOM_LEFT:
        case SEQUENCE_POSITION_BOTTOM_CENTER:
        case SEQUENCE_POSITION_BOTTOM_RIGHT:
            sequence_y_offset = button->y + button->height - text_height - inner_margin;
            break;
    }

    // Pre-calc widths
    int seq_width = lang_text_get_sequence_width(button->sequence, button->sequence_size, font);
    seq_width = seq_width % 2 ? seq_width - 1 : seq_width; // even up for better centering
    int img_before_w = 0, img_after_w = 0;
    const image *img_before = NULL, *img_after = NULL;

    if (button->image_before > 0) {
        img_before = image_get(button->image_before);
        img_before_w = img_before->width + inner_margin;
    }
    if (button->image_after > 0) {
        img_after = image_get(button->image_after);
        img_after_w = img_after->width + inner_margin;
    }

    int total_width = img_before_w + seq_width + img_after_w;

    int cursor_x = 0;
    switch (button->sequence_position) {
        case SEQUENCE_POSITION_TOP_CENTER:
        case SEQUENCE_POSITION_CENTER:
        case SEQUENCE_POSITION_BOTTOM_CENTER:
            cursor_x = button->x + (button->width - total_width) / 2;
            break;
        case SEQUENCE_POSITION_TOP_RIGHT:
        case SEQUENCE_POSITION_CENTER_RIGHT:
        case SEQUENCE_POSITION_BOTTOM_RIGHT:
            cursor_x = button->x + button->width - inner_margin - total_width;
            break;
        case SEQUENCE_POSITION_TOP_LEFT:
        case SEQUENCE_POSITION_CENTER_LEFT:
        case SEQUENCE_POSITION_BOTTOM_LEFT:
        default:
            cursor_x = button->x + inner_margin;
            break;
    }

    // Draw before-image if present
    color_t mask = !button->is_disabled ? COLOR_MASK_NONE : COLOR_MASK_GRAY;
    if (img_before) {
        int img_y = button->y + (button->height - img_before->height) / 2;
        image_draw(button->image_before, cursor_x, img_y, mask, SCALE_NONE);
        cursor_x += img_before->width + inner_margin;
    }

    // Draw sequence (centered version if enum is 2,5,8)
    if (button->sequence && button->sequence_size > 0) {
        if (button->sequence_position == SEQUENCE_POSITION_TOP_CENTER ||
            button->sequence_position == SEQUENCE_POSITION_CENTER ||
            button->sequence_position == SEQUENCE_POSITION_BOTTOM_CENTER) {
            lang_text_draw_sequence_centered(
                button->sequence, button->sequence_size, button->x, sequence_y_offset, button->width, font);
        } else {
            cursor_x += lang_text_draw_sequence(
                button->sequence, button->sequence_size, cursor_x, sequence_y_offset, font);
        }
    }

    // Draw after-image if present
    if (img_after) {
        int img_y = button->y + (button->height - img_after->height) / 2;
        image_draw(button->image_after, cursor_x + inner_margin, img_y, mask, SCALE_NONE);
    }

    graphics_reset_clip_rectangle();
}

static void draw_grey_style(const complex_button *button)
{
    graphics_set_clip_rectangle(button->x, button->y, button->width, button->height);
    // hold the place for the placeholder
}

// === Draw a single button ===
void complex_button_draw(const complex_button *button)
{
    if (button->is_hidden) {
        return;
    }

    switch (button->style) {
        case COMPLEX_BUTTON_STYLE_DEFAULT:
            draw_default_style(button);
            break;
        case COMPLEX_BUTTON_STYLE_GRAY:
            draw_grey_style(button);
            break;
    }
}

void complex_button_array_draw(const complex_button *buttons, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        complex_button_draw(&buttons[i]);
    }
}

int complex_button_handle_mouse(const mouse *m, complex_button *btn)
{
    if (btn->is_hidden || btn->is_disabled) {
        btn->is_focused = 0;
        btn->is_clicked = 0;
        return 0;
    }

    int handled = 0;

    // Expanded hitbox
    int left = btn->x - btn->expanded_hitbox_radius;
    int right = btn->x + btn->width + btn->expanded_hitbox_radius;
    int top = btn->y - btn->expanded_hitbox_radius;
    int bottom = btn->y + btn->height + btn->expanded_hitbox_radius;

    int inside = (m->x >= left && m->x < right && m->y >= top && m->y < bottom);

    btn->is_focused = inside ? 1 : 0;

    if (inside) {
        if (btn->hover_handler) {
            btn->hover_handler(btn);
        }

        // --- Left click ---

        if (m->left.went_up) {
            btn->is_clicked = 1;
            btn->is_active = !btn->is_active; // persistent toggle
            handled = 1;
            if (btn->left_click_handler) {
                btn->left_click_handler(btn);
            }

        }

        // --- Right click ---

        if (m->right.went_up) {
            btn->is_clicked = 1;
            handled = 1;
            if (btn->right_click_handler) {
                btn->right_click_handler(btn);
            }
        }
    } else {
        btn->is_clicked = 0;
    }

    return handled;
}

int complex_button_array_handle_mouse(const mouse *m, complex_button *buttons, unsigned int num_buttons)
{
    int handled = 0;

    for (unsigned int i = 0; i < num_buttons; i++) {
        if (complex_button_handle_mouse(m, &buttons[i])) {
            handled = 1;
        }
    }

    return handled;
}
