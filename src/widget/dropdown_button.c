#include "dropdown_button.h"
#include "graphics/font.h"
#include "graphics/lang_text.h"

#include <stddef.h>

static int calculate_text_width(const complex_button *btn, font_t font)
{
    if (!btn->sequence || btn->sequence_size == 0) {
        return 0;
    }
    return lang_text_get_sequence_width(btn->sequence, btn->sequence_size, font);
}

void dropdown_button_init(dropdown_button *dd, complex_button *buttons,
    unsigned int num_buttons, int width, int spacing, int padding)
{
    dd->buttons = buttons;
    dd->num_buttons = num_buttons;
    dd->expanded = 0;
    dd->selected_index = -1;

    dd->width = width;
    dd->spacing = spacing;
    dd->padding = padding;

    if (num_buttons == 0) {
        dd->calculated_width = 0;
        dd->calculated_height = 0;
        return;
    }

    // Use origin's geometry as anchor
    complex_button *origin = &buttons[0];

    // --- Determine width ---
    int calc_width = width;
    if (calc_width == 0) {
        const font_t font = FONT_NORMAL_BLACK;
        int max_text_width = 0;
        for (unsigned int i = 0; i < num_buttons; i++) {
            int tw = calculate_text_width(&buttons[i], font);
            if (tw > max_text_width) {
                max_text_width = tw;
            }
        }
        calc_width = max_text_width + 2 * padding;
        if (calc_width > DROPDOWN_BUTTON_MAX_WIDTH) {
            calc_width = DROPDOWN_BUTTON_MAX_WIDTH;
        }
    }
    dd->calculated_width = calc_width;

    // --- Determine height ---
    dd->calculated_height = origin->height;

    // --- Apply geometry ---
    origin->width = calc_width;
    for (unsigned int i = 1; i < num_buttons; i++) {
        buttons[i].x = origin->x;
        buttons[i].y = origin->y + origin->height + (i - 1) * (dd->calculated_height + spacing);
        buttons[i].width = calc_width;
        buttons[i].height = dd->calculated_height;
    }
}

void dropdown_button_draw(const dropdown_button *dd)
{
    if (dd->num_buttons == 0) {
        return;
    }

    // Always draw origin
    complex_button_draw(&dd->buttons[0]);

    // Draw options if expanded
    if (dd->expanded) {
        for (unsigned int i = 1; i < dd->num_buttons; i++) {
            complex_button_draw(&dd->buttons[i]);
        }
    }
}

int dropdown_button_handle_mouse(const mouse *m, dropdown_button *dd)
{
    if (dd->num_buttons == 0) {
        return 0;
    }

    int handled = 0;

    // Handle origin
    if (complex_button_handle_mouse(m, &dd->buttons[0])) {
        dd->expanded = !dd->expanded;
        handled = 1;
    }

    // Handle options if expanded
    if (dd->expanded) {
        for (unsigned int i = 1; i < dd->num_buttons; i++) {
            if (complex_button_handle_mouse(m, &dd->buttons[i])) {
                dd->selected_index = i;
                dd->expanded = 0; // collapse
                handled = 1;
            }
        }
    }

    return handled;
}
