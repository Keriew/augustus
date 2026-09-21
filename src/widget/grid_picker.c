#include "grid_picker.h"

#include "graphics/button.h"
#include "graphics/complex_button.h"
#include "graphics/graphics.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/tooltip.h"
#include "graphics/window.h"
#include <string.h>

#define GRID_PICKER_SCREEN_MARGIN 10

int grid_picker_row_column_to_index(grid_picker *picker, int row, int column);
int grid_picker_index_to_row_column(grid_picker *picker, int index, int *row, int *column);

static int debug_shader = 2;

void grid_picker_cells_init(int count, grid_picker_cell *cells, int *images, lang_fragment *sequence, int sequence_size,
    tooltip_context *tooltip_c)
{
    memset(cells, 0, sizeof(*cells) * count);

    for (int i = 0; i < count; i++) {
        cells[i].index = i;
        cells[i].image.id = images ? images[i] : -1;
        cells[i].sequence = sequence ? &sequence[i] : NULL;
        cells[i].sequence_size = sequence_size;
        tooltip_copy_context(&cells[i].tooltip_c, tooltip_c ? &tooltip_c[i] : &(tooltip_context) { 0 });
    }
}

void grid_picker_anchor_init(complex_button *anchor, int x, int y, int width, int height,
    const lang_fragment *sequence, int sequence_size, complex_button_style style, tooltip_context *tooltip_c)
{
    if (!anchor) {
        return;
    }

    memset(anchor, 0, sizeof(*anchor));
    anchor->x = x;
    anchor->y = y;
    anchor->width = width;
    anchor->height = height;
    anchor->style = style;
    anchor->sequence = sequence;
    anchor->sequence_size = sequence_size;
    anchor->sequence_position = SEQUENCE_POSITION_CENTER;
    if (tooltip_c) {
        tooltip_copy_context(&anchor->tooltip_c, tooltip_c);
    }
}

static void grid_picker_geometry(grid_picker *picker)
{
    int s_width = screen_width();
    int s_height = screen_height();

    // calculate the grid width and height:
    picker->grid_width = picker->columns * picker->cell_width + (picker->columns - 1) * picker->spacing_h;
    picker->grid_height = picker->rows * picker->cell_height + (picker->rows - 1) * picker->spacing_v;
    picker->calculated_width = picker->grid_width + 2 * picker->margin;
    picker->calculated_height = picker->grid_height + 2 * picker->margin;

    int anchor_center_x = picker->anchor.x + picker->anchor.width / 2;
    if (picker->picker_y_offset == 0) {
        picker->picker_y_offset = 1; // always move at least 1px down to avoid overlapping the anchor button
    }
    picker->grid_x = anchor_center_x - picker->calculated_width / 2 + picker->picker_x_offset;
    picker->grid_y = picker->anchor.y + picker->anchor.height + picker->picker_y_offset;


    // Clamp horizontally
    if (picker->grid_x < GRID_PICKER_SCREEN_MARGIN) {
        picker->grid_x = GRID_PICKER_SCREEN_MARGIN;
    } else if (picker->grid_x + picker->calculated_width > s_width - GRID_PICKER_SCREEN_MARGIN) {
        picker->grid_x = s_width - GRID_PICKER_SCREEN_MARGIN - picker->calculated_width;
    }

    // Clamp vertically
    if (picker->grid_y < GRID_PICKER_SCREEN_MARGIN) {
        picker->grid_y = GRID_PICKER_SCREEN_MARGIN;
    } else if (picker->grid_y + picker->calculated_height > s_height - GRID_PICKER_SCREEN_MARGIN) {
        picker->grid_y = s_height - GRID_PICKER_SCREEN_MARGIN - picker->calculated_height;
    }
    for (unsigned int i = 0; i < picker->cell_count; i++) {
        int row = (int) i / picker->columns;
        int column = (int) i % picker->columns;

        int first_index = row * picker->columns;
        int cells_in_row = picker->cell_count - first_index;
        if (cells_in_row > picker->columns) {
            cells_in_row = picker->columns;
        }

        int row_width = cells_in_row * picker->cell_width + (cells_in_row - 1) * picker->spacing_h;
        int row_x_offset = (picker->grid_width - row_width) / 2;

        grid_picker_cell *cell = &picker->cells[row][column];

        cell->x = picker->grid_x + picker->margin + row_x_offset + column * (picker->cell_width + picker->spacing_h);
        cell->y = picker->grid_y + picker->margin + row * (picker->cell_height + picker->spacing_v);
    }
}

