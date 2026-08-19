#include "scrollbar.h"

#include "assets/assets.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "core/image_group.h"
#include "graphics/image.h"
#include "graphics/image_button.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/window.h"

#define BLOCK_SIZE 16
#define LEGACY_SCROLLBAR_UP_IMAGE_ID 8
#define LEGACY_SCROLLBAR_DOWN_IMAGE_ID 12

enum {
    TOUCH_DRAG_NONE = 0,
    TOUCH_DRAG_PENDING = 1,
    TOUCH_DRAG_IN_PROGRESS = 2
};

typedef enum scroll_element {
    SCROLL_UP_ARROW = 0,
    SCROLL_DOWN_ARROW = 1,
    SCROLL_DOT = 2,
    SCROLL_BG = 3
} scroll_element;

static scrollbar_type *current;

static void text_scroll(int is_down, int num_lines);

static int get_scrollbar_width(scroll_element element, int legacy)
{
    switch (element) {
        case SCROLL_UP_ARROW:
        case SCROLL_DOWN_ARROW:
            return legacy ? LEGACY_SCROLL_BUTTON_WIDTH : SCROLL_BUTTON_SIDE;
        case SCROLL_DOT:
            return legacy ? LEGACY_SCROLL_DOT_SIZE : SCROLL_BOX_WIDTH;
        case SCROLL_BG:
            return BLOCK_SIZE;
    }
    return 0; // default return value if no case matches
}

static int get_scrollbar_height(scroll_element element, int legacy)
{
    switch (element) {
        case SCROLL_UP_ARROW:
        case SCROLL_DOWN_ARROW:
            return legacy ? LEGACY_SCROLL_BUTTON_HEIGHT : SCROLL_BUTTON_SIDE;
        case SCROLL_DOT:
            return legacy ? LEGACY_SCROLL_DOT_SIZE : SCROLL_BOX_HEIGHT;
        case SCROLL_BG:
            return BLOCK_SIZE;
    }
    return 0; // default return value if no case matches
}

static int get_total_button_height(int legacy)
{
    if (legacy) {
        return LEGACY_TOTAL_SCROLL_BUTTON_HEIGHT;
    } else {
        return TOTAL_SCROLL_BUTTON_HEIGHT;
    }
}

static int get_scrollbar_image_id(scroll_element element, int legacy)
{
    if (element == SCROLL_UP_ARROW) {
        return legacy ? LEGACY_SCROLLBAR_UP_IMAGE_ID : assets_lookup_image_id(ASSET_UI_SCROLLBAR_UP);
    }
    if (element == SCROLL_DOWN_ARROW) {
        return legacy ? LEGACY_SCROLLBAR_DOWN_IMAGE_ID : assets_lookup_image_id(ASSET_UI_SCROLLBAR_DOWN);
    }
    if (element == SCROLL_DOT) {
        return legacy ? image_group(GROUP_PANEL_BUTTON) + 39 : assets_lookup_image_id(ASSET_UI_SCROLLBAR_MIDDLE);
    }
    return 0;
}

static int get_scrollbar_dot_offset(const scrollbar_type *scrollbar)
{
    int pct;
    if (scrollbar->scroll_position <= 0) {
        pct = 0;
    } else if (scrollbar->scroll_position >= scrollbar->max_scroll_position) {
        pct = 100;
    } else {
        pct = calc_percentage(scrollbar->scroll_position, scrollbar->max_scroll_position);
    }
    int offset = calc_adjust_with_percentage(
        scrollbar->height - get_total_button_height(scrollbar->legacy) - 2 * scrollbar->dot_padding, pct);
    if (scrollbar->is_dragging_scrollbar_dot) {
        offset = scrollbar->scrollbar_dot_drag_offset;
    }
    return offset;
}

