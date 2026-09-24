#include "empire_funds.h"

#include "assets/assets.h"
#include "city/finance.h"
#include "core/calc.h"
#include "core/time.h"
#include "game/resource.h"
#include "game/time.h"
#include "graphics/complex_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_sequence.h"
#include "graphics/panel.h"
#include "graphics/window.h"
#include "widget/text_block.h"

#include <stdint.h>

#define WIDGET_WIDTH 460
#define WIDGET_MIN_VISIBLE_WIDTH 240
#define WIDGET_MAX_AVAILABLE_PERCENT 80
#define WIDGET_HEIGHT 36
#define WIDGET_SLIDE_DURATION_MILLIS 1000
#define WIDGET_HIDE_NUDGE_OFFSET 10
#define WIDGET_BANNER_VISIBLE_TIP 8
#define WIDGET_BANNER_HIDDEN_HITBOX_EXTRA 10
#define WIDGET_TOP_BORDER_CLEARANCE 10
#define CENTER_RESERVED_WIDTH 44
#define TEXT_BLOCK_HEIGHT (WIDGET_HEIGHT - 8)
#define TEXT_BLOCK_MARGIN_X 20
#define TEXT_BLOCK_Y 4

static struct {
    int x;
    int y;
    int width;
    int initialised;
    int is_hidden;
    int is_animating;
    int is_hiding;
    time_millis animation_start;
    lang_date_sequence date_sequence;
    lang_fragment money_fragments[3];
    lang_sequence money_sequence;
    text_block date_block;
    text_block money_block;
    complex_button banner_button;
} widget_data;

static int clip_top(void)
{
    return widget_data.y - WIDGET_TOP_BORDER_CLEARANCE;
}

static int hidden_offset(void)
{
    const image *banner = image_get(widget_data.banner_button.image.id);
    int banner_y = widget_data.y + (WIDGET_HEIGHT - banner->height) / 2 + 4;

    return clip_top() + WIDGET_BANNER_VISIBLE_TIP - (banner_y + banner->height);
}

static int interpolate_offset(int start, int end, int elapsed, int duration)
{
    if (duration <= 0) {
        return end;
    }
    elapsed = calc_bound(elapsed, 0, duration);
    return start + (end - start) * elapsed / duration;
}

static int animation_offset_at(int elapsed, int hiding)
{
    int nudge_millis = WIDGET_SLIDE_DURATION_MILLIS / 5;
    int final_offset = hidden_offset();

    if (hiding) {
        if (elapsed < nudge_millis) {
            return interpolate_offset(0, WIDGET_HIDE_NUDGE_OFFSET, elapsed, nudge_millis);
        }
        return interpolate_offset(WIDGET_HIDE_NUDGE_OFFSET, final_offset, elapsed - nudge_millis,
            WIDGET_SLIDE_DURATION_MILLIS - nudge_millis);
    }
    if (elapsed < WIDGET_SLIDE_DURATION_MILLIS - nudge_millis) {
        return interpolate_offset(final_offset, WIDGET_HIDE_NUDGE_OFFSET, elapsed,
            WIDGET_SLIDE_DURATION_MILLIS - nudge_millis);
    }
    return interpolate_offset(WIDGET_HIDE_NUDGE_OFFSET, 0, elapsed - (WIDGET_SLIDE_DURATION_MILLIS - nudge_millis),
        nudge_millis);
}

static int current_y_offset(void)
{
    if (!widget_data.is_animating) {
        return widget_data.is_hidden ? hidden_offset() : 0;
    }

    time_millis elapsed_millis = time_get_millis() - widget_data.animation_start;
    int elapsed = elapsed_millis > WIDGET_SLIDE_DURATION_MILLIS ?
        WIDGET_SLIDE_DURATION_MILLIS : (int) elapsed_millis;
    if (elapsed >= WIDGET_SLIDE_DURATION_MILLIS) {
        widget_data.is_animating = 0;
        widget_data.is_hidden = widget_data.is_hiding;
        return widget_data.is_hidden ? hidden_offset() : 0;
    }

    window_request_refresh();
    return animation_offset_at(elapsed, widget_data.is_hiding);
}

static void button_toggle_visibility(complex_button *button)
{
    (void) button;

    widget_data.is_animating = 1;
    widget_data.is_hiding = !widget_data.is_hidden;
    widget_data.animation_start = time_get_millis();
    widget_data.money_block.is_hidden = 0;
    widget_data.date_block.is_hidden = 0;
    window_request_refresh();
}

static void draw_background(int x, int y)
{
    inner_panel_draw_colored(x, y, widget_data.width, WIDGET_HEIGHT, COLOR_MASK_NONE);
}

