#include "tab_view.h"

#include "graphics/button.h"
#include "graphics/complex_button.h"
#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "graphics/window.h"
#include "input/mouse.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/*
EDIT: not a bad idea tbh but adjusting all exisintg windows to use the styles might be a mammoth excercise.
Won't kill us to define a few styles for larger structures, to make sure they use consistent:
Backgroud drawing, colours, fonts, sizes, titles, etc. It already exists for windows, right? <-reserach this

Establishing blanket templates for all these will make creating new UIs very easy and consistent.
Mixing these basic 3-4 styles from basegame into different widgets should provide an experience varied enough.

Very base style of the tab_view should pull from the visuals of the tabs in settings - that's the best example of
that visual design. There are some quirks in there that I'd like to get rid of though, like changing tab width.

Other parameters, properties to consider adding to tab_view:
>Tabs of equal width? yes/no
>if no, force max/min width?
>behaviour if the text doesn't fit in the button - make sure complex_button manages this on it's own.
>how do we treat scrollbar? I think best solution is to add the dimensions of content area to tab's structure.
 Then, the tab can decide on it's own if the scrollbar is required and where it should go.
 Set tab_view's 'master' content_area's dimensions - that's the window in which you'll be viewing content.
 If the actual content's dimensions exceed that - add scrollbars and pif paf done.
>tab buttons indication of active tab. My favourite and the default style should be removing botton border to
 make the tab button blend with the content area, like in settings.
Other considerations should be style-specific I reckon. Next time, continue iterating on the main drawing style for the
tab_view, ensure that the test trade ledger looks good and proceed with specifics in the trade_ledger itself.

Next iteration - FINISH tab_view as a structure!!

22/07 notes:
Needs tooltip handling for buttons and content. Might be a bit tricky.
UI API standrardisation mentioned above - should be done in a separate PR.
*/

#define TAB_VIEW_MIN_TAB_WIDTH 50
#define TAB_VIEW_MIN_WIDTH 200 
#define TAB_VIEW_MIN_HEIGHT 200 
#define TAB_DEFAULT_MARGIN 10


#define CUSTOM_CLR1 0xffa98462  // just for debugging to test multipel colors
#define CUSTOM_CLR2 0xffb7a08a  // muted taupe brown
// arbitrary minimum sizes to prevent weird displays

static complex_button_style button_style_for_tab_style(tab_view_style style)
{
    switch (style) {
        case TAB_VIEW_STYLE_GRAY:
            return COMPLEX_BUTTON_STYLE_GRAY;
        case TAB_VIEW_STYLE_DEFAULT_SMALL:
            return COMPLEX_BUTTON_STYLE_DEFAULT_SMALL;
        default:
            return COMPLEX_BUTTON_STYLE_DEFAULT;
    }
}

static font_t button_font_for_tab_style(tab_view_style style)
{
    switch (style) {
        case TAB_VIEW_STYLE_DEFAULT_SMALL:
            return FONT_SMALL_PLAIN;
        case TAB_VIEW_STYLE_WOOD:
        case TAB_VIEW_STYLE_GRAY:
        case TAB_VIEW_STYLE_COLORFUL:
        case TAB_VIEW_STYLE_DEFAULT:
        default:
            return FONT_NORMAL_BLACK;
    }
}

static color_t color_for_active_tab_button(tab_view_style style, int is_active)
{

    switch (style) {
        case TAB_VIEW_STYLE_COLORFUL:
            return is_active ? COLOR_MASK_NONE : COLOR_MASK_PASTEL_GRAY;
        case TAB_VIEW_STYLE_WOOD:
            return COLOR_MASK_PASTEL_BROWN2; // consistent brown for all tabs in wood mode
        case TAB_VIEW_STYLE_DEFAULT:
        case TAB_VIEW_STYLE_DEFAULT_SMALL:
        case TAB_VIEW_STYLE_GRAY:
        default:
            return 0; // using 0 instead of COLOR_MASK_NONE
    }
}

static color_t color_for_tab_background(tab_view_style style)
{
    switch (style) {
        case TAB_VIEW_STYLE_DEFAULT:
        case TAB_VIEW_STYLE_DEFAULT_SMALL:
            return COLOR_MASK_NONE; // transparent background for default styles
        case TAB_VIEW_STYLE_COLORFUL:
            return COLOR_MASK_PASTEL_TURQUOISE; // vibrant background for colorful mode
        case TAB_VIEW_STYLE_GRAY:
            return COLOR_MASK_PASTEL_GRAY; // consistent gray for gray mode
        case TAB_VIEW_STYLE_WOOD:
            return COLOR_MASK_PASTEL_BROWN2; // wood-like brown for wood style
        default:
            return COLOR_MASK_NONE; // fallback to transparent
    }

}

