#include "widget/cycling_button.h"

#include "graphics/button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/tooltip.h"
#include "graphics/window.h"
#include "sound/effect.h"

#include <stddef.h>

static const cycling_button_state *cycling_button_get_state(const cycling_button *button);
static color_t cycling_button_color_for_style(cycling_button_style style);
static font_t cycling_button_font_for_style(cycling_button_style style);
static void draw_cycling_button_contents(const cycling_button *button, const cycling_button_state *state, font_t font);
static void cycling_button_draw_default_style(const cycling_button *button);
static void cycling_button_draw_gray_style(const cycling_button *button);

#pragma region Helpers

static const cycling_button_state *cycling_button_get_state(const cycling_button *button)
{
    if (!button || button->state_count <= 0 || button->state_count > MAX_CYCLE_BUTTON_STATES) {
        return NULL;
    }

    int index = button->state_index;
    if (index < 0) {
        index = 0;
    }
    if (index >= button->state_count) {
        index = button->state_count - 1;
    }
    return &button->states[index];
}

static color_t cycling_button_color_for_style(cycling_button_style style)
{
    // Cycling buttons inherit color from state, but this selector allows future style variations
    switch (style) {
        case CYCLING_BUTTON_STYLE_GRAY:
        default:
            return COLOR_MASK_NONE;
    }
}

static font_t cycling_button_font_for_style(cycling_button_style style)
{
    // Cycling buttons inherit font from state, but this selector allows future style variations
    switch (style) {
        case CYCLING_BUTTON_STYLE_GRAY:
            return FONT_NORMAL_GREEN;
        default:
            return FONT_NORMAL_BLACK;
    }
}

#pragma endregion Helpers
#pragma region Drawing

void cycling_button_draw(const cycling_button *button)
{
    switch (button->style) {
        case CYCLING_BUTTON_STYLE_GRAY:
            cycling_button_draw_gray_style(button);
            break;
        default:
            cycling_button_draw_default_style(button);
    }
}

void cycling_button_draw_array(const cycling_button *buttons, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        cycling_button_draw(&buttons[i]);
    }
}

static void cycling_button_draw_default_style(const cycling_button *button)
{
    if (!button) {
        return;
    }

    const cycling_button_state *state = cycling_button_get_state(button);
    if (!state) {
        return;
    }

    font_t font = state->font ? state->font : cycling_button_font_for_style(button->style);

    graphics_set_clip_rectangle(button->x, button->y, button->width, button->height);

    if (button->fill_bg) {
        unbordered_panel_draw_px(button->x, button->y, button->width, button->height);
    }

    draw_cycling_button_contents(button, state, font);
    if (button->style != CYCLING_BUTTON_STYLE_RAW) {
        button_border_draw(button->x, button->y, button->width, button->height, button->is_hovered);
    }
    graphics_reset_clip_rectangle();
}

static void cycling_button_draw_gray_style(const cycling_button *button)
{
    if (!button) {
        return;
    }

    const cycling_button_state *state = cycling_button_get_state(button);
    if (!state) {
        return;
    }

    font_t font = state->font ? state->font : cycling_button_font_for_style(button->style);
    if (button->style != CYCLING_BUTTON_STYLE_RAW) {
        large_label_draw_bg(button->x, button->y, button->width, button->height);
    }
    if (button->is_hovered) {
        graphics_shade_rect(button->x, button->y, button->width, button->height, 2);
    }
    draw_cycling_button_contents(button, state, font);
    if (button->style != CYCLING_BUTTON_STYLE_RAW) {
        large_label_draw_border(button->x, button->y, button->width, button->height);
    }
}