static int text_block_text_width_available(const text_block *block)
{
    int width = block->width - 2 * block->inner_padding_x;

    if (block->image_before > 0) {
        const image *img = image_get(block->image_before);
        width -= img->original.width + 2;
    }
    if (block->image_after > 0) {
        const image *img = image_get(block->image_after);
        width -= img->original.width + 2;
    }

    return width < 0 ? 0 : width;
}

static void set_date_sequence(lang_date_format format)
{
    lang_sequence_date_init_format(&widget_data.date_sequence, game_time_year(), game_time_month(), 0, 1, format);
    widget_data.date_block.sequence = widget_data.date_sequence.sequence;
}

static void update_date_sequence(void)
{
    int max_width = text_block_text_width_available(&widget_data.date_block);

    set_date_sequence(LANG_DATE_FORMAT_FULL);
    if (lang_seq_get_width(&widget_data.date_sequence.sequence, widget_data.date_block.font) <= max_width) {
        return;
    }
    set_date_sequence(LANG_DATE_FORMAT_MONTH_YEAR);
    if (lang_seq_get_width(&widget_data.date_sequence.sequence, widget_data.date_block.font) <= max_width) {
        return;
    }
    set_date_sequence(LANG_DATE_FORMAT_YEAR);
}

static void update_money_text(void)
{
    int funds = city_finance_treasury();

    if (funds < 0) {
        widget_data.money_block.font = FONT_NORMAL_RED;
    } else {
        widget_data.money_block.font = FONT_NORMAL_GREEN;
    }
    lang_seq_frag_number(&widget_data.money_fragments[2], funds);
}

static void update_banner_button(int offset_x, int y_offset, int extend_hidden_hitbox)
{
    const image *banner = image_get(widget_data.banner_button.image.id);
    int full_x = widget_data.x + offset_x + (widget_data.width - banner->width) / 2;
    int full_y = widget_data.y + y_offset + (WIDGET_HEIGHT - banner->height) / 2 + 4;
    int visible_y = full_y < clip_top() ? clip_top() : full_y;
    int visible_height = full_y + banner->height - visible_y;

    if (visible_height < 0) {
        visible_height = 0;
    }
    if (visible_height > banner->height) {
        visible_height = banner->height;
    }

    widget_data.banner_button.x = full_x;
    widget_data.banner_button.y = visible_y;
    widget_data.banner_button.width = banner->width;
    widget_data.banner_button.height = visible_height;
    if (extend_hidden_hitbox && widget_data.is_hidden && !widget_data.is_animating && visible_height > 0) {
        widget_data.banner_button.height += WIDGET_BANNER_HIDDEN_HITBOX_EXTRA;
    }
    widget_data.banner_button.image.image_x_offset = 0;
    widget_data.banner_button.image.image_y_offset = full_y - visible_y;
    widget_data.banner_button.is_hidden = visible_height <= 0;
    widget_data.banner_button.light_on_hover = 1;
}

int widget_empire_funds_width_for_available(int available_width)
{
    int width = available_width * WIDGET_MAX_AVAILABLE_PERCENT / 100;

    if (width > WIDGET_WIDTH) {
        width = WIDGET_WIDTH;
    }
    if (width < WIDGET_MIN_VISIBLE_WIDTH) {
        width = 0;
    }

    return width;
}