static void tab_click_handler(complex_button *btn)
{
    tab_view *view = (tab_view *) btn->user_data;
    if (!view) {
        return;
    }

    // Find which tab was clicked
    for (int i = 0; i < view->view_properties.count; i++) {
        if (&view->tabs[i].button == btn) {
            if (view->state.active_tab != i) {
                view->state.active_tab = i;
                window_request_refresh();
            }
            return;
        }
    }
}

void tab_view_init_simple(tab_view *view, int x, int y, int width, int height, int tab_count, tab_view_style style)
{
    if (!view || tab_count <= 0) { // protect from invalid parameters
        return;
    }

    memset(view, 0, sizeof(*view)); // prepare memory for view

    view->x = x;
    view->y = y;
    view->width = width > TAB_VIEW_MIN_WIDTH ? width : TAB_VIEW_MIN_WIDTH;
    view->height = height > TAB_VIEW_MIN_HEIGHT ? height : TAB_VIEW_MIN_HEIGHT;
    view->tab_height = style == TAB_VIEW_STYLE_DEFAULT_SMALL ? 20 : 26;

    view->view_properties.style = style;
    view->view_properties.position = TAB_POS_CENTER; // default position
    view->view_properties.count = tab_count;
    view->view_properties.spread = TAB_SPREAD_SMALL; // default spread
    view->state.active_tab = 0;

    view->tabs = calloc(tab_count, sizeof(tab)); // allocate memory for tabs

    if (!view->tabs) { // if memory allocation failed, clean up and return
        memset(view, 0, sizeof(*view));
        return;
    }

    // Initialize tab buttons with defaults
    for (int i = 0; i < tab_count; i++) {
        memset(&view->tabs[i].button, 0, sizeof(complex_button));
        view->tabs[i].button.left_click_handler = tab_click_handler;
        view->tabs[i].button.user_data = view;
        view->tabs[i].button.style = button_style_for_tab_style(style);
        view->tabs[i].button.font = button_font_for_tab_style(style);
        view->tabs[i].button.sequence_position = SEQUENCE_POSITION_CENTER;
        view->tabs[i].button.color_mask = 0; // default color mask, can be overridden later
        view->tabs[i].button.sequence_size = 1;
        view->tabs[i].visible = 1;
        view->tabs[i].enabled = 1;
    }
}

void tab_view_destroy(tab_view *view)
{
    if (!view) {
        return; // protect from null pointer
    }

    free(view->tabs); // deallocate tab array
    memset(view, 0, sizeof(*view)); // clear all fields
}

int tab_view_layout_singular(tab_view *view)
{
    // placeholder
    return TAB_LAYOUT_OK;
}

