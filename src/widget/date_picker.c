#include "date_picker.h"

#include "assets/assets.h"
#include "core/string.h"
#include "game/time.h"
#include "graphics/window.h"
#include "translation/translation.h"

#include <string.h>

static btn_img plus_sign[1];
static btn_img minus_sign[1];

typedef enum {
    DATE_PICKER_DECREASE = 0,
    DATE_PICKER_DATE,
    DATE_PICKER_INCREASE,
    DATE_PICKER_BUTTON_COUNT
} date_picker_button_type;

static int picker_min_offset(const date_picker *picker)
{
    return -picker->years_forward;
}

static int picker_max_offset(const date_picker *picker)
{
    return picker->years_back;
}

static void clamp_selected_year(date_picker *picker)
{
    if (picker->selected_year_offset < picker_min_offset(picker)) {
        picker->selected_year_offset = picker_min_offset(picker);
    }
    if (picker->selected_year_offset > picker_max_offset(picker)) {
        picker->selected_year_offset = picker_max_offset(picker);
    }
}

static void compose_jump_tooltip(date_picker *picker, translation_key current_tooltip)
{
    uint8_t *cursor = picker->tooltip_text;
    int remaining = (int) sizeof(picker->tooltip_text);

    // Match the old numeric-prefix tooltip shape: "N" + " years ago".
    if (picker->selected_year_offset > 1) {
        int offset = string_from_int(cursor, picker->selected_year_offset, 0);
        cursor += offset;
        remaining -= offset;
    }

    cursor = string_copy(translation_for(current_tooltip), cursor, remaining);
    remaining = (int) sizeof(picker->tooltip_text) - (int) (cursor - picker->tooltip_text);
    if (remaining > 1) {
        cursor = string_copy(string_from_ascii("\n"), cursor, remaining);
        remaining = (int) sizeof(picker->tooltip_text) - (int) (cursor - picker->tooltip_text);
    }
    if (remaining > 0) {
        string_copy(translation_for(TR_SIDEBAR_DATE_JUMP_TO_CURRENT), cursor, remaining);
    }
}

static void update_tooltips(date_picker *picker)
{
    complex_button *decrease = &picker->buttons[DATE_PICKER_DECREASE];
    complex_button *date = &picker->buttons[DATE_PICKER_DATE];
    complex_button *increase = &picker->buttons[DATE_PICKER_INCREASE];

    decrease->tooltip_c.translation_key = decrease->is_disabled ?
        TR_UI_TRADE_YEAR_NO_EARLIER : TR_UI_TRADE_YEAR_PREVIOUS;
    increase->tooltip_c.translation_key = increase->is_disabled ?
        TR_UI_TRADE_YEAR_CURRENT_LIMIT : TR_UI_TRADE_YEAR_NEXT;

    date->tooltip_c.type = TOOLTIP_BUTTON;
    date->tooltip_c.translation_key = TR_UI_TRADE_YEAR_CURRENT;
    date->tooltip_c.has_numeric_prefix = 0;
    date->tooltip_c.numeric_prefix = 0;
    date->tooltip_c.precomposed_text = 0;

    if (picker->selected_year_offset > 0) {
        translation_key current_tooltip = picker->selected_year_offset == 1 ?
            TR_UI_TRADE_YEAR_LAST : TR_UI_TRADE_YEAR_YEARS_AGO;
        compose_jump_tooltip(picker, current_tooltip);
        date->tooltip_c.translation_key = 0;
        date->tooltip_c.precomposed_text = picker->tooltip_text;
    }
}

static void refresh_button_state(date_picker *picker)
{
    // Keep the public selected offset valid before drawing or handling hover/clicks.
    clamp_selected_year(picker);

    picker->buttons[DATE_PICKER_DECREASE].is_disabled = picker->selected_year_offset >= picker_max_offset(picker);
    picker->buttons[DATE_PICKER_INCREASE].is_disabled = picker->selected_year_offset <= picker_min_offset(picker);
    picker->buttons[DATE_PICKER_DATE].is_disabled = picker->selected_year_offset == 0;
    update_tooltips(picker);
}

static void set_selected_year(date_picker *picker, int selected_year_offset)
{
    int previous = picker->selected_year_offset;

    picker->selected_year_offset = selected_year_offset;
    refresh_button_state(picker);
    picker->changed = previous != picker->selected_year_offset;
    if (picker->changed) {
        window_request_refresh();
    }
}

static void decrease_click(complex_button *button)
{
    date_picker *picker = button->user_data;

    if (picker) {
        set_selected_year(picker, picker->selected_year_offset + 1);
    }
}

static void increase_click(complex_button *button)
{
    date_picker *picker = button->user_data;

    if (picker) {
        set_selected_year(picker, picker->selected_year_offset - 1);
    }
}