void widget_empire_funds_initialise(int x, int y, int width)
{
    int text_block_width = (width - CENTER_RESERVED_WIDTH - 2 * TEXT_BLOCK_MARGIN_X) / 2;

    widget_data.x = x;
    widget_data.y = y;
    widget_data.width = width;
    widget_data.initialised = 1;
    lang_seq_frag_label(&widget_data.money_fragments[0], CUSTOM_TRANSLATION, TR_WIDGET_DN);
    lang_seq_frag_space(&widget_data.money_fragments[1], 3);
    lang_seq_frag_number(&widget_data.money_fragments[2], city_finance_treasury());
    widget_data.money_sequence.count = 3;
    widget_data.money_sequence.fragments = widget_data.money_fragments;

    widget_text_block_init_simple(&widget_data.money_block, x + TEXT_BLOCK_MARGIN_X, y + TEXT_BLOCK_Y,
        text_block_width, TEXT_BLOCK_HEIGHT, &widget_data.money_sequence, SEQUENCE_POSITION_CENTER, TEXT_BLOCK_STYLE_RAW);

    resource_data *money = resource_get_data(RESOURCE_DENARII);
    widget_data.money_block.image_before = money->image.icon;
    widget_data.money_block.image_after = money->image.icon;
    widget_data.money_block.font = FONT_NORMAL_GREEN;
    widget_data.money_block.text_offset_x = 4;
    widget_data.money_block.text_offset_y = 1;
    widget_data.money_block.tooltip_c.type = TOOLTIP_BUTTON;
    widget_data.money_block.tooltip_c.text_group = 68;
    widget_data.money_block.tooltip_c.text_id = 60;
    widget_data.money_block.is_hidden = 0;

    widget_text_block_init_simple(&widget_data.date_block,
        x + width - TEXT_BLOCK_MARGIN_X - text_block_width, y + TEXT_BLOCK_Y,
        text_block_width, TEXT_BLOCK_HEIGHT, NULL, SEQUENCE_POSITION_CENTER, TEXT_BLOCK_STYLE_RAW);
    widget_data.date_block.image_before = assets_lookup_image_id(ASSET_UI_HOURGLASS_ICON);
    widget_data.date_block.image_after = assets_lookup_image_id(ASSET_UI_HOURGLASS_ICON);
    widget_data.date_block.font = FONT_NORMAL_GREEN;
    widget_data.date_block.text_offset_y = 1;
    widget_data.date_block.tooltip_c.type = TOOLTIP_BUTTON;
    widget_data.date_block.tooltip_c.text_group = 68;
    widget_data.date_block.tooltip_c.text_id = 62;
    widget_data.date_block.is_hidden = 0;
    update_date_sequence();

    complex_button_init_style(&widget_data.banner_button, COMPLEX_BUTTON_STYLE_IMAGE);
    widget_data.banner_button.image.id = assets_get_image_id("UI", "Victory_Banner");
    widget_data.banner_button.image.auto_center = 0;
    widget_data.banner_button.left_click_handler = button_toggle_visibility;
    update_banner_button(0, current_y_offset(), 0);
}

static void draw_text_block_with_offset(text_block *block, int offset_x, int offset_y)
{
    int original_x = block->x;
    int original_y = block->y;

    block->x += offset_x;
    block->y += offset_y;
    widget_text_block_draw_clipped(block, widget_data.x + offset_x, clip_top(), widget_data.width,
        WIDGET_HEIGHT + WIDGET_TOP_BORDER_CLEARANCE + WIDGET_HIDE_NUDGE_OFFSET);

    block->x = original_x;
    block->y = original_y;
}

void widget_empire_funds_draw(int offset_x, int offset_y)
{
    if (!widget_data.initialised) {
        return;
    }

    int y_offset = current_y_offset();
    int draw_y = widget_data.y + offset_y + y_offset;

    update_banner_button(offset_x, y_offset + offset_y, 0);
    graphics_set_clip_rectangle(widget_data.x + offset_x, clip_top(), widget_data.width,
        WIDGET_HEIGHT + WIDGET_TOP_BORDER_CLEARANCE + WIDGET_HIDE_NUDGE_OFFSET);
    draw_background(widget_data.x + offset_x, draw_y);
    segmented_border_draw(widget_data.x + offset_x, draw_y, widget_data.width, WIDGET_HEIGHT);
    graphics_reset_clip_rectangle();

    update_money_text();
    update_date_sequence();
    draw_text_block_with_offset(&widget_data.money_block, offset_x, y_offset + offset_y);
    draw_text_block_with_offset(&widget_data.date_block, offset_x, y_offset + offset_y);
    complex_button_draw(&widget_data.banner_button);
}

static int handle_text_block_mouse_with_offset(text_block *block, const mouse *m, int offset_x, int offset_y)
{
    int handled;

    block->x += offset_x;
    block->y += offset_y;
    handled = widget_text_block_handle_mouse(block, m);
    block->x -= offset_x;
    block->y -= offset_y;

    return handled;
}

int widget_empire_funds_handle_mouse(const mouse *m, int offset_x, int offset_y)
{
    if (!widget_data.initialised) {
        return 0;
    }

    int y_offset = current_y_offset();
    update_banner_button(offset_x, y_offset + offset_y, 1);
    if (complex_button_handle_mouse(&widget_data.banner_button, m)) {
        return 1;
    }

    if (widget_data.is_hidden || widget_data.is_animating) {
        return 0;
    }

    handle_text_block_mouse_with_offset(&widget_data.money_block, m, offset_x, y_offset + offset_y);
    handle_text_block_mouse_with_offset(&widget_data.date_block, m, offset_x, y_offset + offset_y);

    return 0;
}

int widget_empire_funds_handle_tooltip(tooltip_context *c)
{
    if (!widget_data.initialised || widget_data.is_hidden || widget_data.is_animating) {
        return 0;
    }

    return widget_text_block_handle_tooltip(&widget_data.money_block, c) ||
        widget_text_block_handle_tooltip(&widget_data.date_block, c);
}

int widget_empire_funds_width(void)
{
    return widget_data.initialised ? widget_data.width : WIDGET_WIDTH;
}

int widget_empire_funds_height(void)
{
    return WIDGET_HEIGHT;
}