// simple init should set picker_y_offset to like 5 or smth for base
void grid_picker_init(complex_button *anchor, grid_picker *picker, const grid_picker_cell *cells, unsigned int cell_count,
    int columns, int rows, int cell_width, int cell_height, int spacing, grid_picker_style style)
{
    if (!picker || !cells || !anchor || cell_count == 0 || cell_count > GRID_PICKER_MAX_OPTIONS) {
        return;
    }

    if (columns <= 0 || columns > GRID_PICKER_MAX_COLUMNS || rows <= 0 || rows > GRID_PICKER_MAX_ROWS) {
        return;
    }

    if (cell_width <= 0 || cell_height <= 0 || spacing < 0 || cell_count >(unsigned int) (columns * rows)) {
        return;
    }

    memset(picker, 0, sizeof(*picker));

    memcpy(&picker->anchor, anchor, sizeof(picker->anchor));
    // pointer assignment wont work because tooltip_c contains const's - memcpy

    picker->cell_count = cell_count;
    picker->columns = columns;
    picker->rows = rows;
    picker->cell_width = cell_width;
    picker->cell_height = cell_height;
    picker->spacing_h = spacing;
    picker->spacing_v = spacing;
    picker->selected_index = -1;
    picker->hovered_index = -1;
    picker->margin = 10;
    picker->style = style;

    for (unsigned int i = 0; i < cell_count; i++) {
        int row = (int) i / columns;
        int column = (int) i % columns;
        grid_picker_cell *cell = &picker->cells[row][column];

        memcpy(cell, &cells[i], sizeof(*cell));

        cell->index = (int) i;
        cell->x = column * (cell_width + spacing);
        cell->y = row * (cell_height + spacing);
    }
    grid_picker_geometry(picker);
}

static font_t grid_picker_font_for_style(grid_picker_style style)
{
    switch (style) {
        case GRID_PICKER_STYLE_GRAY:
            return FONT_NORMAL_GREEN;
        default:
            return FONT_NORMAL_BLACK;
    }
}

static color_t grid_picker_color_for_style(grid_picker_style style)
{
    switch (style) {
        default:
            return COLOR_MASK_NONE;
    }
}

