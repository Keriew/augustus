#include "dropdown_button.h"
#include "graphics/font.h"
#include "graphics/lang_text.h"
#include "graphics/window.h"
#include "graphics/tooltip.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

static int calculate_text_width(const complex_button *btn, font_t font)
{
    if (!btn->sequence || btn->sequence_size == 0) {
        return 0;
    }
    lang_sequence sequence;
    lang_seq_init(&sequence, (lang_fragment *) btn->sequence, btn->sequence_size);
    return lang_seq_get_width(&sequence, font);
}

static complex_button_style dropdown_button_style_to_complex_style(dropdown_button_style style)
{
    // Dropdown button can't have no fill - otherwise the options buttons would break.
    // In order to continue using complex button styles in init_simple, simple mapping:
    switch (style) {
        case DD_BUTTON_STYLE_DEFAULT:
            return COMPLEX_BUTTON_STYLE_DEFAULT;
        case DD_BUTTON_STYLE_DEFAULT_SMALL:
            return COMPLEX_BUTTON_STYLE_DEFAULT_SMALL;
        case DD_BUTTON_STYLE_GRAY:
            return COMPLEX_BUTTON_STYLE_GRAY;
        default:
            return COMPLEX_BUTTON_STYLE_DEFAULT;
    }
}

static unsigned int get_option_visual_slot(const dropdown_button *dd, unsigned int option_index)
{
    if (!dd || option_index == 0 || option_index >= dd->num_buttons) {
        return 0;
    }

    if (dd->reverse_order) {
        return dd->num_buttons - option_index - 1;
    }
    return option_index - 1;
}

static int get_option_y(const dropdown_button *dd, const complex_button *origin, unsigned int option_index)
{
    unsigned int slot = get_option_visual_slot(dd, option_index);
    int stride = dd->calculated_height + dd->spacing;

    if (dd->drop_up) {
        return origin->y - dd->calculated_height - (int) slot * stride;
    }
    return origin->y + origin->height + (int) slot * stride;
}

/* --- Helper to set anchor visual parameters to match selected option --- */
static void update_anchor(dropdown_button *dd)
{
    int index = dd->selected_index;
    if (!dd || dd->num_buttons == 0 || index < 1 || index >= (int) dd->num_buttons) {
        return;
    }

    complex_button *anchor = &dd->buttons[0];
    const complex_button *selected = &dd->buttons[index];

    // Copy visual parameters from selected option to anchor
    anchor->sequence = selected->sequence;
    anchor->sequence_size = selected->sequence_size;
    anchor->sequence_position = selected->sequence_position;
    anchor->image_before = selected->image_before;
    anchor->image_after = selected->image_after;
    anchor->color_mask = selected->color_mask;
    anchor->font = selected->font;
    anchor->style = selected->style;
    if (selected->tooltip_c.type) { // only copy tooltip to anchor if the selected option has a valid tooltip
        tooltip_copy_context(&anchor->tooltip_c, &selected->tooltip_c);
    }

}

static void save_anchor(dropdown_button *dd)
{
    if (!dd || dd->num_buttons == 0) {
        return;
    }
    complex_button *anchor_og = &dd->anchor_backup;
    complex_button *anchor = &dd->buttons[0];
    memcpy(anchor_og, anchor, sizeof(complex_button));
    // might have to do the assignment 1 by 1 
}

static void restore_anchor(dropdown_button *dd)
{
    // don't use memcpy on the entire button - it will copy state, flags, and bug the dropdown.
    if (!dd || dd->num_buttons == 0) {
        return;
    }
    // copy only the visual parameters
    complex_button *anchor_og = &dd->anchor_backup;
    complex_button *anchor = &dd->buttons[0];
    anchor->sequence = anchor_og->sequence;
    anchor->sequence_size = anchor_og->sequence_size;
    anchor->sequence_position = anchor_og->sequence_position;
    anchor->image_before = anchor_og->image_before;
    anchor->image_after = anchor_og->image_after;
    anchor->color_mask = anchor_og->color_mask;
    anchor->font = anchor_og->font;
    anchor->style = anchor_og->style;
    tooltip_copy_context(&anchor->tooltip_c, &anchor_og->tooltip_c);
}

void dropdown_button_advanced_update_anchor(dropdown_button *dd)
{
    update_anchor(dd); // expose the internal function for non-simple init
}

void dropdown_button_advanced_restore_anchor(dropdown_button *dd)
{
    restore_anchor(dd); // expose the internal function for non-simple init
}

void dropdown_button_advanced_save_anchor(dropdown_button *dd)
{
    save_anchor(dd); // expose the internal function for non-simple init
}

/* --- Default left click handler for dropdown options --- */
void dropdown_button_default_option_click(complex_button *btn)
{
    dropdown_button *dd = (dropdown_button *) btn->user_data;
    dd->selected_value = btn->parameters[0]; // free value carrier
    dd->expanded = !dd->expanded;

    // Update anchor visual parameters to match selected option
    update_anchor(dd);
}

