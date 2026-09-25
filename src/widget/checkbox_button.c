#include "widget/checkbox_button.h"

#include "graphics/button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/tooltip.h"
#include "graphics/window.h"
#include "sound/effect.h"

#include <stdint.h>
#include <stddef.h>

#pragma region State

int checkbox_button_is_checked(const checkbox_button *button)
{
    return button->is_checked;
}

int checkbox_button_check(checkbox_button *button)
{
    int changed_state = button->is_checked == 0;
    button->is_checked = 1;
    return changed_state;
}

int checkbox_button_uncheck(checkbox_button *button)
{
    int changed_state = button->is_checked == 1;
    button->is_checked = 0;
    return changed_state;
}

int checkbox_button_toggle(checkbox_button *button)
{
    button->is_checked = !button->is_checked;
    return button->is_checked;
}

#pragma endregion State
#pragma region Drawing

void checkbox_button_draw(const checkbox_button *button)
{
    if (!button) {
        return;
    }

    const int spacing = 6;
    font_t font = button->font ? button->font : FONT_NORMAL_BROWN;
    color_t text_color = button->color_mask ? button->color_mask : COLOR_MASK_NONE;
    color_t image_color = COLOR_MASK_NONE;

    int box_size = button->height;
    if (box_size < 12) {
        box_size = 12;
    }
    if (box_size > button->width) {
        box_size = button->width;
    }

    int box_x = button->box_on_right ? button->x + button->width - box_size : button->x;
    int box_y = button->y;
    int content_x = button->box_on_right ? button->x : box_x + box_size + spacing;
    int content_width = button->width - box_size - spacing;
    if (content_width < 0) {
        content_width = 0;
    }

    const image *img_before = NULL;
    const image *img_after = NULL;
    int img_before_w = 0;
    int img_after_w = 0;

    if (button->image_before > 0) {
        img_before = image_get(button->image_before);
        img_before_w = img_before->width + spacing;
    }
    if (button->image_after > 0) {
        img_after = image_get(button->image_after);
        img_after_w = img_after->width + spacing;
    }

    graphics_set_clip_rectangle(button->x, button->y, button->width, button->height);
    if (button->fill_bg) {
        unbordered_panel_draw_px(button->x, button->y, button->width, button->height);
    }
    button_border_draw(box_x, box_y, box_size, box_size, button->is_hovered);
    if (button->is_checked) {
        int mark_x = box_x + (box_size * 6) / 20;
        int mark_y = box_y + (box_size * 3) / 20;
        text_draw((const uint8_t *) "x", mark_x, mark_y, FONT_NORMAL_BROWN, COLOR_MASK_NONE);
    }

    int cursor_x = content_x;
    if (img_before) {
        int img_y = button->y + (button->height - img_before->height) / 2;
        image_draw(button->image_before, cursor_x, img_y, image_color, SCALE_NONE);
        cursor_x += img_before_w;
    }

    int text_y = button->y + (button->height - font_definition_for(font)->line_height) / 2;
    int max_text_width = content_width - img_before_w - img_after_w;
    if (max_text_width < 0) {
        max_text_width = 0;
    }
    int was_ellipsized = 0;
    if (button->sequence.fragments && button->sequence.count > 0) {
        cursor_x += lang_seq_draw_ellipsized(&button->sequence, cursor_x, text_y, max_text_width, font, text_color,
            &was_ellipsized);
    }
    ((checkbox_button *) button)->is_ellipsized = was_ellipsized;

    if (img_after) {
        int img_y = button->y + (button->height - img_after->height) / 2;
        image_draw(button->image_after, cursor_x, img_y, image_color, SCALE_NONE);
    }

    graphics_reset_clip_rectangle();
}

void checkbox_button_draw_array(const checkbox_button *buttons, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        checkbox_button_draw(&buttons[i]);
    }
}

#pragma endregion Drawing
#pragma region Input Handling

int checkbox_button_handle_mouse(checkbox_button *btn, const mouse *m)
{
    if (!btn) {
        return 0;
    }

    int inside = (m->x >= btn->x && m->x < btn->x + btn->width && m->y >= btn->y && m->y < btn->y + btn->height);
    if (btn->is_hovered != inside) {
        window_request_refresh();
    }
    btn->is_hovered = inside;

    if (btn->is_ellipsized && btn->is_hovered) {
        static uint8_t tooltip_text[512];
        lang_seq_concatenate(&btn->sequence, tooltip_text, 512);
        btn->tooltip_c.type = TOOLTIP_BUTTON;
        btn->tooltip_c.precomposed_text = tooltip_text;
    }

    if (inside && btn->hover_handler) {
        btn->hover_handler(btn);
    }

    if (inside && m->left.went_up) {
        sound_effect_play(SOUND_EFFECT_ICON);
        btn->is_checked = !btn->is_checked;
        if (btn->left_click_handler) {
            btn->left_click_handler(btn);
        }
        return 1;
    }

    return 0;
}

int checkbox_button_handle_mouse_array(checkbox_button *buttons, const mouse *m, unsigned int num_buttons)
{
    int handled = 0;

    for (unsigned int i = 0; i < num_buttons; i++) {
        if (checkbox_button_handle_mouse(&buttons[i], m)) {
            handled = 1;
        }
    }

    return handled;
}

#pragma endregion Input Handling
#pragma region Tooltip

int checkbox_button_handle_tooltip(const checkbox_button *button, tooltip_context *c)
{
    if (button->is_hovered) {
        tooltip_copy_context(c, &button->tooltip_c);
        return 1;
    }
    return 0;
}

int checkbox_button_handle_tooltip_array(const checkbox_button *buttons, tooltip_context *c, unsigned int num_buttons)
{
    for (unsigned int i = 0; i < num_buttons; i++) {
        if (checkbox_button_handle_tooltip(&buttons[i], c)) {
            return 1;
        }
    }
    return 0;
}

#pragma endregion Tooltip