static void position_scrollbar_dot_button(scrollbar_type *scrollbar)
{
    int scroll_btn_width = get_scrollbar_width(SCROLL_UP_ARROW, scrollbar->legacy);
    int scroll_btn_height = get_scrollbar_height(SCROLL_UP_ARROW, scrollbar->legacy);
    int scroll_dot_width = get_scrollbar_width(SCROLL_DOT, scrollbar->legacy);
    short x_offset = ((scroll_btn_width - scroll_dot_width) / 2);
    short y_offset = (short) (scroll_btn_height + scrollbar->dot_padding + get_scrollbar_dot_offset(scrollbar));
    scrollbar->image_button_scroll_dot.x_offset = x_offset;
    scrollbar->image_button_scroll_dot.y_offset = y_offset;
}

void scrollbar_init(scrollbar_type *scrollbar, unsigned int scroll_position, unsigned int total_elements)
{
    unsigned int max_scroll_position;
    if (total_elements <= scrollbar->elements_in_view) {
        max_scroll_position = 0;
    } else {
        max_scroll_position = total_elements - scrollbar->elements_in_view;
    }
    scrollbar->scroll_position = calc_bound(scroll_position, 0, max_scroll_position);
    scrollbar->max_scroll_position = max_scroll_position;
    scrollbar->is_dragging_scrollbar_dot = 0;
    scrollbar->touch_drag_state = TOUCH_DRAG_NONE;
    scrollbar->legacy = config_get(CONFIG_UI_SCROLL_LEGACY_SCROLLBAR); // save in struct to access everywhere

    int scrollup_id, scrolldown_id, scroll_dot_id, img_group, scroll_btn_width, scroll_btn_height, scroll_dot_width, scroll_dot_height;
    scrollup_id = get_scrollbar_image_id(SCROLL_UP_ARROW, scrollbar->legacy);
    scrolldown_id = get_scrollbar_image_id(SCROLL_DOWN_ARROW, scrollbar->legacy);
    scroll_dot_id = get_scrollbar_image_id(SCROLL_DOT, scrollbar->legacy);
    img_group = scrollbar->legacy ? GROUP_OK_CANCEL_SCROLL_BUTTONS : 0;
    scroll_btn_width = get_scrollbar_width(SCROLL_UP_ARROW, scrollbar->legacy);
    scroll_btn_height = get_scrollbar_height(SCROLL_UP_ARROW, scrollbar->legacy);
    scroll_dot_width = get_scrollbar_width(SCROLL_DOT, scrollbar->legacy);
    scroll_dot_height = get_scrollbar_height(SCROLL_DOT, scrollbar->legacy);


    scrollbar->image_button_scroll_up = (image_button) { 0, 0, scroll_btn_width, scroll_btn_height, IB_SCROLL,
        img_group, scrollup_id, text_scroll, button_none, 0, 1, 1 };
    scrollbar->image_button_scroll_down = (image_button) { 0, 0, scroll_btn_width, scroll_btn_height, IB_SCROLL,
        img_group, scrolldown_id, text_scroll, button_none, 1, 1, 1 };
    scrollbar->image_button_scroll_dot = (image_button) { 0, 0, scroll_dot_width, scroll_dot_height, IB_SCROLL,
        0, scroll_dot_id, button_none, button_none, 0, 0, 1 };
    if (scrollbar->legacy) {
        scrollbar->image_button_scroll_dot.static_image = 1; // disabled animation and hover effects
    }
}

void scrollbar_reset(scrollbar_type *scrollbar, unsigned int scroll_position)
{
    scrollbar->scroll_position = calc_bound(scroll_position, 0, scrollbar->max_scroll_position);
    scrollbar->is_dragging_scrollbar_dot = 0;
    scrollbar->touch_drag_state = TOUCH_DRAG_NONE;
}

void scrollbar_update_total_elements(scrollbar_type *scrollbar, unsigned int total_elements)
{
    unsigned int max_scroll_position;
    if (total_elements <= scrollbar->elements_in_view) {
        max_scroll_position = 0;
    } else {
        max_scroll_position = total_elements - scrollbar->elements_in_view;
    }
    scrollbar->max_scroll_position = max_scroll_position;
    if (scrollbar->scroll_position > max_scroll_position) {
        scrollbar->scroll_position = max_scroll_position;
    }
}

