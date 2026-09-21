#include "text_block.h"

#include "graphics/button.h"
#include "graphics/graphics.h"
#include "graphics/lang_sequence.h"
#include "graphics/panel.h"
#include "graphics/text.h"
#include "graphics/window.h"


#include <stddef.h>
#include <string.h>

static int text_block_content_width(const text_block *block)
{
    return block->width - 2 * block->inner_padding_x;
}

static int text_block_content_height(const text_block *block)
{
    return block->height - 2 * block->inner_padding_y;
}

static sequence_positioning text_block_position(const text_block *block)
{
    if (block->position < SEQUENCE_POSITION_TOP_LEFT || block->position > SEQUENCE_POSITION_BOTTOM_RIGHT) {
        return SEQUENCE_POSITION_CENTER; // default to center if the value is out of bounds
    }
    return block->position;
}

static int text_block_position_column(const text_block *block)
{
    return (text_block_position(block) - 1) % 3;
}

static int text_block_position_row(const text_block *block)
{
    return (text_block_position(block) - 1) / 3;
}

static int text_block_get_x(const text_block *block, int text_width)
{
    int x = block->x + block->inner_padding_x;
    int content_width = text_block_content_width(block);

    if (text_block_position_column(block) == 1) {
        return x + (content_width - text_width) / 2;
    }
    if (text_block_position_column(block) == 2) {
        return x + content_width - text_width;
    }
    return x;
}

static int text_block_get_y(const text_block *block, int text_height)
{
    int y = block->y + block->inner_padding_y;
    int content_height = text_block_content_height(block);

    // Preserve the beginning of content when it is taller than the available area
    if (text_height >= content_height) {
        return y;
    }

    if (text_block_position_row(block) == 1) {
        return y + (content_height - text_height) / 2;
    }
    if (text_block_position_row(block) == 2) {
        return y + content_height - text_height;
    }
    return y;
}

static color_t text_block_color(const text_block *block)
{
    return block->is_disabled ? COLOR_FONT_GRAY : block->font_primary;
}

static void text_block_draw_background_and_border(const text_block *block)
{
    if (block->draw_background) {
        unbordered_panel_draw_px(block->x, block->y, block->width, block->height);
    }
    if (block->draw_border) {
        button_border_draw(block->x, block->y, block->width, block->height, 0);
    }
}

static void text_block_draw_sequence(const text_block *block)
{
    int content_width = text_block_content_width(block);
    int sequence_width = lang_seq_get_width(&block->sequence, block->font);
    int line_height = font_definition_for(block->font)->line_height;
    color_t color = text_block_color(block);

    if (sequence_width <= content_width) {
        int x = text_block_get_x(block, sequence_width);
        int y = text_block_get_y(block, line_height);
        lang_seq_draw(&block->sequence, x, y, block->font, color);
        return;
    }

    int text_height = lang_seq_get_multiline_height(&block->sequence, content_width, 0, block->font);
    int x = block->x + block->inner_padding_x;
    int y = text_block_get_y(block, text_height);

    if (text_block_position_column(block) == 1) {
        lang_seq_draw_multiline_aligned_center(&block->sequence, x, y, content_width, 0, block->font, color);
    } else if (text_block_position_column(block) == 2) {
        lang_seq_draw_multiline_aligned_right(&block->sequence, x, y, content_width, 0, block->font, color);
    } else {
        lang_seq_draw_multiline_aligned_left(&block->sequence, x, y, content_width, 0, block->font, color);
    }
}

static void text_block_draw_raw(const text_block *block)
{
    int content_width = text_block_content_width(block);
    int text_width = text_get_width(block->raw_text, block->font);
    int line_height = font_definition_for(block->font)->line_height;
    color_t color = text_block_color(block);

    // Single-line raw text gets normal positioning because it requires no extra layout logic.
    if (text_width <= content_width) {
        int x = text_block_get_x(block, text_width);
        int y = text_block_get_y(block, line_height);
        text_draw(block->raw_text, x, y, block->font, color);
        return;
    }

    // Multiline raw text is intentionally only a simple fallback.
    int x = block->x + block->inner_padding_x;
    int y = block->y + block->inner_padding_y;
    text_draw_multiline(block->raw_text, x, y, content_width, 0, block->font, color);
}

int widget_text_block_init_simple(text_block *block, int x, int y, int width, int height, const lang_sequence *sequence,
    sequence_positioning position)
{
    if (!block) {
        return 0;
    }

    memset(block, 0, sizeof(*block));

    if (sequence) {
        block->sequence = *sequence;
    }

    block->position = position;
    block->font = FONT_NORMAL_BLACK;
    block->font_primary = COLOR_MASK_NONE;
    block->x = x;
    block->y = y;
    block->width = width;
    block->height = height;
    block->inner_padding_x = 2;
    block->inner_padding_y = 2;

    return 1;
}

void text_block_draw(const text_block *block)
{
    if (!block || block->is_hidden) {
        return;
    }

    graphics_set_clip_rectangle(block->x, block->y, block->width, block->height);
    text_block_draw_background_and_border(block);

    if (block->sequence.count > 0) {
        text_block_draw_sequence(block);
    } else if (block->raw_text) {
        text_block_draw_raw(block);
    }

    graphics_reset_clip_rectangle();
}

int text_block_handle_mouse(text_block *block, const mouse *m)
{
    // currently no mouse functionality, only hover state tracking for tooltip support
    if (!block || !m) {
        return 0;
    }

    if (block->is_hidden || block->is_disabled) {
        if (block->state_is_hovered) {
            block->state_is_hovered = 0;
            window_request_refresh();
        }
        return 0;
    }

    int inside = m->x >= block->x && m->x < block->x + block->width &&
        m->y >= block->y && m->y < block->y + block->height;

    if (block->state_is_hovered != inside) {
        block->state_is_hovered = inside;
        window_request_refresh();
    }

    return 0;
}

int text_block_handle_tooltip(const text_block *block, tooltip_context *c)
{
    if (!block || !c || block->is_hidden || !block->state_is_hovered || tooltip_context_is_empty(&block->tooltip_c)) {
        return 0;
    }

    tooltip_copy_context(c, &block->tooltip_c);
    return 1;
}