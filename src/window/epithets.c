#include "epithets.h"

#include "assets/assets.h"
#include "city/constants.h"
#include "city/gods.h"
#include "core/image_group.h"
#include "core/string.h"
#include "graphics/color.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/rich_text.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "translation/translation.h"
#include "window/advisors.h"
#include "window/message_dialog.h"
#include "window/option_popup.h"

#define EPITHET_TEXT_BUFFER_SIZE 4096
#define EPITHETS_PER_GOD 3

static uint8_t epithet_text_buffer[EPITHET_TEXT_BUFFER_SIZE];

static void button_god(const struct generic_button *button);
static void button_close(int param1, int param2);
static void build_epithets_text(int god_id);

static struct {
    option_menu_item option;
    const char image_id[32];
} epithets_options[18] = {
    {
        { TR_BUILDING_GRAND_TEMPLE_CERES_DESC, TR_BUILDING_GRAND_TEMPLE_CERES_BONUS_DESC },
        "Ceres M Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_CERES_DESC_MODULE_1, TR_BUILDING_GRAND_TEMPLE_CERES_MODULE_1_DESC },
        "Ceres M Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_CERES_DESC_MODULE_2, TR_BUILDING_GRAND_TEMPLE_CERES_MODULE_2_DESC },
        "Ceres M2 Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_NEPTUNE_DESC, TR_BUILDING_GRAND_TEMPLE_NEPTUNE_BONUS_DESC },
        "Nept M2 Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_NEPTUNE_DESC_MODULE_1, TR_BUILDING_GRAND_TEMPLE_NEPTUNE_MODULE_1_DESC },
        "Nept M2 Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_NEPTUNE_DESC_MODULE_2, TR_BUILDING_GRAND_TEMPLE_NEPTUNE_MODULE_2_DESC },
        "Nept M Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_MERCURY_DESC, TR_BUILDING_GRAND_TEMPLE_MERCURY_BONUS_DESC },
        "Merc M Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_MERCURY_DESC_MODULE_1, TR_BUILDING_GRAND_TEMPLE_MERCURY_MODULE_1_DESC },
        "Merc M Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_MERCURY_DESC_MODULE_2, TR_BUILDING_GRAND_TEMPLE_MERCURY_MODULE_2_DESC },
        "Merc M2 Icon"
    }, 
    {
        { TR_BUILDING_GRAND_TEMPLE_MARS_DESC, TR_BUILDING_GRAND_TEMPLE_MARS_BONUS_DESC },
        "Mars M2 Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_MARS_DESC_MODULE_1, TR_BUILDING_GRAND_TEMPLE_MARS_MODULE_1_DESC },
        "Mars M2 Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_MARS_DESC_MODULE_2, TR_BUILDING_GRAND_TEMPLE_MARS_MODULE_2_DESC },
        "Mars M Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_VENUS_DESC, TR_BUILDING_GRAND_TEMPLE_VENUS_BONUS_DESC },
        "Venus M Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_VENUS_DESC_MODULE_1, TR_BUILDING_GRAND_TEMPLE_VENUS_MODULE_1_DESC },
        "Venus M Icon"
    },
    {
        { TR_BUILDING_GRAND_TEMPLE_VENUS_DESC_MODULE_2, TR_BUILDING_GRAND_TEMPLE_VENUS_MODULE_2_DESC },
        "Venus M2 Icon"
    },
    {
        { TR_BUILDING_PANTHEON_DESC, TR_BUILDING_PANTHEON_BONUS_DESC },
        "Panth M Icon"
    },
    {
        { TR_BUILDING_PANTHEON_DESC_MODULE_1, TR_BUILDING_PANTHEON_MODULE_1_DESC },
        "Panth M Icon"
    },
    {
        { TR_BUILDING_PANTHEON_DESC_MODULE_2, TR_BUILDING_PANTHEON_MODULE_2_DESC },
        "Panth M2 Icon"
    }
};

static int selected_god_id;

static image_button image_buttons_bottom[] = {
    {600, 338, 24, 24, IB_NORMAL, GROUP_CONTEXT_ICONS, 4, button_close, button_none, 0, 0, 1}
};

static generic_button buttons_gods_size[] = {
    {30, 56, 80, 90, button_god},
    {130, 56, 80, 90, button_god},
    {230, 56, 80, 90, button_god},
    {330, 56, 80, 90, button_god},
    {430, 56, 80, 90, button_god},
    {530, 56, 80, 90, button_god},
    {630, 56, 80, 90, button_god}
};

static unsigned int focus_button_id;
static unsigned int focus_image_button_id;

static void build_epithets_text(int god_id)
{
    uint8_t *cursor = epithet_text_buffer;
    int remaining = EPITHET_TEXT_BUFFER_SIZE - 1;

    for (int i = 0; i < EPITHETS_PER_GOD; i++) {
        const uint8_t *header = translation_for(epithets_options[god_id * EPITHETS_PER_GOD + i].option.header);
        const uint8_t *desc = translation_for(epithets_options[god_id * EPITHETS_PER_GOD + i].option.desc);

        if (i > 0) {
            if (remaining < 2) {
                break;
            }
            *cursor++ = '@';
            *cursor++ = 'P';
            remaining -= 2;
        }
        if (remaining < 2) {
            break;
        }
        *cursor++ = '@';
        *cursor++ = 'H';
        remaining -= 2;

        int n = string_length(header);
        if (n >= remaining) {
            break;
        }
        string_copy(header, cursor, remaining);
        cursor += n;
        remaining -= n;

        if (remaining < 2) {
            break;
        }
        *cursor++ = '@';
        *cursor++ = 'L';
        remaining -= 2;

        n = string_length(desc);
        if (n >= remaining) {
            break;
        }
        string_copy(desc, cursor, remaining);
        cursor += n;
        remaining -= n;
    }
    *cursor = 0;
}