int tab_view_layout(tab_view *view)
{
    if (!view || !view->tabs || view->view_properties.count <= 0) {
        return TAB_ERR_NULL_VIEW;
    }
    if (view->view_properties.spread == TAB_SPREAD_MAX && view->view_properties.width_mode == TAB_WIDTH_MAX) {
        // These two modes are incompatible - if width is max, spread must be minimal!
        // to account for this, we prioritise width mode and set spread to next best thing - wide.
        view->view_properties.spread = TAB_SPREAD_WIDE;
    }

    if (view->view_properties.count == 1) {   // do a separate function for tab count 1
        return tab_view_layout_singular(view);// no need to bother with it as a standard case
    }
    int tab_count = view->view_properties.count;
    int sum_text_w = 0; // width of the text in all tab titles
    int single_gap = 0;
    int total_gap = 0;
    int available_for_tabs = 0;
    for (int i = 0; i < tab_count; i++) {
        if (!view->tabs[i].initialised) {
            return TAB_ERR_UNINITIALISED_TAB; // indicate layout was not successful due to uninitialised tabs
        }
        lang_sequence sequence;
        lang_seq_init(&sequence, (lang_fragment *) view->tabs[i].button.sequence, 1);
        sum_text_w += lang_seq_get_width(&sequence, view->view_properties.tab_font);
    }

    // === Step 1 - determine tab widths===
    if (view->view_properties.width_mode != TAB_WIDTH_CUSTOM) { // custom widths  = separate case

        if (view->view_properties.spread != TAB_SPREAD_MAX) {
            single_gap = view->view_properties.spread;
            total_gap = single_gap * (tab_count - 1);  // gaps only exist between tabs (count-1 gaps)
            available_for_tabs = view->width - total_gap;  // width available after accounting for gaps
        } else {
            total_gap = view->width - sum_text_w;
            if (view->view_properties.position == TAB_POS_CENTER) {
                // for center, gaps are also on left of the first tab and right of the last tab
                single_gap = total_gap / (tab_count + 1); // +1 for the extra gap on the right of the last tab
            } else {
                single_gap = total_gap / (tab_count - 1); // for left or right aligned, distribute gaps evenly
            }
            available_for_tabs = view->width - single_gap * (tab_count - 1);
            // there might be a remainder if total_gap is not perfectly divisible
            int remainder = abs(total_gap - single_gap * (tab_count - 1));
            // im literally only making it a variable so it can be easily seen in the debugger, dw about it
        }

        if (available_for_tabs < sum_text_w) {
            return TAB_ERR_INSUFFICIENT_WIDTH; // not enough space to fit all tab titles, layout fails
        }

    } else {
        // Fixed width for all tabs set via user_data. 
        int sum_tab_w = 0;
        for (int i = 0; i < tab_count; i++) {
            int custom_width = (int) (intptr_t) view->tabs[i].user_data; // user_data holds custom width
            sum_tab_w += custom_width;
            if (custom_width <= 0) {
                return TAB_ERR_CUSTOM_WIDTHS; // invalid custom width, layout fails
            }
        }
        if (sum_tab_w > view->width) {
            return TAB_ERR_INSUFFICIENT_WIDTH; // not enough space to fit all tabs with custom widths, layout fails
        }
    }
    // === Step 2 - position tabs ===

    int tab_y = view->y - view->tab_height + 4; // 4pixels down to 'sink' the tabs
    int tab_x = view->x;
    int total_tabs_w = 0;

    // First determine final button widths
    for (int i = 0; i < tab_count; i++) {
        lang_sequence sequence;
        lang_seq_init(&sequence, (lang_fragment *) view->tabs[i].button.sequence, 1);
        int text_w = lang_seq_get_width(&sequence, view->view_properties.tab_font);

        switch (view->view_properties.width_mode) {
            case TAB_WIDTH_MAX:
                view->tabs[i].button.width = available_for_tabs / tab_count;
                break;
            case TAB_WIDTH_TO_CONTENT:
                view->tabs[i].button.width = text_w + TAB_DEFAULT_MARGIN;
                break;

            case TAB_WIDTH_CUSTOM:
                view->tabs[i].button.width = (int) (intptr_t) view->tabs[i].user_data;
                break;

            case TAB_WIDTH_EQUAL:
            default:
                view->tabs[i].button.width = (sum_text_w / tab_count) + TAB_DEFAULT_MARGIN;
                break;
        }

        if (view->tabs[i].button.width < TAB_VIEW_MIN_TAB_WIDTH) {
            view->tabs[i].button.width = TAB_VIEW_MIN_TAB_WIDTH;
        }

        view->tabs[i].button.height = view->tab_height;
        total_tabs_w += view->tabs[i].button.width;
    }

    // Then determine the gap between tabs
    if (view->view_properties.spread == TAB_SPREAD_MAX) {
        single_gap = (view->width - total_tabs_w) / (tab_count - 1);
        if (single_gap < 0) {
            return TAB_ERR_INSUFFICIENT_WIDTH;
        }
    } else {
        single_gap = view->view_properties.spread;
    }

    int total_w = total_tabs_w + single_gap * (tab_count - 1);

    // Finally determine starting x based on alignment
    switch (view->view_properties.position) {
        case TAB_POS_RIGHT:
            tab_x = view->x + view->width - total_w;
            break;

        case TAB_POS_CENTER:
            tab_x = view->x + (view->width - total_w) / 2;
            break;

        case TAB_POS_LEFT:
        default:
            tab_x = view->x; //miniature offset to align with border of the content area
            break;
    }

    if (tab_x < view->x || tab_x + total_w > view->x + view->width) {
        return TAB_ERR_INSUFFICIENT_WIDTH;
    }

    // Apply final geometry to buttons
    for (int i = 0; i < tab_count; i++) {
        view->tabs[i].button.x = tab_x + (i == tab_count - 1) - (i == 0);// first pass -1, last pass +1. see note* below
        view->tabs[i].button.y = tab_y;
        view->tabs[i].button.color_mask = color_for_active_tab_button(
            view->view_properties.style,
            view->state.active_tab == i
        );

        tab_x += view->tabs[i].button.width + single_gap;
    }
    //*note - button borders are drawn 1 pixel to the right to account for red border for 'focused' state. 
    //in order to not rewrite the 15 lines of code that affect the entire codebase, adjusting the first and last only.

    // === Step 3 - content area ===
    view->content.x = view->x;
    view->content.y = view->y;
    view->content.width = view->width;
    view->content.height = view->height;

    return TAB_LAYOUT_OK; // layout successful
}