static void grid_picker_draw_cell_contents(const grid_picker *picker, const grid_picker_cell *cell)
{
    const int inner_margin = 2;
    font_t font = grid_picker_font_for_style(picker->style);
    color_t color = grid_picker_color_for_style(picker->style);
    int image_before_width = 0;
    int image_after_width = 0;
    int sequence_width = 0;
    graphics_set_clip_rectangle(cell->x, cell->y, picker->cell_width, picker->cell_height);

    // if main image ->no other content
    if (cell->image.id > 0) {
        int x, y;
        if (cell->image.auto_center) {
            int image_width = image_get(cell->image.id)->width;
            int image_height = image_get(cell->image.id)->height;
            x = cell->x + (picker->cell_width - image_width) / 2 + cell->image.image_x_offset;
            y = cell->y + (picker->cell_height - image_height) / 2 + cell->image.image_y_offset;
        } else {
            x = cell->x + cell->image.image_x_offset;
            y = cell->y + cell->image.image_y_offset;
        }
        image_draw(cell->image.id, x, y, COLOR_MASK_NONE, SCALE_NONE);
        graphics_reset_clip_rectangle();
        return;
    }

    if (cell->image_before > 0) {
        image_before_width = (image_get(cell->image_before)->width) + inner_margin;
    }
    if (cell->image_after > 0) {
        image_after_width = (image_get(cell->image_after)->width) + inner_margin;
    }

    int text_max_width = picker->cell_width - 2 * inner_margin - image_before_width - image_after_width;

    if (cell->sequence && cell->sequence_size > 0) {
        lang_sequence sequence;
        lang_seq_init(&sequence, (lang_fragment *) cell->sequence, cell->sequence_size);
        sequence_width = lang_seq_get_width(&sequence, font);

        if (sequence_width > text_max_width) {
            sequence_width = text_max_width;
        }
    }

    int total_width = image_before_width + sequence_width + image_after_width;
    int cursor_x = cell->x + (picker->cell_width - total_width) / 2;
    int text_y = cell->y + (picker->cell_height - font_definition_for(font)->line_height) / 2;

    if (cell->image_before > 0) {
        int image_y = cell->y + (picker->cell_height - image_get(cell->image_before)->height) / 2;
        image_draw(cell->image_before, cursor_x, image_y, COLOR_MASK_NONE, SCALE_NONE);
        cursor_x += image_before_width;
    }
    if (cell->sequence && cell->sequence_size > 0) {
        lang_sequence sequence;
        lang_seq_init(&sequence, (lang_fragment *) cell->sequence, cell->sequence_size);
        cursor_x += lang_seq_draw_ellipsized(&sequence, cursor_x, text_y, text_max_width, font, color, NULL);
    }

    if (cell->image_after > 0) {
        int image_y = cell->y + (picker->cell_height - image_get(cell->image_after)->height) / 2;
        image_draw(cell->image_after, cursor_x + inner_margin, image_y, COLOR_MASK_NONE, SCALE_NONE);
    }

    graphics_reset_clip_rectangle();
}

static void grid_picker_draw_default_style(grid_picker *picker)
{

    bordered_panel_draw_colored(picker->grid_x, picker->grid_y, picker->calculated_width, picker->calculated_height,
        0, COLOR_MASK_NONE, COLOR_MASK_NONE);

    for (unsigned int i = 0; i < picker->cell_count; i++) {
        int row = (int) i / picker->columns;
        int column = (int) i % picker->columns;
        grid_picker_cell *cell = &picker->cells[row][column];
        grid_picker_draw_cell_contents(picker, cell);
        int has_focus = cell->index == picker->hovered_index;
        button_border_draw_colored(cell->x, cell->y, picker->cell_width, picker->cell_height, has_focus, 0);
    }
}

static void grid_picker_draw_gray_style(grid_picker *picker)
{

    large_label_draw_custom_size(picker->grid_x, picker->grid_y, picker->calculated_width, picker->calculated_height);

    for (unsigned int i = 0; i < picker->cell_count; i++) {
        int row = (int) i / picker->columns;
        int column = (int) i % picker->columns;
        grid_picker_cell *cell = &picker->cells[row][column];
        int has_focus = cell->index == picker->hovered_index;
        grid_picker_draw_cell_contents(picker, cell);
        large_label_draw_border(cell->x, cell->y, picker->cell_width, picker->cell_height);
        if (has_focus) {
            graphics_shade_rect(cell->x, cell->y, picker->cell_width, picker->cell_height, debug_shader);
        }
    }
}

void grid_picker_draw(grid_picker *picker)
{
    grid_picker_geometry(picker); // recalc before drawing
    if (!picker) {
        return;
    }
    complex_button_draw(&picker->anchor);
    if (!picker->is_expanded) {
        return;
    }
    switch (picker->style) {
        case GRID_PICKER_STYLE_GRAY:
            grid_picker_draw_gray_style(picker);
            break;
        default:
            grid_picker_draw_default_style(picker);
    }
}

static grid_picker_cell *get_selected_cell(grid_picker *picker)
{
    if (!picker || picker->selected_index < 0) {
        return NULL;
    }
    int row, column;
    if (grid_picker_index_to_row_column(picker, picker->selected_index, &row, &column) < 0) {
        return NULL;
    }
    return &picker->cells[row][column];
}

