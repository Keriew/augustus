#include "dropdown_button.h"
#include "graphics/font.h"
#include "graphics/lang_text.h"
#include "graphics/window.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define DROPDOWN_BUTTON_MAX_COUNT 20 // arbitrary limit for static storage

static complex_button dropdown_button_storage[DROPDOWN_BUTTON_MAX_COUNT]; //buffer for simple init

static int calculate_text_width(const complex_button *btn, font_t font)
{
    if (!btn->sequence || btn->sequence_size == 0) {
        return 0;
    }
    return lang_text_get_sequence_width(btn->sequence, btn->sequence_size, font);
}

/* --- Default left click handler for dropdown options --- */
static void dropdown_option_click(const complex_button *btn)
{
    dropdown_button *dd = (dropdown_button *) btn->user_data;
    int index = btn->parameters[0];

    dd->selected_index = index;
    dd->selected_value = btn->parameters[1]; // free value carrier
    dd->expanded = 0;

    // Update origin text to match selected option
    dd->buttons[0].sequence = btn->sequence;
    dd->buttons[0].sequence_size = btn->sequence_size;
}

static void dropdown_cancel(const complex_button *btn)
{
    dropdown_button *dd = (dropdown_button *) btn->user_data;
    dd->expanded = 0;
    window_request_refresh();
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

void dropdown_button_init_simple(int x, int y, const lang_fragment *frags, unsigned int count, dropdown_button *dd)
{
    if (count == 0 || count > DROPDOWN_BUTTON_MAX_COUNT) {
        memset(dd, 0, sizeof(*dd));
        return;
    }

    dd->buttons = dropdown_button_storage;
    memset(dd->buttons, 0, sizeof(dropdown_button_storage));

    dd->num_buttons = count;
    dd->expanded = 0;
    dd->selected_index = 0; // show the sequence of the origin by default
    dd->selected_value = -1;
    int buttons_width = dd->width ? dd->width : 0;
    dd->spacing = 2;
    dd->padding = 10;

    // Setup origin (button 0)
    complex_button *origin = &dd->buttons[0];
    origin->x = x;
    origin->y = y;
    origin->height = font_definition_for(FONT_NORMAL_BLACK)->line_height + 6;
    origin->width = buttons_width;
    origin->style = COMPLEX_BUTTON_STYLE_DEFAULT;
    origin->is_hidden = 0;
    origin->is_disabled = 0;
    origin->sequence = &frags[0];
    origin->sequence_position = SEQUENCE_POSITION_CENTER;
    origin->sequence_size = 1;

    // Setup options [1..count-1]
    for (unsigned int i = 1; i < count; i++) {
        complex_button *opt = &dd->buttons[i];
        opt->style = COMPLEX_BUTTON_STYLE_DEFAULT;
        opt->is_hidden = 0;
        opt->is_disabled = 0;
        opt->sequence = &frags[i];
        opt->sequence_size = 1;
        opt->sequence_position = SEQUENCE_POSITION_CENTER;

        // store backref to dropdown + index + value
        opt->user_data = dd; // pointer to parent 
        opt->parameters[0] = i;    // keep index in int slot
        opt->parameters[1] = i;    // default "value" = index, can override
        opt->left_click_handler = dropdown_option_click;
        //opt->right_click_handler = dropdown_cancel;
    }

    // Finalize layout
    dropdown_button_init(dd, dd->buttons, count, buttons_width, dd->spacing, dd->padding);
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
        window_request_refresh();
        return handled; // don't process options on same click
    }

    // Handle options if expanded
    if (dd->expanded) {
        if (!dd->rightclick_expanded_callback && m->right.went_up) {
            dd->expanded = 0;
            window_request_refresh();
            handled = 1;
            return handled; // collapse on any rightclick if no callback
        }
        for (unsigned int i = 1; i < dd->num_buttons; i++) { //handle option buttons
            if (complex_button_handle_mouse(m, &dd->buttons[i])) {
                dd->selected_index = i;
                dd->expanded = 0; // collapse
                handled = 1;
                // activate the callback if dropdown state changes:
                if (dd->selected_callback) {
                    dd->selected_callback((dropdown_button *) dd); // pass origin button as parameter
                }
                window_request_refresh();
                return handled;
            }
        }
        if (m->right.went_up) { // handle rightclick callback if set
            if (dd->rightclick_expanded_callback) {
                dd->rightclick_expanded_callback((dropdown_button *) dd);
                handled = 1;
                window_request_refresh();
                return handled;
            }
        }
    }

    return handled;
}