static void date_click(complex_button *button)
{
    date_picker *picker = button->user_data;

    if (picker && picker->selected_year_offset != 0) {
        set_selected_year(picker, 0);
    }
}

static void init_change_button(complex_button *button, date_picker *picker, int x, int y, int asset_id,
    void (*click_handler)(complex_button *button))
{
    memset(button, 0, sizeof(*button));
    complex_button_init_style(button, COMPLEX_BUTTON_STYLE_IMAGE);
    button->x = x;
    button->y = y;
    button->width = DATE_PICKER_BUTTON_WIDTH;
    button->height = DATE_PICKER_BUTTON_HEIGHT;
    button->image.id = assets_lookup_image_id(asset_id);
    button->left_click_handler = click_handler;
    button->light_on_hover = 2;
    button->user_data = picker;
    plus_sign[0].id = assets_lookup_image_id(ASSET_UI_PLUS_BUTTON_CLICK);
    minus_sign[0].id = assets_lookup_image_id(ASSET_UI_MINUS_BUTTON_CLICK);
    if (asset_id == ASSET_UI_PLUS_BUTTON_IDLE) {
        complex_button_animation_init(button, plus_sign, 1, BUTTON_ANIMATION_TRIGGER_CLICK, BUTTON_ANIMATION_ONCE);
    } else if (asset_id == ASSET_UI_MINUS_BUTTON_IDLE) {
        complex_button_animation_init(button, minus_sign, 1, BUTTON_ANIMATION_TRIGGER_CLICK, BUTTON_ANIMATION_ONCE);
    }

}

void widget_date_picker_init(date_picker *picker, int x, int y, int date_field_height, int date_field_width,
    complex_button_style style, int padding, int years_back, int years_forward)
{
    if (!picker) {
        return;
    }

    memset(picker, 0, sizeof(*picker));
    picker->x = x;
    picker->y = y;
    picker->date_field_height = date_field_height;
    picker->date_field_width = date_field_width;
    picker->padding = padding;
    picker->years_back = years_back < 0 ? 0 : years_back;
    picker->years_forward = years_forward < 0 ? 0 : years_forward;

    int change_button_y = y + (date_field_height - DATE_PICKER_BUTTON_HEIGHT) / 2;
    int date_x = x + DATE_PICKER_BUTTON_WIDTH + padding;
    int increase_x = date_x + date_field_width + padding;

    // The compound is three complex buttons: older year, date/current jump, newer year.
    init_change_button(&picker->buttons[DATE_PICKER_DECREASE], picker, x, change_button_y,
        ASSET_UI_MINUS_BUTTON_IDLE, decrease_click);
    init_change_button(&picker->buttons[DATE_PICKER_INCREASE], picker, increase_x, change_button_y,
        ASSET_UI_PLUS_BUTTON_IDLE, increase_click);

    complex_button *date = &picker->buttons[DATE_PICKER_DATE];
    memset(date, 0, sizeof(*date));
    complex_button_init_style(date, style);
    date->x = date_x;
    date->y = y;
    date->width = date_field_width;
    date->height = date_field_height;
    date->sequence_position = SEQUENCE_POSITION_CENTER;
    date->left_click_handler = date_click;
    date->user_data = picker;
    date->is_disabled = 1; // date button starts on the current/middle date, so it should start disabled.

    refresh_button_state(picker);
}

void widget_date_picker_draw(date_picker *picker)
{
    if (!picker) {
        return;
    }

    // The date sequence is refreshed lazily so the displayed year follows game time changes.
    refresh_button_state(picker);
    int display_year = game_time_year() - picker->selected_year_offset;
    lang_sequence_date_init_format(&picker->date, display_year, 0, 0, 0, LANG_DATE_FORMAT_YEAR);
    picker->buttons[DATE_PICKER_DATE].sequence = picker->date.sequence;

    complex_button_draw_array(picker->buttons, DATE_PICKER_BUTTON_COUNT);
}

int widget_date_picker_handle_input(date_picker *picker, const mouse *m)
{
    if (!picker || !m) {
        return 0;
    }

    // Callers read picker->changed after input and sync their own model if needed.
    picker->changed = 0;
    refresh_button_state(picker);

    return complex_button_handle_mouse_array(picker->buttons, m, DATE_PICKER_BUTTON_COUNT);
}

int widget_date_picker_handle_tooltip(date_picker *picker, tooltip_context *c)
{
    if (!picker || !c) {
        return 0;
    }

    refresh_button_state(picker);
    return complex_button_handle_tooltip_array(picker->buttons, c, DATE_PICKER_BUTTON_COUNT);
}