void scrollbar_draw(scrollbar_type *scrollbar)
{
    if (scrollbar->max_scroll_position > 0 || scrollbar->always_visible) {
        int scroll_btn_height = get_scrollbar_height(SCROLL_UP_ARROW, scrollbar->legacy);
        if (scrollbar->decorate_scrollbar) {
            if (scrollbar->legacy) {
                inner_panel_draw(scrollbar->x + 4, scrollbar->y + 2 * BLOCK_SIZE, 2, scrollbar->height / BLOCK_SIZE - 4);
            } else { // default
                scrollbar_panel_draw(scrollbar->x, scrollbar->y, scrollbar->height);
            }
        }
        image_buttons_draw(scrollbar->x, scrollbar->y, &scrollbar->image_button_scroll_up, 1);
        image_buttons_draw(scrollbar->x, scrollbar->y + scrollbar->height - scroll_btn_height,
            &scrollbar->image_button_scroll_down, 1);
        position_scrollbar_dot_button(scrollbar);
        image_buttons_draw(scrollbar->x, scrollbar->y, &scrollbar->image_button_scroll_dot, 1);
        window_invalidate();
    }
}

static int touch_inside_scrollable_area(const scrollbar_type *scrollbar, const touch *t, int in_dialog)
{
    int x = t->start_point.x;
    int y = t->start_point.y;
    if (in_dialog) {
        x -= screen_dialog_offset_x();
        y -= screen_dialog_offset_y();
    }
    return scrollbar->max_scroll_position > 0 &&
        x >= scrollbar->x - scrollbar->scrollable_width && x <= scrollbar->x - 2 &&
        y >= scrollbar->y && y < scrollbar->y + scrollbar->height;
}

static int handle_touch(scrollbar_type *scrollbar, const touch *t, int in_dialog)
{
    unsigned int old_position = scrollbar->scroll_position;
    int active = scrollbar->touch_drag_state == TOUCH_DRAG_IN_PROGRESS;

    if (t->has_started && touch_inside_scrollable_area(scrollbar, t, in_dialog)) {
        scrollbar->touch_drag_state = TOUCH_DRAG_PENDING;
        scrollbar->position_on_touch = scrollbar->scroll_position;
    }
    if (t->has_moved && scrollbar->touch_drag_state != TOUCH_DRAG_NONE) {
        scrollbar->touch_drag_state = TOUCH_DRAG_IN_PROGRESS;
        int element_height = (scrollbar->height - 8 * scrollbar->has_y_margin) / scrollbar->elements_in_view;
        int current_y = t->current_point.y - ((t->current_point.y - (scrollbar->y + 8 * scrollbar->has_y_margin)) % element_height);
        int start_y = t->start_point.y - ((t->start_point.y - (scrollbar->y + 8 * scrollbar->has_y_margin)) % element_height);
        int touch_scrolled = (current_y - start_y) / element_height;
        scrollbar->scroll_position = calc_bound(scrollbar->position_on_touch - touch_scrolled, 0, scrollbar->max_scroll_position);
        active = 1;
    }
    if (t->has_ended) {
        scrollbar->touch_drag_state = TOUCH_DRAG_NONE;
    }
    if (scrollbar->on_scroll_callback && old_position != scrollbar->scroll_position) {
        scrollbar->on_scroll_callback();
    }
    window_request_refresh();
    return active;
}