void dropdown_button_default_origin_click(complex_button *btn)
{
    dropdown_button *dd = (dropdown_button *) btn->user_data;
    dd->expanded = !dd->expanded;
    window_request_refresh();
}

static void dropdown_cancel(complex_button *btn)
{
    dropdown_button *dd = (dropdown_button *) btn->user_data;
    dd->expanded = 0;
    window_request_refresh();
}

void dropdown_button_init(dropdown_button *dd, complex_button *buttons,
    unsigned int num_buttons, int width, int height, int spacing, int padding)
{
    memcpy(dd->buttons, buttons, sizeof(complex_button) * num_buttons);
    //dd->buttons = buttons;
    dd->num_buttons = num_buttons;
    dd->expanded = 0;
    dd->selected_index = -1;

    dd->height = height;
    dd->width = width;
    dd->spacing = spacing;
    dd->padding = padding;

    if (num_buttons == 0) {
        dd->calculated_width = 0;
        dd->calculated_height = 0;
        return;
    }

    // Use origin's geometry as anchor
    complex_button *origin = &dd->buttons[0];

    // --- Determine width ---
    int calc_width = width;
    if (calc_width == 0) { // if width not given - determine from longest text
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
    // --- Determine height ---
    dd->calculated_width = calc_width;
    dd->calculated_height = height ? height : origin->height;
    // --- Apply geometry ---
    origin->width = calc_width;
    for (unsigned int i = 1; i < num_buttons; i++) {
        dd->buttons[i].x = origin->x;
        dd->buttons[i].y = get_option_y(dd, origin, i);
        dd->buttons[i].width = calc_width;
        dd->buttons[i].height = dd->calculated_height;
    }
}

void dropdown_button_init_simple(int x, int y, int width, int height, const lang_fragment *frags, unsigned int count,
     dropdown_button *dd, dropdown_button_style dd_style, tooltip_context *origin_tooltip)
{
    if (count == 0 || count > DROPDOWN_BUTTON_MAX_COUNT) {
        memset(dd, 0, sizeof(*dd));
        return;
    }
    dd->num_buttons = count;
    dd->expanded = 0;
    dd->selected_index = dd->selected_index > 0 ? dd->selected_index : 0; // show the sequence of the origin by default
    dd->selected_value = -1;
    int buttons_width = width ? width : 0;
    dd->spacing = 2;
    dd->padding = 10; // TODO: Check why the width calculation downstream doesnt change with padding change
    complex_button_style style = dropdown_button_style_to_complex_style(dd_style);
    font_t style_font = complex_button_font_for_style(style); // ensure font is set for style
    // Setup origin (button 0)
    complex_button *origin = &dd->buttons[0];
    origin->x = x;
    origin->y = y;
    origin->height = height ? height : font_definition_for(style_font)->line_height + 8;
    origin->width = buttons_width;
    origin->style = style;
    origin->is_hidden = 0;
    origin->is_disabled = 0;
    int has_selection = dd->selected_index > 0;
    origin->sequence = &frags[has_selection ? dd->selected_index : 0];
    origin->sequence_position = SEQUENCE_POSITION_CENTER;
    origin->sequence_size = 1;
    origin->left_click_handler = dropdown_button_default_origin_click;
    origin->user_data = dd; // pointer to parent
    if (origin_tooltip) {
        tooltip_copy_context(&origin->tooltip_c, origin_tooltip);
    }
    save_anchor(dd); // store original anchor for restoring

    // Setup options [1..count-1]
    for (unsigned int i = 1; i < count; i++) {
        complex_button *opt = &dd->buttons[i];
        opt->style = style;
        opt->is_hidden = 0;
        opt->is_disabled = 0;
        opt->sequence = &frags[i];
        opt->sequence_size = 1;
        opt->sequence_position = SEQUENCE_POSITION_CENTER;

        // store backref to dropdown + index + value
        opt->user_data = dd; // pointer to parent 
        opt->parameters[0] = i;    // keep index in int slot
        opt->parameters[1] = i;    // default "value" = index, can override
        opt->left_click_handler = dropdown_button_default_option_click;
        opt->right_click_handler = dropdown_cancel;
    }

    // Finalize layout
    dropdown_button_init(dd, dd->buttons, count, buttons_width, dd->height, dd->spacing, dd->padding);
}

void dropdown_button_update_dimensions(int x, int y, int width, int height, dropdown_button *dd)
{
    if (!dd || dd->num_buttons == 0) {
        return;
    }

    complex_button *origin = &dd->buttons[0];

    // Geometry-only updates: keep layout config/state untouched.
    const int new_width = width > 0 ? width : (dd->calculated_width > 0 ? dd->calculated_width : origin->width);
    const int new_height = height > 0 ? height : (dd->calculated_height > 0 ? dd->calculated_height : origin->height);

    origin->x = x;
    origin->y = y;
    origin->width = new_width;
    origin->height = new_height;

    dd->calculated_width = new_width;
    dd->calculated_height = new_height;

    for (unsigned int i = 1; i < dd->num_buttons; i++) {
        complex_button *opt = &dd->buttons[i];
        opt->x = x;
        opt->y = get_option_y(dd, origin, i);
        opt->width = new_width;
        opt->height = new_height;
    }
}

int dropdown_button_handle_tooltip(const dropdown_button *dd, tooltip_context *c)
{
    if (!dd || dd->num_buttons == 0) {
        return 0;
    }
    return complex_button_handle_tooltip_array(dd->buttons, c, dd->num_buttons);
}

int dropdown_button_handle_tooltip_array(const dropdown_button *dds, tooltip_context *c, unsigned int num_dropdowns)
{
    if (!dds || !c) {
        return 0;
    }

    for (unsigned int i = 0; i < num_dropdowns; i++) {
        if (dropdown_button_handle_tooltip(&dds[i], c)) {
            return 1;
        }
    }
    return 0;
}

void dropdown_button_draw(const dropdown_button *dd)
{
    if (dd->num_buttons == 0) {
        return;
    }
    if (dd->show_origin && dd->expanded) {
        restore_anchor((dropdown_button *) dd);
    } else {
        update_anchor((dropdown_button *) dd); // cast away const to update anchor
    }
    // Draw options if expanded
    complex_button_draw(&dd->buttons[0]); // always draw origin
    if (dd->expanded) {
        // if expanded, make anchor show the default instead of currently selected index
        for (unsigned int i = 1; i < dd->num_buttons; i++) {
            complex_button_draw(&dd->buttons[i]);
        }
    }
}

void dropdown_button_draw_array(const dropdown_button *dds, unsigned int num_dropdowns)
{
    if (!dds) {
        return;
    }

    for (unsigned int i = 0; i < num_dropdowns; i++) {
        dropdown_button_draw(&dds[i]);
    }
}

static void unfocus_all(dropdown_button *dd)
{
    for (unsigned int i = 0; i < dd->num_buttons; i++) {
        dd->buttons[i].is_focused = 0;
    }
}

int dropdown_button_handle_mouse(dropdown_button *dd, const mouse *m)
{
    int handled = 0; // indicator if returning 1 - means rest of the input handling should stop
    if (!dd || dd->num_buttons == 0) {
        return 0;
    }

    if (complex_button_handle_mouse(&dd->buttons[0], m)) {    // Handle origin
        handled = 1;
        window_request_refresh();
        return handled; // don't process options on same click
    }

    // Handle options if expanded
    if (dd->expanded) {
        handled = 1; // if expanded, swallow all mouse input.
        if (!dd->rightclick_expanded_callback && m->right.went_up) {
            dd->expanded = 0;
            window_request_refresh();
            return handled; // collapse on any rightclick if no callback
        }
        for (unsigned int i = 1; i < dd->num_buttons; i++) { //handle option buttons
            if (complex_button_handle_mouse(&dd->buttons[i], m)) {
                dd->expanded = 0; // collapse
                dd->selected_index = i; // This is the best place to set selected_index
                if (dd->selected_callback && i) { // activate the callback if dropdown state changed. 
                    dd->selected_callback((dropdown_button *) dd); // pass dd as parameter, with selected index set
                }
                unfocus_all(dd); // remove focus from all buttons so the tooltip functions
                window_request_refresh();
                return handled;
            }
        }
        if (m->right.went_up) { // handle rightclick callback if set
            if (dd->rightclick_expanded_callback) {
                dd->rightclick_expanded_callback((dropdown_button *) dd);
                dd->expanded = 0; // collapse
                unfocus_all(dd); // remove focus from all buttons so the tooltip functions
                window_request_refresh();
                return handled;
            }
        }
        if (m->left.went_up) { // collapse if clicked outside
            dd->expanded = 0;
            unfocus_all(dd); // remove focus from all buttons so the tooltip functions
            window_request_refresh();
        }
    }

    return handled;
}

int dropdown_button_handle_mouse_array(dropdown_button *dds, const mouse *m, unsigned int num_dropdowns)
{
    if (!dds || !m) {
        return 0;
    }

    for (unsigned int i = 0; i < num_dropdowns; i++) {
        if (dds[i].expanded) {
            dropdown_button_handle_mouse(&dds[i], m);
            return 1;
        }
    }

    for (unsigned int i = 0; i < num_dropdowns; i++) {
        if (dropdown_button_handle_mouse(&dds[i], m)) {
            return 1;
        }
    }

    return 0;
}

int dropdown_button_get_x_min(dropdown_button *dd)
{
    if (!dd || dd->num_buttons == 0) {
        return 0;
    }
    return dd->buttons[0].x;
}

int dropdown_button_get_x_max(dropdown_button *dd)
{
    if (!dd || dd->num_buttons == 0) {
        return 0;
    }
    return dd->buttons[0].x + dd->calculated_width;
}

int dropdown_button_get_width(dropdown_button *dd)
{
    if (!dd || dd->num_buttons == 0) {
        return 0;
    }
    return dd->calculated_width;
}