void update_anchor(grid_picker *picker)
{
    grid_picker_cell *cell = get_selected_cell(picker);
    if (!cell) {
        return;
    }
    if (cell->image.id > 0) {
        picker->anchor.image = cell->image;
        picker->anchor.sequence = NULL;
        picker->anchor.sequence_size = 0;
    } else {
        picker->anchor.image_before = cell->image_before;
        picker->anchor.sequence = cell->sequence;
        picker->anchor.sequence_size = cell->sequence_size;
        picker->anchor.image_after = cell->image_after;
    }
}

int grid_picker_handle_mouse(grid_picker *picker, const mouse *m)
{
    int handled = 0; // if input is handled, return 1 to stop further input processing
    picker->hovered_index = -1; // reset hovered index each frame, will be set if mouse is over a cell
    complex_button *anchor_btn = &picker->anchor;
    if (complex_button_handle_mouse(anchor_btn, m)) {
        if (anchor_btn->is_clicked) {
            picker->is_expanded = !picker->is_expanded;
            window_request_refresh();
        }
        return 1;
    }

    if (picker->is_expanded) {
        handled = 1; // if picker is expanded, swallow all mouse input.
        if (m->right.went_up) {
            picker->is_expanded = 0;
            window_request_refresh();
            return 1;
        }
        int inside = (m->x >= picker->grid_x && m->x < picker->grid_x + picker->calculated_width &&
            m->y >= picker->grid_y && m->y < picker->grid_y + picker->calculated_height);
        if (m->left.went_up && !inside) {
            picker->is_expanded = 0;
            window_request_refresh();
            return 1;
        }
        for (unsigned int i = 0; i < picker->cell_count; i++) {
            int row = (int) i / picker->columns;
            int column = (int) i % picker->columns;
            grid_picker_cell *cell = &picker->cells[row][column];

            int inside_cell = m->x >= cell->x && m->x < cell->x + picker->cell_width &&
                m->y >= cell->y && m->y < cell->y + picker->cell_height;

            if (inside_cell) {
                picker->hovered_index = cell->index;

                if (m->left.went_up) {
                    picker->selected_index = cell->index;
                    update_anchor(picker);
                    if (picker->selected_callback) {
                        picker->selected_callback(picker);
                    }
                    picker->is_expanded = 0;
                    window_request_refresh();
                }
                if (picker->hover_callback) {
                    picker->hover_callback(picker);
                }
                handled = 1;
                break;
            }
        }
    }

    return handled;
}

int grid_picker_handle_tooltip(grid_picker *picker, tooltip_context *c)
{
    if (picker->is_expanded && picker->hovered_index >= 0) {
        int row;
        int column;
        if (grid_picker_index_to_row_column(picker, picker->hovered_index, &row, &column) < 0) {
            return 0;
        }
        grid_picker_cell *cell = &picker->cells[row][column];
        tooltip_copy_context(c, &cell->tooltip_c);
        return 1;
    } else if (picker->anchor.is_focused && !tooltip_context_is_empty(&picker->anchor.tooltip_c)) {
        tooltip_copy_context(c, &picker->anchor.tooltip_c);
        return 1;
    }
    return 0;
}

int grid_picker_row_column_to_index(grid_picker *picker, int row, int column)
{
    if (!picker) {
        return -1;
    }

    if (row < 0 || row >= picker->rows ||
        column < 0 || column >= picker->columns) {
        return -1;
    }

    int index = row * picker->columns + column;

    if (index >= (int) picker->cell_count) {
        return -1;
    }

    return index;
}

int grid_picker_index_to_row_column(grid_picker *picker, int index, int *row, int *column)
{
    if (!picker || !row || !column) {
        return -1;
    }

    if (index < 0 || index >= (int) picker->cell_count) {
        return -1;
    }

    *row = index / picker->columns;
    *column = index % picker->columns;

    return 0;
}