static int handle_scrollbar_dot(scrollbar_type *scrollbar, const mouse *m)
{
    if (scrollbar->max_scroll_position <= 0) {
        return 0;
    }

    position_scrollbar_dot_button(scrollbar);
    image_buttons_handle_mouse(m, scrollbar->x, scrollbar->y, &scrollbar->image_button_scroll_dot, 1, 0);

    if (m->left.went_down && scrollbar->image_button_scroll_dot.focused) {
        scrollbar->is_dragging_scrollbar_dot = 0;
        scrollbar->scrollbar_dot_drag_offset = 0;
        return 1;
    }

    if (!m->left.is_down) {
        scrollbar->is_dragging_scrollbar_dot = 0;
        return 0;
    }

    int scroll_btn_width = get_scrollbar_width(SCROLL_UP_ARROW, scrollbar->legacy);
    int scroll_btn_height = get_scrollbar_height(SCROLL_UP_ARROW, scrollbar->legacy);
    int scroll_dot_height = get_scrollbar_height(SCROLL_DOT, scrollbar->legacy);
    int track_height = scrollbar->height - get_total_button_height(scrollbar->legacy) - 2 * scrollbar->dot_padding;
    if (m->x < scrollbar->x || m->x >= scrollbar->x + scroll_btn_width) {
        return 0;
    }
    if (m->y < scrollbar->y + scroll_btn_height + scrollbar->dot_padding ||
        m->y > scrollbar->y + scrollbar->height - scroll_btn_height - scrollbar->dot_padding) {
        return 0;
    }
    int dot_offset = m->y - scrollbar->y - scroll_dot_height / 2 - scroll_btn_height;
    if (dot_offset < 0) {
        dot_offset = 0;
    }
    if (dot_offset > track_height) {
        dot_offset = track_height;
    }
    int pct_scrolled = calc_percentage(dot_offset, track_height);
    scrollbar->scroll_position = calc_adjust_with_percentage(
        scrollbar->max_scroll_position, pct_scrolled);
    scrollbar->is_dragging_scrollbar_dot = 1;
    scrollbar->scrollbar_dot_drag_offset = dot_offset;
    if (scrollbar->scrollbar_dot_drag_offset < 0) {
        scrollbar->scrollbar_dot_drag_offset = 0;
    }
    if (scrollbar->on_scroll_callback) {
        scrollbar->on_scroll_callback();
    }
    return 1;
}

int scrollbar_handle_mouse(scrollbar_type *scrollbar, const mouse *m, int in_dialog)
{
    if (scrollbar->max_scroll_position <= 0) {
        return 0;
    }
    current = scrollbar;
    if (!m->is_touch) {
        scrollbar->touch_drag_state = TOUCH_DRAG_NONE;
    }
    if (scrollbar->touch_drag_state != TOUCH_DRAG_IN_PROGRESS) {
        int scroll_btn_height = get_scrollbar_height(SCROLL_UP_ARROW, scrollbar->legacy);
        if (m->scrolled == SCROLL_DOWN) {
            text_scroll(1, 3);
        } else if (m->scrolled == SCROLL_UP) {
            text_scroll(0, 3);
        }

        if (image_buttons_handle_mouse(m,
            scrollbar->x, scrollbar->y, &scrollbar->image_button_scroll_up, 1, 0)) {
            return 1;
        }
        if (image_buttons_handle_mouse(m, scrollbar->x, scrollbar->y + scrollbar->height - scroll_btn_height,
            &scrollbar->image_button_scroll_down, 1, 0)) {
            return 1;
        }
    }
    if (m->is_touch && handle_touch(scrollbar, touch_get_earliest(), in_dialog)) {
        return 1;
    }
    return handle_scrollbar_dot(scrollbar, m);
}

static void text_scroll(int is_down, int num_lines)
{
    scrollbar_type *scrollbar = current;
    if (is_down) {
        scrollbar->scroll_position += num_lines;
        if (scrollbar->scroll_position > scrollbar->max_scroll_position) {
            scrollbar->scroll_position = scrollbar->max_scroll_position;
        }
    } else {
        if (scrollbar->scroll_position <= (unsigned int) num_lines) {
            scrollbar->scroll_position = 0;
        } else {
            scrollbar->scroll_position -= num_lines;
        }
    }
    scrollbar->is_dragging_scrollbar_dot = 0;
    if (scrollbar->on_scroll_callback) {
        scrollbar->on_scroll_callback();
    }
}