static void draw_cycling_button_contents(const cycling_button *button, const cycling_button_state *state, font_t font)
{
    const int inner_margin = 2;
    const color_t text_color = state->color_mask ? state->color_mask : cycling_button_color_for_style(button->style);
    const color_t image_color = COLOR_MASK_NONE;

    int img_before_w = 0;
    int img_after_w = 0;
    int img_before_margin_x = inner_margin;
    const image *img_before = NULL;
    const image *img_after = NULL;

    if (state->image_before > 0) {
        img_before = image_get(state->image_before);
        if (img_before->original.width >= button->width) {
            img_before_margin_x = 0;
        }
        img_before_w = img_before->original.width + img_before_margin_x;
    }
    if (state->image_after > 0) {
        img_after = image_get(state->image_after);
        img_after_w = img_after->original.width + inner_margin;
    }

    int text_max_width = button->width - 2 * inner_margin - img_before_w - img_after_w;
    if (text_max_width < 0) {
        text_max_width = 0;
    }

    int seq_width = 0;
    if (state->sequence.fragments && state->sequence.count > 0) {
        seq_width = lang_seq_get_width(&state->sequence, font);
    }
    int visible_seq_width = seq_width < text_max_width ? seq_width : text_max_width;

    int total_width = img_before_w + visible_seq_width + img_after_w;
    int cursor_x = button->x + (button->width - total_width) / 2;
    if (cursor_x < button->x + inner_margin) {
        cursor_x = button->x + inner_margin;
    }
    int text_y = button->y + (button->height - font_definition_for(font)->line_height) / 2;

    if (img_before) {
        int img_x = img_before->original.width >= button->width ? button->x : cursor_x;
        int img_y = img_before->original.height >= button->height
            ? button->y
            : button->y + (button->height - img_before->original.height) / 2;
        image_draw(state->image_before, img_x, img_y, image_color, SCALE_NONE);
        cursor_x += img_before_w;
    }

    if (state->sequence.fragments && state->sequence.count > 0) {
        cursor_x += lang_seq_draw_ellipsized(&state->sequence, cursor_x, text_y, text_max_width, font, text_color, 0);
    }

    if (img_after) {
        int img_y = button->y + (button->height - img_after->original.height) / 2;
        image_draw(state->image_after, cursor_x + inner_margin, img_y, image_color, SCALE_NONE);
    }
}

#pragma endregion Drawing
#pragma region Input Handling

int cycling_button_handle_mouse(cycling_button *btn, const mouse *m)
{
    if (!btn) {
        return 0;
    }

    int inside = (m->x >= btn->x && m->x < btn->x + btn->width && m->y >= btn->y && m->y < btn->y + btn->height);
    if (btn->is_hovered != inside) {
        window_request_refresh();
    }
    btn->is_hovered = inside;

    if (inside && btn->hover_handler) {
        btn->hover_handler(btn);
    }

    int handled = 0;

    if (inside && m->left.went_up) {
        sound_effect_play(SOUND_EFFECT_ICON);
        if (btn->state_count > 0) {
            btn->state_index = (btn->state_index + 1) % btn->state_count;
            window_request_refresh();
        }
        if (btn->left_click_handler) {
            btn->left_click_handler(btn);
        }
        handled = 1;
    }

    if (inside && m->right.went_up) {
        if (btn->state_count > 0) {
            btn->state_index = (btn->state_index + btn->state_count - 1) % btn->state_count;
            window_request_refresh();
        }
        if (btn->right_click_handler) {
            btn->right_click_handler(btn);
        }
        handled = 1;
    }

    return handled;
}

int cycling_button_handle_mouse_array(cycling_button *buttons, const mouse *m, unsigned int num_buttons)
{
    int handled = 0;

    for (unsigned int i = 0; i < num_buttons; i++) {
        if (cycling_button_handle_mouse(&buttons[i], m)) {
            handled = 1;
        }
    }

    return handled;
}

#pragma endregion Input Handling
#pragma region Tooltip

int cycling_button_handle_tooltip(const cycling_button *button, tooltip_context *c)
{
    if (!button || !c || !button->is_hovered) {
        return 0;
    }

    const cycling_button_state *state = cycling_button_get_state(button);
    if (!state) {
        return 0;
    }

    tooltip_copy_context(c, &state->tooltip_c);
    c->type = TOOLTIP_BUTTON; // constant - for all buttons.
    return 1;
}

int cycling_button_handle_tooltip_array(const cycling_button *buttons, tooltip_context *c, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        if (cycling_button_handle_tooltip(&buttons[i], c)) {
            return 1;
        }
    }
    return 0;
}

#pragma endregion Tooltip
