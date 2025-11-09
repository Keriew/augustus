#include "complex_button.h"

#include "graphics/button.h"
#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "graphics/window.h"
#include "input/mouse.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static void complex_button_ellipsized(complex_button *button, int was_ellipsized);

color_t complex_button_basic_colors(int id)
{
    switch (id) {
        case 1: return COLOR_MASK_PASTEL_GREEN;
        case 2: return COLOR_MASK_PASTEL_PURPLE;
        case 3: return COLOR_MASK_PASTEL_ORANGE;
        case 4: return COLOR_MASK_PASTEL_OLIVE;
        case 5: return COLOR_MASK_PASTEL_TURQUOISE;
        case 6: return COLOR_MASK_PASTEL_CORAL;
        case 7: return COLOR_MASK_PASTEL_GRAY;
        case 8: return COLOR_MASK_PASTEL_BLUE;
        case 9: return COLOR_MASK_PASTEL_DARK_BLUE;
        case 10: return COLOR_MASK_PASTEL_BLACK;
        default: return COLOR_MASK_NONE;
    }
}

static void draw_default_style(const complex_button *button, font_t base_font, color_t label_color)
{
    font_t font;
    const int inner_margin = 2; // small horizontal margin for text/images
    switch (base_font) {
        default:
        case FONT_NORMAL_BLACK:
            font = !button->is_disabled ? FONT_NORMAL_BLACK : FONT_NORMAL_WHITE;
            break;
        case FONT_SMALL_PLAIN:
            font = base_font;
            break;
    }
    const color_t f_color = button->is_disabled ? COLOR_FONT_GRAY : COLOR_MASK_NONE;
    label_color = label_color ? label_color : COLOR_MASK_NONE;
    graphics_set_clip_rectangle(button->x, button->y, button->width, button->height);

    int height_blocks = button->height / BLOCK_SIZE;
    unbordered_panel_draw_colored(button->x, button->y, button->width / BLOCK_SIZE + 1, height_blocks + 1,
        label_color);
    int draw_red_border = !button->is_disabled ? button->is_focused : 0;    // Only draw border if enabled
    button_border_draw_colored(button->x, button->y, button->width, button->height, draw_red_border, label_color);
    sequence_positioning pos = (!button->sequence_position) ? SEQUENCE_POSITION_CENTER : button->sequence_position;
    // Y offset based on positioning enum (row: top, center, bottom)
    int text_height = font_definition_for(font)->line_height;
    int sequence_y_offset = 0;
    switch (pos) {
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
    switch (pos) {
        case SEQUENCE_POSITION_TOP_RIGHT:
        case SEQUENCE_POSITION_CENTER_RIGHT:
        case SEQUENCE_POSITION_BOTTOM_RIGHT:
            cursor_x = button->x + button->width - inner_margin - total_width;
            break;
        case SEQUENCE_POSITION_TOP_LEFT:
        case SEQUENCE_POSITION_CENTER_LEFT:
        case SEQUENCE_POSITION_BOTTOM_LEFT:
            cursor_x = button->x + inner_margin;
            break;
        case SEQUENCE_POSITION_TOP_CENTER:
        case SEQUENCE_POSITION_CENTER:
        case SEQUENCE_POSITION_BOTTOM_CENTER:
        default:
            cursor_x = button->x + (button->width - total_width) / 2;
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
    int was_ellipsized = 0;
    if (button->sequence && button->sequence_size > 0) {
        if (pos == SEQUENCE_POSITION_TOP_CENTER || pos == SEQUENCE_POSITION_CENTER ||
             pos == SEQUENCE_POSITION_BOTTOM_CENTER) {
            lang_text_draw_sequence_centered_ellipsized(button->sequence, button->sequence_size, button->x,
                sequence_y_offset, button->width, font, f_color, &was_ellipsized);
        } else {
            cursor_x += lang_text_draw_sequence_ellipsized(button->sequence, button->sequence_size, cursor_x,
                sequence_y_offset, button->width, font, f_color, &was_ellipsized);
        }
    }
    complex_button_ellipsized((complex_button *) button, was_ellipsized); //de-constant button to set ellipsized flag

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

static void complex_button_ellipsized(complex_button *button, int was_ellipsized)
{
    button->is_ellipsized = was_ellipsized;
}

// === Draw a single button ===
void complex_button_draw(const complex_button *button)
{
    if (button->is_hidden) {
        return;
    }
    color_t base_color = button->color_mask ? button->color_mask : COLOR_MASK_NONE;
    font_t base_font = button->font ? button->font : FONT_NORMAL_BLACK;
    switch (button->style) {
        case COMPLEX_BUTTON_STYLE_DEFAULT_SMALL:
            draw_default_style(button, base_font, base_color);
            break;
        case COMPLEX_BUTTON_STYLE_GRAY:
            draw_grey_style(button);
            break;
        case COMPLEX_BUTTON_STYLE_DEFAULT:
        default:
            draw_default_style(button, base_font, base_color);
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
    if (btn->is_focused != inside) {
        window_request_refresh(); // redraw to show focus change
    }
    btn->is_focused = inside;
    if (btn->is_ellipsized && btn->is_focused) { //if the button is ellipsized, show tooltip
        static uint8_t tooltip_text[512];
        lang_text_concatenate_sequence(btn->sequence, btn->sequence_size, tooltip_text, 512);
        btn->tooltip_c.type = TOOLTIP_BUTTON;
        btn->tooltip_c.precomposed_text = tooltip_text; // reset precomposed text to force re-generation
    }

    if (inside) {

        if (btn->hover_handler) {
            btn->hover_handler(btn);
            // hover handler does not consume the event, but it doesn't request refresh either
            // if needed, the handler should call window_request_refresh() or window_invalidate()
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

int complex_button_handle_tooltip(const complex_button *button, tooltip_context *c)
{
    if (button->is_focused) {
        c->type = button->tooltip_c.type;
        c->precomposed_text = button->tooltip_c.precomposed_text;
        return 1;
    }
    return 0;
}

int complex_button_array_handle_tooltip(const complex_button *buttons, unsigned int num_buttons, tooltip_context *c)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        if (complex_button_handle_tooltip(&buttons[i], c)) {
            return 1;
        }
    }
    return 0;
}