static void init(void)
{
    selected_god_id = 0;
    rich_text_set_fonts(FONT_NORMAL_WHITE, FONT_NORMAL_GREEN, FONT_NORMAL_WHITE, 5);
    build_epithets_text(selected_god_id);
    rich_text_reset(0);
}

static void draw_background(void)
{
    window_advisors_draw_dialog_background();

    graphics_in_dialog();

    outer_panel_draw(0, 0, 40, 24);
    
    lang_text_draw_centered(CUSTOM_TRANSLATION, TR_WINDOW_ADVISOR_EPITHETS, 0, 15, 640, FONT_LARGE_BLACK);
    int border_image_id = assets_get_image_id("UI", "Image Border Small");
    int highlight_image_id = assets_get_image_id("UI", "Highlight");
    int base_image_id = assets_get_image_id("UI", "Pantheon_Epithet_Button_01");
    color_t border_color =  COLOR_BORDER_ORANGE;
    color_t highlight_color = COLOR_MASK_NONE;
    
    for (int god = 0; god <= MAX_GODS; god++) {
        if (god == selected_god_id) {
            button_border_draw(100 * god + 26, 52, 90, 100, 1);
            if (god == MAX_GODS) {
                image_draw_border(border_image_id, 100 * god + 30, 56, border_color);
                image_draw(base_image_id, 100 * god + 35, 61, COLOR_MASK_NONE, SCALE_NONE);
                image_draw_border(highlight_image_id, 100 * god + 35, 61, highlight_color);
            } else {
                image_draw(image_group(GROUP_PANEL_WINDOWS) + god + 21, 100 * god + 30, 56, COLOR_MASK_NONE, SCALE_NONE);
            }
        } else {
            if (god == MAX_GODS) {
                highlight_color = COLOR_BLACK;
                border_color =  COLOR_BORDER_BROWN;
                image_draw_border(border_image_id, 100 * god + 30, 56, border_color);
                image_draw(base_image_id, 100 * god + 35, 61, COLOR_MASK_NONE, SCALE_NONE);
                image_draw_border(highlight_image_id, 100 * god + 35, 61, highlight_color);
            } else {
                image_draw(image_group(GROUP_PANEL_WINDOWS) + god + 16, 100 * god + 30, 56, COLOR_MASK_NONE, SCALE_NONE);
            }
        }
    }

    inner_panel_draw(16, 170, 38, 10);
    rich_text_init(epithet_text_buffer, 24, 170, 35, 10, 0);
    rich_text_draw(epithet_text_buffer, 32, 176, 34 * BLOCK_SIZE, 9, 0);

    graphics_reset_dialog();
}


static void draw_foreground(void)
{
    graphics_in_dialog();

    rich_text_draw_scrollbar();
    image_buttons_draw(0, 0, image_buttons_bottom, 1);

    graphics_reset_dialog();
}

static void button_god(const struct generic_button *button)
{
    selected_god_id = (button - buttons_gods_size); // Calculate index based on button pointer
    build_epithets_text(selected_god_id);
    rich_text_reset(0);
    window_invalidate();
}

static void button_close(int param1, int param2)
{
    window_advisors_show();
}

static void get_tooltip(tooltip_context *c)
{
     if (!focus_image_button_id && !focus_button_id) {
        return;
    }

    c->type = TOOLTIP_BUTTON;

    // image buttons
    if (focus_image_button_id) {
        c->text_id = 2;
    }
    // gods
    switch (focus_button_id) {
        case 1: c->translation_key = TR_WINDOW_ADVISOR_EPITHETS_TOOLTIP_CERES; break;
        case 2: c->translation_key = TR_WINDOW_ADVISOR_EPITHETS_TOOLTIP_NEPTUNE; break;
        case 3: c->translation_key = TR_WINDOW_ADVISOR_EPITHETS_TOOLTIP_MERCURY; break;
        case 4: c->translation_key = TR_WINDOW_ADVISOR_EPITHETS_TOOLTIP_MARS; break;
        case 5: c->translation_key = TR_WINDOW_ADVISOR_EPITHETS_TOOLTIP_VENUS; break;
        case 6: c->translation_key = TR_WINDOW_ADVISOR_EPITHETS_TOOLTIP_PANTHEON; break;
    }
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_dialog = mouse_in_dialog(m);

    if (rich_text_handle_mouse(m_dialog)) {
        return;
    }

    int handled = image_buttons_handle_mouse(m_dialog, 0, 0, image_buttons_bottom, 1, &focus_image_button_id) |
        generic_buttons_handle_mouse(m_dialog, 0, 0, buttons_gods_size, 7, &focus_button_id);

    if (focus_image_button_id) {
        focus_button_id = 0;
    }

    if (!handled && input_go_back_requested(m, h)) {
        window_advisors_show();
    }
}

void window_epithets_show(void)
{
    window_type window = {
        WINDOW_EPITHETS,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    init();
    window_show(&window);
}
