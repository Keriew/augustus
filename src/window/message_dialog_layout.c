#include "message_dialog_layout.h"

#include "graphics/panel.h"
#include "graphics/text.h"

#define DIALOG_CANVAS_WIDTH 640
#define DIALOG_CANVAS_HEIGHT 480
#define DIALOG_SCREEN_MARGIN BLOCK_SIZE
#define DIALOG_MAX_WIDTH (DIALOG_CANVAS_WIDTH - 2 * DIALOG_SCREEN_MARGIN)
#define DIALOG_MAX_HEIGHT (DIALOG_CANVAS_HEIGHT - 2 * DIALOG_SCREEN_MARGIN)
#define TITLE_HORIZONTAL_MARGIN 16
#define TITLE_TOP_MARGIN 20
#define MESSAGE_HORIZONTAL_MARGIN 20
#define TITLE_MESSAGE_GAP 12

static int clamp(int value, int minimum, int maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static int round_up_to_block(int value)
{
    return (value + BLOCK_SIZE - 1) / BLOCK_SIZE * BLOCK_SIZE;
}

static int multiline_height(int lines, font_t font)
{
    int line_height = font_definition_for(font)->line_height;
    if (line_height < 11) {
        line_height = 11;
    }
    return lines * (line_height + 5);
}

void message_dialog_layout_calculate(message_dialog_layout *layout,
    const uint8_t *title, const uint8_t *message, int preferred_width,
    int minimum_width, int minimum_height, int footer_height)
{
    static const uint8_t EMPTY_TEXT[] = { 0 };
    const uint8_t *safe_title = title ? title : EMPTY_TEXT;
    const uint8_t *safe_message = message ? message : EMPTY_TEXT;

    int title_required_width = text_get_width(safe_title, FONT_LARGE_BLACK) + 2 * TITLE_HORIZONTAL_MARGIN;
    int requested_width = title_required_width > preferred_width ? title_required_width : preferred_width;
    requested_width = requested_width > minimum_width ? requested_width : minimum_width;
    layout->width = clamp(round_up_to_block(requested_width), minimum_width, DIALOG_MAX_WIDTH);

    layout->title_x = TITLE_HORIZONTAL_MARGIN;
    layout->title_y = TITLE_TOP_MARGIN;
    layout->title_width = layout->width - 2 * TITLE_HORIZONTAL_MARGIN;
    int title_lines = text_measure_multiline(safe_title, layout->title_width, FONT_LARGE_BLACK, 0);
    layout->title_height = multiline_height(title_lines, FONT_LARGE_BLACK);

    layout->message_x = MESSAGE_HORIZONTAL_MARGIN;
    layout->message_y = layout->title_y + layout->title_height + TITLE_MESSAGE_GAP;
    layout->message_width = layout->width - 2 * MESSAGE_HORIZONTAL_MARGIN;
    layout->message_lines = text_measure_multiline(safe_message, layout->message_width, FONT_NORMAL_BLACK, 0);
    layout->message_height = multiline_height(layout->message_lines, FONT_NORMAL_BLACK);

    int requested_height = layout->message_y + layout->message_height + footer_height;
    requested_height = requested_height > minimum_height ? requested_height : minimum_height;
    layout->height = clamp(round_up_to_block(requested_height), minimum_height, DIALOG_MAX_HEIGHT);
}