void tab_view_init_tab(tab_view *view, int tab_index, content_draw_callback callback, const lang_fragment *frag)
{
    if (!view || !view->tabs) {
        return;
    }

    if (tab_index < 0 || tab_index >= view->view_properties.count) {
        return;
    }

    view->tabs[tab_index].draw_callback = callback;
    view->tabs[tab_index].button.sequence = frag;
    view->tabs[tab_index].button.sequence_size = frag ? 1 : 0; // only one fragment per tab allowed in simple init
    // if you'd like to make a more complex tab title, you will need to set properties yourself
    view->tabs[tab_index].visible = 1;
    view->tabs[tab_index].enabled = 1;
    view->tabs[tab_index].initialised = 1;
}

void tab_view_set_tab_text(tab_view *view, int tab_index, const lang_fragment *frag)
{
    if (!view || !view->tabs || tab_index < 0 || tab_index >= view->view_properties.count) {
        return;
    }

    view->tabs[tab_index].button.sequence = frag;
    if (view->tabs[tab_index].draw_callback) {
        // if draw_callback is already set, the tab is initialised
        view->tabs[tab_index].initialised = 1;
    }
}

void tab_view_set_tab_draw_callback(tab_view *view, int tab_index, content_draw_callback callback)
{
    if (!view || !view->tabs || tab_index < 0 || tab_index >= view->view_properties.count) {
        return;
    }

    view->tabs[tab_index].draw_callback = callback;
    if (view->tabs[tab_index].button.sequence) {
        // if button.sequence is already set, the tab is initialised
        view->tabs[tab_index].initialised = 1;
    }
}

void tab_view_draw(tab_view *view)
{
    if (!view || !view->tabs || view->view_properties.count <= 0) {
        return;
    }

    // Draw all visible tab buttons BEFORE the content section
    for (int i = 0; i < view->view_properties.count; i++) {
        if (view->tabs[i].visible && i != view->state.active_tab) {
            complex_button_draw(&view->tabs[i].button);
        }
    }
    // Draw inner panel for content area (no outer border for tab_view itself)
    int red_content = view->tabs[view->state.active_tab].button.is_focused;
    color_t content_color = color_for_tab_background(view->view_properties.style);
    bordered_panel_draw_colored(view->content.x, view->content.y, view->content.width, view->content.height, red_content, content_color, content_color);
    // y+1 to ever so slightly lower the border 
    view->tabs[view->state.active_tab].button.flush_with_background = 1; // active tab flushes with background
    complex_button_draw(&view->tabs[view->state.active_tab].button); // draw active tab last so it looks flushed
    // Draw content for active tab
    int active_tab = view->state.active_tab;
    graphics_in_dialog_with_size(view->width, view->height);
    if (active_tab >= 0 && active_tab < view->view_properties.count) {
        tab *active = &view->tabs[active_tab];
        if (active->draw_callback) {
            active->draw_callback(view, active);
        }
    }
    graphics_reset_dialog();
}

int tab_view_handle_mouse(const mouse *m, tab_view *view)
{
    if (!m || !view || !view->tabs || view->view_properties.count <= 0) {
        return 0;
    }

    int handled = 0;

    // Handle mouse for all visible tabs
    for (int i = 0; i < view->view_properties.count; i++) {
        if (view->tabs[i].visible && view->tabs[i].enabled) {
            if (complex_button_handle_mouse(&view->tabs[i].button, m)) {
                handled = 1;
            }
        }
    }

    return handled;
}

static int tab_view_translate_mouse_to_content(const tab_view *view, const mouse *m, int source_width, int source_height,
    mouse *out_mouse)
{
    if (!m || !out_mouse) {
        return 0;
    }

    *out_mouse = *m;
    if (!view || view->width <= 0 || view->height <= 0) {
        return 0;
    }

    // Map mouse coordinates from source dialog space to tab content drawing space.
    out_mouse->x += (view->width - source_width) / 2;
    out_mouse->y += (view->height - source_height) / 2;
    return 1;
}

int tab_view_handle_content_mouse(const tab_view *view, const mouse *m, int source_width, int source_height,
    content_mouse_handler handler, void *user_data)
{
    mouse mapped_mouse;
    if (!handler || !tab_view_translate_mouse_to_content(view, m, source_width, source_height, &mapped_mouse)) {
        return 0;
    }
    return handler(&mapped_mouse, user_data);
}

int tab_view_get_active_tab(const tab_view *view)
{
    if (!view || view->view_properties.count <= 0) {
        return -1;
    }
    return view->state.active_tab;
}

void tab_view_set_active_tab(tab_view *view, int tab_index)
{
    if (!view || tab_index < 0 || tab_index >= view->view_properties.count) {
        return;
    }
    if (view->state.active_tab != tab_index) {
        view->state.active_tab = tab_index;
        tab_view_layout(view);
        window_request_refresh();
    }
}
