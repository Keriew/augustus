#include "depot.h"

#include "assets/assets.h"
#include "building/building.h"
#include "building/count.h"
#include "building/data_transfer.h"
#include "building/storage.h"
#include "building/warehouse.h"
#include "city/resource.h"
#include "city/view.h"
#include "core/config.h"
#include "figure/figure.h"
#include "graphics/button.h"
#include "graphics/complex_button.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/panel.h"
#include "graphics/screen.h"
#include "graphics/scrollbar.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "translation/translation.h"
#include "widget/dropdown_button.h"
#include "window/building_info.h"

static void order_set_source(const generic_button *button);
static void order_set_destination(const generic_button *button);
static void order_set_resource(const generic_button *button);
static void order_set_condition_type(const generic_button *button);
static void order_set_condition_threshold(const generic_button *button);
static void order_set_condition_threshold_reverse(const generic_button *button);
static void set_order_resource(const generic_button *button);
static void set_camera_position(const generic_button *button);
static void goto_special_orders_on_top(const generic_button *button);
static void copy_settings(const generic_button *button);
static void paste_settings(const generic_button *button);

static void tooltip_style_changed(dropdown_button *button);

#define DEPOT_BUTTONS_X_OFFSET 32
#define DEPOT_BUTTONS_Y_OFFSET 204
#define ROW_HEIGHT 22
#define ROW_HEIGHT_RESOURCE 26
#define ROW_WIDTH_RESOURCE 193
#define MAX_VISIBLE_ROWS 15
#define MAX_RESOURCE_ROWS 24

static struct {
    unsigned int focus_button_id;
    unsigned int orders_focus_button_id;
    unsigned int resource_focus_button_id;
    unsigned int storage_building_focus_button_id;
    unsigned int storage_building_view_focus_button_id;
    unsigned int storage_building_goto_orders_focus_button_id;
    unsigned int depot_resource_focus_button_id;
    int depot_building_id;
    unsigned char available_storages;
    unsigned char secondary_storages;
    unsigned char advanced_mode;
    resource_type target_resource_id;
    pixel_area window_area;
} data;

static void on_scroll(void);
static scrollbar_type scrollbar = { 0, 0, ROW_HEIGHT * MAX_VISIBLE_ROWS, 432, MAX_VISIBLE_ROWS, on_scroll, 0, 4 };

// field values will be overwritten in window_building_draw_depot_select_source_destination
static generic_button depot_select_storage_buttons[MAX_VISIBLE_ROWS];

static generic_button depot_view_storage_buttons[] = {
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
    {0, 0, 0, ROW_HEIGHT, set_camera_position},
};

static generic_button depot_goto_storage_orders_buttons[] = {
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
    {0, 0, 0, ROW_HEIGHT, goto_special_orders_on_top},
};

static generic_button depot_select_resource_buttons[] = {
    {18, 0, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, 0, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18, ROW_HEIGHT_RESOURCE * 1, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE * 1, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18, ROW_HEIGHT_RESOURCE * 2, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE * 2, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18, ROW_HEIGHT_RESOURCE * 3, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE * 3, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18, ROW_HEIGHT_RESOURCE * 4, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE * 4, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18, ROW_HEIGHT_RESOURCE * 5, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE * 5, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18, ROW_HEIGHT_RESOURCE * 6, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE * 6, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18, ROW_HEIGHT_RESOURCE * 7, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE * 7, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18, ROW_HEIGHT_RESOURCE * 8, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE * 8, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18, ROW_HEIGHT_RESOURCE * 9, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE * 9, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18, ROW_HEIGHT_RESOURCE * 10, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE * 10, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18, ROW_HEIGHT_RESOURCE * 11, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
    {18 + ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE * 11, ROW_WIDTH_RESOURCE, ROW_HEIGHT_RESOURCE, set_order_resource},
};

static generic_button depot_order_buttons[] = {
    {100,  0, 284, ROW_HEIGHT + 4, order_set_resource, 0, 1},
    {100, 56, 284, ROW_HEIGHT, order_set_source, 0, 2},
    {100, 82, 284, ROW_HEIGHT, order_set_destination, 0, 3},
    {100, 30, 284, ROW_HEIGHT, order_set_condition_type, 0, 4},
    {384, 30,  32, ROW_HEIGHT, order_set_condition_threshold, order_set_condition_threshold_reverse, 5},
    {384, 56,  32, ROW_HEIGHT, set_camera_position},
    {384, 82,  32, ROW_HEIGHT, set_camera_position},
    {40, 112,  22, ROW_HEIGHT, copy_settings},
    {63, 112,  22, ROW_HEIGHT, paste_settings},
};
dropdown_button tooltip_style_dropdown_button;

static void setup_buttons_for_selected_depot(void)
{
    for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        depot_select_storage_buttons[i].height = ROW_HEIGHT;
        depot_select_storage_buttons[i].parameter1 = data.depot_building_id;
        depot_select_storage_buttons[i].parameter2 = 0;
        depot_view_storage_buttons[i].parameter1 = 0;
    }

    int button_index = 0;
    int storage_array_size = building_storage_get_array_size();
    unsigned int row_count = 0;
    unsigned int drawn_rows = 0;

    // Header row for active storages
    if (data.available_storages > 0 && drawn_rows < MAX_VISIBLE_ROWS && data.advanced_mode) {
        row_count++;
        if (row_count > scrollbar.scroll_position) {
            drawn_rows++; // This row doesn't have buttons, just increment drawn_rows
        }
    }

    // First pass: active storages
    for (int i = 0; i < storage_array_size && button_index < MAX_VISIBLE_ROWS; i++) {
        const data_storage *storage = building_storage_get_array_entry(i);
        building *store_building = building_get(storage->building_id);
        if (!storage->in_use || !storage->building_id || store_building->state == BUILDING_STATE_MOTHBALLED ||
             (!resource_is_food(data.target_resource_id) && store_building->type == BUILDING_GRANARY)) {
            continue;
        }
        int max_storable = building_storage_resource_max_storable(store_building, data.target_resource_id);
        if (storage->in_use && max_storable > 0) {
            row_count++;
            if (row_count <= scrollbar.scroll_position) {
                continue;
            }
            // Map button_index to the correct drawn_row position
            depot_select_storage_buttons[drawn_rows].parameter2 = storage->building_id;
            depot_view_storage_buttons[drawn_rows].parameter1 = storage->building_id;
            depot_goto_storage_orders_buttons[drawn_rows].parameter1 = storage->building_id;
            drawn_rows++;
            button_index++;
        }
    }
    if (!data.advanced_mode) {
        return;
    }
    // Add separator rows if there are secondary storages

    if (data.secondary_storages > 0 && drawn_rows < MAX_VISIBLE_ROWS) {
        row_count += 2; // Skip 2 rows for separator
        if (row_count > scrollbar.scroll_position) {
            drawn_rows += 2; // These rows don't have buttons, just increment drawn_rows
        }
    }

    // Second pass: inactive/other storages
    for (int i = 0; i < storage_array_size && button_index < MAX_VISIBLE_ROWS; i++) {
        const data_storage *storage = building_storage_get_array_entry(i);
        building *store_building = building_get(storage->building_id);
        if (!storage->building_id ||
             (!resource_is_food(data.target_resource_id) && store_building->type == BUILDING_GRANARY)) {
            continue;
        }
        // Only include inactive storages that have a valid storage_id and weren't already counted in first pass
        int max_storable = building_storage_resource_max_storable(store_building, data.target_resource_id);
        if ((max_storable == 0 || !storage->in_use || store_building->state == BUILDING_STATE_MOTHBALLED) &&
            store_building->storage_id > 0) {
            row_count++;
            if (row_count <= scrollbar.scroll_position) {
                continue;
            }
            // Map button_index to the correct drawn_row position
            depot_select_storage_buttons[drawn_rows].parameter1 = -1; // indicate secondary storage
            depot_select_storage_buttons[drawn_rows].parameter2 = storage->building_id;
            depot_view_storage_buttons[drawn_rows].parameter1 = storage->building_id;
            depot_goto_storage_orders_buttons[drawn_rows].parameter1 = storage->building_id;
            drawn_rows++;
            button_index++;
        }
    }
    scrollbar_reset(&scrollbar, 0);
}

static int total_storages(void)
{
    return data.available_storages + (data.advanced_mode ? data.secondary_storages : 0);
}

static void calculate_available_storages(int building_id)
{
    data.depot_building_id = building_id;

    building *b = building_get(data.depot_building_id);
    if (!b->data.depot.current_order.resource_type) {
        b->data.depot.current_order.resource_type = RESOURCE_MIN_FOOD;
    }
    data.target_resource_id = b->data.depot.current_order.resource_type;

    data.available_storages = 0;
    data.secondary_storages = 0;
    int has_valid_src = 0;
    int has_valid_dst = 0;
    int storage_array_size = building_storage_get_array_size();
    for (int i = 0; i < storage_array_size; i++) {

        const data_storage *storage = building_storage_get_array_entry(i);
        building *store = building_get(storage->building_id);
        int max_storable = building_storage_resource_max_storable(store, data.target_resource_id);
        int active = storage->in_use;
        if (!store || !building_id || (!resource_is_food(data.target_resource_id) && store->type == BUILDING_GRANARY)) {
            continue;
        }
        if (active && max_storable > 0) {
            data.available_storages++;
        } else if ((!active || max_storable == 0) && store->storage_id > 0) {
            if (data.advanced_mode) {
                data.secondary_storages++;  // advanced orders: allow inactive/non-used storages
            }
        }
        if ((unsigned) b->data.depot.current_order.src_storage_id == store->id) {
            building_storage_state src_state = building_storage_get_state(
                building_get(b->data.depot.current_order.src_storage_id), b->data.depot.current_order.resource_type, 0);
            has_valid_src = src_state != BUILDING_STORAGE_STATE_NOT_ACCEPTING ? 1 : 0;
        }
        if ((unsigned) b->data.depot.current_order.dst_storage_id == store->id) {
            building_storage_state dst_state = building_storage_get_state(
                building_get(b->data.depot.current_order.dst_storage_id), b->data.depot.current_order.resource_type, 0);
            has_valid_dst = dst_state != BUILDING_STORAGE_STATE_NOT_ACCEPTING ? 1 : 0;
        }
    }
    if (!has_valid_src) {
        b->data.depot.current_order.src_storage_id = 0;
    }
    if (!has_valid_dst) {
        b->data.depot.current_order.dst_storage_id = 0;
    }

}

void window_building_depot_init_main(int building_id)
{
    city_resource_determine_available(1);
    data.advanced_mode = config_get(CONFIG_GP_CH_CART_DEPOT_ADVANCED);
    calculate_available_storages(building_id);
}

void window_building_depot_init_resource_selection(void)
{
    int total_rows = (city_resource_get_potential()->size + 1) / 2;
    scrollbar_init(&scrollbar, 0, total_rows);
    int extra_width = total_rows > MAX_VISIBLE_ROWS ? 0 : 20;

    for (int i = 0; i < MAX_RESOURCE_ROWS; i++) {
        if (depot_select_resource_buttons[i].x != 18) {
            depot_select_resource_buttons[i].x = 18 + ROW_WIDTH_RESOURCE + extra_width;
        }
        depot_select_resource_buttons[i].width = ROW_WIDTH_RESOURCE + extra_width;
    }
}

static void tooltip_style_changed(dropdown_button *button)
{
    if (button->selected_index < 1 || button->selected_index > 4) {
        return;
    }
    config_set(CONFIG_UI_CART_DEPOT_TOOLTIP_STYLE, button->selected_index - 1);
    window_request_refresh();
}

static void window_building_depot_init_tooltip_style_dropdown(building_info_context *c)
{
    static lang_fragment frags[4]; // fragments array - keep static to ensure it remains valid
    for (int i = 0; i < 4; i++) {
        frags[i].type = LANG_FRAG_LABEL;
        frags[i].text_group = CUSTOM_TRANSLATION;
        frags[i].text_id = TR_BUILDING_INFO_CART_DEPOT_TOOLTIP_STYLE + i;
    }
    frags[0].text_id = config_get(CONFIG_UI_CART_DEPOT_TOOLTIP_STYLE) + TR_BUILDING_INFO_CART_DEPOT_TOOLTIP_STYLE + 1;

    int btn_width = 150;
    int dropdown_x = c->x_offset + (c->width_blocks * BLOCK_SIZE) / 2 - btn_width / 2; // Center the button
    int dropdown_y = c->y_offset + (c->height_blocks + 1) * BLOCK_SIZE - 7;
    tooltip_style_dropdown_button.width = btn_width;
    dropdown_button_init_simple(dropdown_x, dropdown_y, frags, 4, &tooltip_style_dropdown_button);
    tooltip_style_dropdown_button.selected_callback = tooltip_style_changed; // Set the callback function
}

void window_building_depot_init_storage_selection(building_info_context *c)
{
    int separator_rows = (data.secondary_storages > 0 ? 2 : 0) + (data.available_storages > 0 ? 1 : 0);
    int total_rows = data.available_storages + data.secondary_storages + separator_rows;
    if (!data.advanced_mode) {
        total_rows = data.available_storages; // for basic mode, only show active storages
    }
    window_building_depot_init_tooltip_style_dropdown(c);

    scrollbar_init(&scrollbar, 0, total_rows);
}

static void on_scroll(void)
{
    window_request_refresh();
}

void window_building_draw_depot(building_info_context *c)
{
    c->advisor_button = ADVISOR_TRADE;
    window_building_play_sound(c, "wavs/granary2.wav");
    setup_buttons_for_selected_depot();
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 138);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    const building *b = building_get(data.depot_building_id);
    translation_key depot_name_key = b->num_workers <= 0 && c->has_road_access ?
        TR_BUILDING_CAT_DEPOT : TR_BUILDING_DEPOT;
    text_draw_centered(translation_for(depot_name_key),
        c->x_offset, c->y_offset + 12, 16 * c->width_blocks, FONT_LARGE_BLACK, 0);
    data.window_area.x = c->x_offset;
    data.window_area.y = c->y_offset;
    data.window_area.width = c->width_blocks * BLOCK_SIZE;
    data.window_area.height = c->height_blocks * BLOCK_SIZE;
}

void window_building_draw_depot_foreground(building_info_context *c)
{
    building *b = building_get(data.depot_building_id);
    building *src = building_get(b->data.depot.current_order.src_storage_id);
    building *dst = building_get(b->data.depot.current_order.dst_storage_id);
    setup_buttons_for_selected_depot();
    calculate_available_storages(data.depot_building_id);
    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (b->num_workers <= 0) {
        window_building_draw_description(c, CUSTOM_TRANSLATION, TR_BUILDING_DEPOT_NO_EMPLOYEES);
    } else if (c->worker_percentage < 25) {
        window_building_draw_description(c, CUSTOM_TRANSLATION, TR_BUILDING_DEPOT_FEW_EMPLOYEES);
    } else if (c->worker_percentage < 50) {
        window_building_draw_description(c, CUSTOM_TRANSLATION, TR_BUILDING_DEPOT_SOME_EMPLOYEES);
    } else if (c->worker_percentage < 75) {
        window_building_draw_description(c, CUSTOM_TRANSLATION, TR_BUILDING_DEPOT_HALF_EMPLOYEES);
    } else if (c->worker_percentage < 100) {
        window_building_draw_description(c, CUSTOM_TRANSLATION, TR_BUILDING_DEPOT_MANY_EMPLOYEES);
    } else {
        window_building_draw_description(c, CUSTOM_TRANSLATION, TR_BUILDING_DEPOT_DESC);
    }

    resource_type resource = data.target_resource_id;
    int x_offset = c->x_offset + DEPOT_BUTTONS_X_OFFSET;
    int y_offset = c->y_offset + DEPOT_BUTTONS_Y_OFFSET;

    text_draw(translation_for(TR_FIGURE_INFO_DEPOT_DELIVER), x_offset, y_offset + 8, FONT_NORMAL_BLACK, 0);
    depot_order_buttons[0].x = 100;
    int image_id = resource_get_data(resource)->image.icon;
    const image *img = image_get(image_id);
    button_border_draw(x_offset + depot_order_buttons[0].x, y_offset + depot_order_buttons[0].y,
        depot_order_buttons[0].width, depot_order_buttons[0].height, data.focus_button_id == 1);
    image_draw(image_id,
        x_offset + depot_order_buttons[0].x + (26 - img->original.width) / 2,
        y_offset + depot_order_buttons[0].y + (26 - img->original.height) / 2, COLOR_MASK_NONE, SCALE_NONE);
    text_draw_centered(resource_get_data(resource)->text,
        x_offset + depot_order_buttons[0].x, y_offset + depot_order_buttons[0].y + 8, depot_order_buttons[0].width,
        FONT_NORMAL_BLACK, 0);
    image_draw(image_id,
        x_offset + depot_order_buttons[0].x + depot_order_buttons[0].width - 26 + (26 - img->original.width) / 2,
        y_offset + depot_order_buttons[0].y + (26 - img->original.height) / 2, COLOR_MASK_NONE, SCALE_NONE);

    order_condition_type condition_type = b->data.depot.current_order.condition.condition_type;
    text_draw(translation_for(TR_BUILDING_INFO_DEPOT_CONDITION), x_offset, y_offset + depot_order_buttons[3].y + 6, FONT_NORMAL_BLACK, 0);
    button_border_draw(x_offset + depot_order_buttons[3].x, y_offset + depot_order_buttons[3].y,
        depot_order_buttons[3].width, depot_order_buttons[3].height, data.focus_button_id == 4);
    text_draw_centered(translation_for(TR_ORDER_CONDITION_NEVER + condition_type),
        x_offset + depot_order_buttons[3].x, y_offset + depot_order_buttons[3].y + 6, depot_order_buttons[3].width, FONT_NORMAL_BLACK, 0);
    if (condition_type != ORDER_CONDITION_ALWAYS && condition_type != ORDER_CONDITION_NEVER) {
        button_border_draw(x_offset + depot_order_buttons[4].x, y_offset + depot_order_buttons[4].y,
            depot_order_buttons[4].width, depot_order_buttons[4].height, data.focus_button_id == 5);
        text_draw_number_centered(b->data.depot.current_order.condition.threshold,
            x_offset + depot_order_buttons[4].x, y_offset + depot_order_buttons[4].y + 6, depot_order_buttons[4].width, FONT_NORMAL_BLACK);
    }

    text_draw(translation_for(TR_BUILDING_INFO_DEPOT_SOURCE), x_offset, y_offset + depot_order_buttons[1].y + 6, FONT_NORMAL_BLACK, 0);
    int focus_border = data.focus_button_id == 2 && total_storages() > 1;
    button_border_draw(x_offset + depot_order_buttons[1].x, y_offset + depot_order_buttons[1].y,
        depot_order_buttons[1].width, depot_order_buttons[1].height, focus_border);
    if (src->storage_id) {
        text_draw_label_and_number_centered(lang_get_string(28, src->type),
            src->storage_id, "", x_offset + depot_order_buttons[1].x, y_offset + depot_order_buttons[1].y + 6,
            depot_order_buttons[1].width, FONT_NORMAL_BLACK, 0);
        button_border_draw(x_offset + depot_order_buttons[5].x, y_offset + depot_order_buttons[5].y,
            depot_order_buttons[5].width, depot_order_buttons[5].height, data.focus_button_id == 6);
        image_draw(assets_lookup_image_id(ASSET_CENTER_CAMERA_ON_BUILDING), x_offset + depot_order_buttons[5].x + 3,
            y_offset + depot_order_buttons[5].y + 3, COLOR_FONT_PLAIN, SCALE_NONE);
        depot_order_buttons[5].parameter1 = src->id;
    } else if (total_storages() > 1) {
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_BUILDING_INFO_DEPOT_SELECT_SOURCE,
            x_offset + depot_order_buttons[1].x, y_offset + depot_order_buttons[1].y + 6,
            depot_order_buttons[1].width, FONT_NORMAL_BLACK);
    } else {
        translation_key translation;
        translation = total_storages() == 1 ?
            TR_BUILDING_INFO_DEPOT_ONE_BUILDING_FOR_RESOURCE : TR_BUILDING_INFO_DEPOT_NO_SOURCE_AVAILABLE;
        text_draw_centered(lang_get_string(CUSTOM_TRANSLATION, translation),
            x_offset + depot_order_buttons[1].x, y_offset + depot_order_buttons[1].y + 6,
            depot_order_buttons[1].width, FONT_NORMAL_PLAIN, COLOR_FONT_GRAY);
    }

    text_draw(translation_for(TR_BUILDING_INFO_DEPOT_DESTINATION), x_offset, y_offset + depot_order_buttons[2].y + 6, FONT_NORMAL_BLACK, 0);
    button_border_draw(x_offset + depot_order_buttons[2].x, y_offset + depot_order_buttons[2].y,
        depot_order_buttons[2].width, depot_order_buttons[2].height,
        data.focus_button_id == 3 && total_storages() > 1);
    if (dst->storage_id) {
        text_draw_label_and_number_centered(lang_get_string(28, dst->type),
            dst->storage_id, "", x_offset + depot_order_buttons[2].x, y_offset + depot_order_buttons[2].y + 6,
            depot_order_buttons[2].width, FONT_NORMAL_BLACK, 0);
        button_border_draw(x_offset + depot_order_buttons[6].x, y_offset + depot_order_buttons[6].y,
            depot_order_buttons[6].width, depot_order_buttons[6].height, data.focus_button_id == 7);
        image_draw(assets_lookup_image_id(ASSET_CENTER_CAMERA_ON_BUILDING), x_offset + depot_order_buttons[6].x + 3,
            y_offset + depot_order_buttons[6].y + 3, COLOR_FONT_PLAIN, SCALE_NONE);
        depot_order_buttons[6].parameter1 = dst->id;
    } else if (total_storages() > 1) {
        lang_text_draw_centered(CUSTOM_TRANSLATION, TR_BUILDING_INFO_DEPOT_SELECT_DESTINATION,
            x_offset + depot_order_buttons[2].x, y_offset + depot_order_buttons[2].y + 6,
            depot_order_buttons[2].width, FONT_NORMAL_BLACK);
    } else {
        translation_key translation = total_storages() == 1 ? TR_BUILDING_INFO_DEPOT_ONE_BUILDING_FOR_RESOURCE :
            TR_BUILDING_INFO_DEPOT_NO_DESTINATION_AVAILABLE;
        text_draw_centered(lang_get_string(CUSTOM_TRANSLATION, translation),
            x_offset + depot_order_buttons[2].x, y_offset + depot_order_buttons[2].y + 6,
            depot_order_buttons[1].width, FONT_NORMAL_PLAIN, COLOR_FONT_GRAY);
    }
    //copy and paste buttons
    if (data.advanced_mode) {
        button_border_draw(x_offset + depot_order_buttons[7].x, y_offset + depot_order_buttons[7].y,
            depot_order_buttons[7].width, depot_order_buttons[7].height, data.focus_button_id == 8);
        image_draw(assets_lookup_image_id(ASSET_UI_COPY_ICON), x_offset + depot_order_buttons[7].x + 3,
            y_offset + depot_order_buttons[7].y + 3, COLOR_FONT_PLAIN, SCALE_NONE);
        button_border_draw(x_offset + depot_order_buttons[8].x, y_offset + depot_order_buttons[8].y,
            depot_order_buttons[8].width, depot_order_buttons[8].height, data.focus_button_id == 9);
        image_draw(assets_lookup_image_id(ASSET_UI_PASTE_ICON), x_offset + depot_order_buttons[8].x + 3,
            y_offset + depot_order_buttons[8].y + 3, COLOR_FONT_PLAIN, SCALE_NONE);
    }
}

int window_building_handle_mouse_depot(const mouse *m, building_info_context *c)
{
    return generic_buttons_handle_mouse(m, c->x_offset + DEPOT_BUTTONS_X_OFFSET, c->y_offset + DEPOT_BUTTONS_Y_OFFSET,
        depot_order_buttons, data.advanced_mode ? 9 : 7, &data.focus_button_id);
}

static void order_set_source(const generic_button *button)
{
    if (total_storages() > 1) {
        window_building_info_depot_select_source();
    }
}

static void order_set_destination(const generic_button *button)
{
    if (total_storages() > 1) {
        window_building_info_depot_select_destination();
    }
}

static void order_set_condition_type(const generic_button *button)
{
    window_building_info_depot_toggle_condition_type();
}

static void order_set_condition_threshold(const generic_button *button)
{
    window_building_info_depot_toggle_condition_threshold();
}

static void order_set_condition_threshold_reverse(const generic_button *button)
{
    window_building_info_depot_toggle_condition_threshold_reverse();
}

void window_building_draw_depot_order_source_destination_background(building_info_context *c, int is_select_destination)
{
    setup_buttons_for_selected_depot();

    const uint8_t *title = translation_for(TR_BUILDING_INFO_DEPOT_SELECT_SOURCE_TITLE);
    if (is_select_destination) {
        title = translation_for(TR_BUILDING_INFO_DEPOT_SELECT_DESTINATION_TITLE);
    }
    int y_offset = window_building_get_vertical_offset(c, 28);
    c->help_id = 0;
    outer_panel_draw(c->x_offset, y_offset, 29, 28);
    text_draw_centered(title, c->x_offset, y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, 0);
    inner_panel_draw(c->x_offset + 16, y_offset + 42, c->width_blocks - 2, 21);
    data.window_area.x = c->x_offset;
    data.window_area.y = y_offset;
    data.window_area.width = 29 * BLOCK_SIZE;
    data.window_area.height = 28 * BLOCK_SIZE;
}

void window_building_draw_depot_select_source_destination(building_info_context *c)
{
    int y_offset = window_building_get_vertical_offset(c, 28);

    scrollbar.x = c->x_offset + 16 * (c->width_blocks - 2) - 26;
    scrollbar.y = y_offset + 46;
    scrollbar_draw(&scrollbar);

    int base_width = BLOCK_SIZE * (c->width_blocks - 4) - 4 - (scrollbar.max_scroll_position > 0 ? 39 : 0) - BLOCK_SIZE * 2;
    int storage_array_size = building_storage_get_array_size();
    unsigned int row_count = 0;
    unsigned int drawn_rows = 0;

    // header/separator for active storages
    if (data.available_storages > 0 && drawn_rows < MAX_VISIBLE_ROWS && data.advanced_mode) {
        row_count++;; // skip 1 row for active storage header
        if (row_count > scrollbar.scroll_position) {
            lang_text_draw_centered(CUSTOM_TRANSLATION, TR_BUILDING_INFO_ACTIVE_STORAGE_BUILDINGS,
                c->x_offset + 18 + BLOCK_SIZE * 2, y_offset + 50 + ROW_HEIGHT * drawn_rows, base_width,
                FONT_NORMAL_WHITE);
            drawn_rows++;
        }
    }
    // First pass: draw active storages
    for (int i = 0; i < storage_array_size && drawn_rows < MAX_VISIBLE_ROWS; i++) {
        const data_storage *storage = building_storage_get_array_entry(i);
        building *store_building = building_get(storage->building_id);
        if (!storage->in_use || !storage->building_id || store_building->state == BUILDING_STATE_MOTHBALLED ||
            (!resource_is_food(data.target_resource_id) && store_building->type == BUILDING_GRANARY)) {
            continue;
        }

        int max_storable = building_storage_resource_max_storable(store_building, data.target_resource_id);
        if (storage->in_use && max_storable > 0) {
            row_count++;
            if (row_count <= scrollbar.scroll_position) {
                continue;
            }

            building *bld = store_building;
            // Left button - goto storage orders
            button_border_draw(c->x_offset + 18, y_offset + 46 + ROW_HEIGHT * drawn_rows, BLOCK_SIZE * 2, 22,
                data.storage_building_goto_orders_focus_button_id == drawn_rows + 1);
            image_draw(assets_lookup_image_id(ASSET_UI_GEAR_ICON),
                c->x_offset + 28, y_offset + 50 + ROW_HEIGHT * drawn_rows, COLOR_FONT_PLAIN, SCALE_NONE);
            image_draw(assets_lookup_image_id(ASSET_UI_GEAR_ICON),
                c->x_offset + 27, y_offset + 49 + ROW_HEIGHT * drawn_rows, COLOR_MASK_NONE, SCALE_NONE);

            // Middle button - select storage
            button_border_draw(c->x_offset + 18 + BLOCK_SIZE * 2, y_offset + 46 + ROW_HEIGHT * drawn_rows, base_width,
                22, data.storage_building_focus_button_id == drawn_rows + 1);
            text_draw_label_and_number_centered(
                lang_get_string(28, bld->type), bld->storage_id, "", c->x_offset + 18 + BLOCK_SIZE * 2,
                y_offset + 52 + ROW_HEIGHT * drawn_rows, base_width, FONT_NORMAL_WHITE, 0);

            // Right button - view storage
            button_border_draw(c->x_offset + 18 + base_width + BLOCK_SIZE * 2, y_offset + 46 + ROW_HEIGHT * drawn_rows,
                BLOCK_SIZE * 2, 22, data.storage_building_view_focus_button_id == drawn_rows + 1);
            image_draw(
                assets_lookup_image_id(ASSET_CENTER_CAMERA_ON_BUILDING), c->x_offset + 22 + base_width + BLOCK_SIZE * 2,
                y_offset + 50 + ROW_HEIGHT * drawn_rows, COLOR_FONT_PLAIN, SCALE_NONE);
            image_draw(
                assets_lookup_image_id(ASSET_CENTER_CAMERA_ON_BUILDING), c->x_offset + 21 + base_width + BLOCK_SIZE * 2,
                y_offset + 49 + ROW_HEIGHT * drawn_rows, COLOR_MASK_NONE, SCALE_NONE);

            drawn_rows++;
        }
    }
    if (!data.advanced_mode) {
        int label_x = c->x_offset + 45;
        int label_y = c->y_offset + (c->height_blocks + 1) * BLOCK_SIZE - 3; // Same line as dropdown
        lang_text_draw_ellipsized(CUSTOM_TRANSLATION, TR_BUILDING_INFO_CART_DEPOT_TOOLTIP_STYLE,
            label_x, label_y, tooltip_style_dropdown_button.buttons[0].x - label_x, FONT_NORMAL_BLACK);
        dropdown_button_draw(&tooltip_style_dropdown_button);
        return;
    }
    // Draw separator if we have inactive storages
    if (data.secondary_storages > 0 && drawn_rows < MAX_VISIBLE_ROWS) {
        row_count += 2; //skip two rows for inactive storage header
        if (row_count > scrollbar.scroll_position) {
            drawn_rows++;
            lang_text_draw_centered(CUSTOM_TRANSLATION, TR_BUILDING_INFO_OTHER_STORAGE_BUILDINGS,
                c->x_offset + 18 + BLOCK_SIZE * 2, y_offset + 50 + ROW_HEIGHT * drawn_rows, base_width, FONT_NORMAL_RED);
            drawn_rows++;
        }
    }

    // Second pass: draw inactive storages
    for (int i = 0; i < storage_array_size && drawn_rows < MAX_VISIBLE_ROWS; i++) {
        const data_storage *storage = building_storage_get_array_entry(i);
        building *store_building = building_get(storage->building_id);
        if (!storage->in_use || !storage->building_id ||
            (!resource_is_food(data.target_resource_id) && store_building->type == BUILDING_GRANARY)) {
            continue;
        }

        // Only include inactive storages that have a valid storage_id and weren't already counted in first pass
        int max_storable = building_storage_resource_max_storable(store_building, data.target_resource_id);
        if ((max_storable == 0 || !storage->in_use || store_building->state == BUILDING_STATE_MOTHBALLED)
            && store_building->storage_id > 0) {
            row_count++;
            if (row_count <= scrollbar.scroll_position) {
                continue;
            }
            // TODO: come up with a better way to deal with standard buttons drawing
            building *bld = store_building;
            // Left button - goto storage orders
            button_border_draw(c->x_offset + 18, y_offset + 46 + ROW_HEIGHT * drawn_rows, BLOCK_SIZE * 2, 22,
                data.storage_building_goto_orders_focus_button_id == drawn_rows + 1);
            image_draw(assets_lookup_image_id(ASSET_UI_GEAR_ICON),
                c->x_offset + 28, y_offset + 50 + ROW_HEIGHT * drawn_rows, COLOR_FONT_PLAIN, SCALE_NONE);
            image_draw(assets_lookup_image_id(ASSET_UI_GEAR_ICON),
                c->x_offset + 27, y_offset + 49 + ROW_HEIGHT * drawn_rows, COLOR_MASK_NONE, SCALE_NONE);

            // Middle button - select storage (disabled for inactive storages)
            button_border_draw(c->x_offset + 18 + BLOCK_SIZE * 2, y_offset + 46 + ROW_HEIGHT * drawn_rows,
                base_width, 22, 0);
            text_draw_label_and_number_centered(lang_get_string(28, bld->type), bld->storage_id, "",
                c->x_offset + 18 + BLOCK_SIZE * 2, y_offset + 52 + ROW_HEIGHT * drawn_rows, base_width,
                FONT_NORMAL_BLACK, COLOR_FONT_LIGHT_GRAY);

            // Right button - view storage - center camera on storage
            button_border_draw(c->x_offset + 18 + base_width + BLOCK_SIZE * 2, y_offset + 46 + ROW_HEIGHT * drawn_rows,
                BLOCK_SIZE * 2, 22, data.storage_building_view_focus_button_id == drawn_rows + 1);
            image_draw(
                assets_lookup_image_id(ASSET_CENTER_CAMERA_ON_BUILDING), c->x_offset + 22 + base_width + BLOCK_SIZE * 2,
                y_offset + 50 + ROW_HEIGHT * drawn_rows, COLOR_FONT_PLAIN, SCALE_NONE);
            image_draw(
                assets_lookup_image_id(ASSET_CENTER_CAMERA_ON_BUILDING), c->x_offset + 21 + base_width + BLOCK_SIZE * 2,
                y_offset + 49 + ROW_HEIGHT * drawn_rows, COLOR_MASK_NONE, SCALE_NONE);

            drawn_rows++;
        }
    }

    int label_x = c->x_offset + 45;
    int label_y = c->y_offset + (c->height_blocks + 1) * BLOCK_SIZE - 3; // Same line as dropdown
    lang_text_draw_ellipsized(CUSTOM_TRANSLATION, TR_BUILDING_INFO_CART_DEPOT_TOOLTIP_STYLE,
        label_x, label_y, tooltip_style_dropdown_button.buttons[0].x - label_x, FONT_NORMAL_BLACK);
    dropdown_button_draw(&tooltip_style_dropdown_button);
}

static void set_order_source(const generic_button *button)
{
    int depot_building_id = button->parameter1;
    int building_id = button->parameter2;
    if (depot_building_id == -1) { //ineligible storage
        return;
    }
    if (!building_id) {
        return;
    }
    building *b = building_get(depot_building_id);
    b->data.depot.current_order.src_storage_id = building_id;
    if (b->data.depot.current_order.dst_storage_id == building_id) {
        b->data.depot.current_order.dst_storage_id = 0;
    }
    window_building_info_depot_return_to_main_window();
}

static void set_order_destination(const generic_button *button)
{
    int depot_building_id = button->parameter1;
    int building_id = button->parameter2;
    if (depot_building_id == -1 || !building_id) { //ineligible storage hack
        return;
    }
    building *b = building_get(depot_building_id);
    b->data.depot.current_order.dst_storage_id = building_id;
    if (b->data.depot.current_order.src_storage_id == building_id) {
        b->data.depot.current_order.src_storage_id = 0;
    }
    window_building_info_depot_return_to_main_window();
}

static void set_camera_position(const generic_button *button)
{
    int building_id = button->parameter1;
    const building *b = building_get(building_id);
    if (!b || b->id == 0) {
        return;
    }
    city_view_go_to_grid_offset(b->grid_offset);
    city_view_adjust_camera_from_obstruction(b->grid_offset, 3, &data.window_area);
    window_request_refresh();
}

static void copy_settings(const generic_button *button)
{
    building *b = building_get(data.depot_building_id);
    building_data_transfer_copy(b);
    calculate_available_storages(data.depot_building_id);
    setup_buttons_for_selected_depot();
    window_invalidate();
}

static void paste_settings(const generic_button *button)
{
    building *b = building_get(data.depot_building_id);
    building_data_transfer_paste(b);
    calculate_available_storages(data.depot_building_id);
    setup_buttons_for_selected_depot();
    window_invalidate();
}


static void goto_special_orders_on_top(const generic_button *button)
{
    window_building_info_show_storage_special_orders_on_top(button->parameter1);
}

static int handle_mouse_depot_select_source_destination(const mouse *m, building_info_context *c, int is_source)
{
    if (scrollbar_handle_mouse(&scrollbar, m, 1)) {
        return 1;
    }
    if (dropdown_button_handle_mouse(m, &tooltip_style_dropdown_button)) {
        return 1;
    }
    int y_offset = window_building_get_vertical_offset(c, 28);
    for (int i = 0; i < MAX_VISIBLE_ROWS; i++) {
        // Left button - goto storage orders
        depot_goto_storage_orders_buttons[i].x = 0;
        depot_goto_storage_orders_buttons[i].y = ROW_HEIGHT * i;
        depot_goto_storage_orders_buttons[i].width = BLOCK_SIZE * 2;
        depot_goto_storage_orders_buttons[i].height = ROW_HEIGHT;

        // Middle button - select storage (adjusted x position to make room for left button)
        depot_select_storage_buttons[i].x = BLOCK_SIZE * 2;
        depot_select_storage_buttons[i].y = ROW_HEIGHT * i;
        depot_select_storage_buttons[i].width = BLOCK_SIZE * (c->width_blocks - 4) - 4 -
            (scrollbar.max_scroll_position > 0 ? 39 : 0) - BLOCK_SIZE * 2;
        depot_select_storage_buttons[i].height = ROW_HEIGHT;
        depot_select_storage_buttons[i].left_click_handler = is_source ? set_order_source : set_order_destination;

        // Right button - view storage (adjusted x position)
        depot_view_storage_buttons[i].x = BLOCK_SIZE * (c->width_blocks - 4) - 4 -
            (scrollbar.max_scroll_position > 0 ? 39 : 0);
        depot_view_storage_buttons[i].y = ROW_HEIGHT * i;
        depot_view_storage_buttons[i].width = BLOCK_SIZE * 2;
        depot_view_storage_buttons[i].height = ROW_HEIGHT;
    }

    return generic_buttons_handle_mouse(m, c->x_offset + 18, y_offset + 46, depot_goto_storage_orders_buttons,
        MAX_VISIBLE_ROWS, &data.storage_building_goto_orders_focus_button_id) ||
        generic_buttons_handle_mouse(m, c->x_offset + 18, y_offset + 46, depot_select_storage_buttons,
        MAX_VISIBLE_ROWS, &data.storage_building_focus_button_id) ||
        generic_buttons_handle_mouse(m, c->x_offset + 18, y_offset + 46, depot_view_storage_buttons,
        MAX_VISIBLE_ROWS, &data.storage_building_view_focus_button_id);
}

int window_building_handle_mouse_depot_select_source(const mouse *m, building_info_context *c)
{
    return handle_mouse_depot_select_source_destination(m, c, 1);
}

int window_building_handle_mouse_depot_select_destination(const mouse *m, building_info_context *c)
{
    return handle_mouse_depot_select_source_destination(m, c, 0);
}

static void order_set_resource(const generic_button *button)
{
    window_building_info_depot_select_resource();
}

void window_building_depot_get_tooltip_main(int *translation)
{

    if (data.focus_button_id < 5) {
        return;
    }
    if (data.focus_button_id == 5) {
        *translation = TR_TOOLTIP_RIGHT_CLICK_TO_DECREASE;
    }
    const building *depot = building_get(data.depot_building_id);
    if (!depot) {
        return;
    }
    if ((data.focus_button_id == 6 && depot->data.depot.current_order.src_storage_id) ||
        (data.focus_button_id == 7 && depot->data.depot.current_order.dst_storage_id)) {
        *translation = TR_TOOLTIP_BUTTON_CENTER_CAMERA;
    }
    if (data.focus_button_id == 8) {
        *translation = TR_HOTKEY_COPY_SETTINGS;
    }
    if (data.focus_button_id == 9) {
        *translation = TR_HOTKEY_PASTE_SETTINGS;
    }
}

const uint8_t *window_building_depot_get_tooltip_source_destination(int *translation, int *group_id)
{
    if (data.storage_building_view_focus_button_id &&
        depot_select_storage_buttons[data.storage_building_view_focus_button_id - 1].parameter2) {
        *translation = TR_TOOLTIP_BUTTON_CENTER_CAMERA;
        *group_id = CUSTOM_TRANSLATION;
        return 0;
    }
    if (data.storage_building_goto_orders_focus_button_id &&
        depot_goto_storage_orders_buttons[data.storage_building_goto_orders_focus_button_id - 1].parameter1) {
        *translation = TR_TOOLTIP_GOTO_STORAGE_INSTRUCTIONS;
        *group_id = CUSTOM_TRANSLATION;
    }
    // For storage selection buttons, show storage summary tooltip
    if (data.storage_building_focus_button_id &&
        depot_select_storage_buttons[data.storage_building_focus_button_id - 1].parameter2) {
        int storage_building_id = depot_select_storage_buttons[data.storage_building_focus_button_id - 1].parameter2;
        building *storage_building = building_get(storage_building_id);
        if (storage_building) {
            static char tooltip_buffer[1024];
            storage_summary_style tooltip_style = config_get(CONFIG_UI_CART_DEPOT_TOOLTIP_STYLE);
            if (building_storage_summary_tooltip(storage_building, tooltip_buffer, sizeof(tooltip_buffer), tooltip_style)) {
                return (const uint8_t *) tooltip_buffer;
            }
        }
    }
    return 0;
}

static void set_order_resource(const generic_button *button)
{
    int depot_building_id = button->parameter1;
    resource_type resource_id = button->parameter2;
    if (resource_id >= RESOURCE_MIN && resource_id < RESOURCE_MAX && resource_is_storable(resource_id)) {
        building *b = building_get(depot_building_id);
        b->data.depot.current_order.resource_type = resource_id;
        calculate_available_storages(depot_building_id);
        window_building_info_depot_return_to_main_window();
    }
}

void window_building_draw_depot_select_resource(building_info_context *c)
{
    int y_offset = window_building_get_vertical_offset(c, 28);
    c->help_id = 0;
    outer_panel_draw(c->x_offset, y_offset, 29, 28);
    text_draw_centered(translation_for(TR_BUILDING_INFO_DEPOT_SELECT_RESOURCE_TITLE),
        c->x_offset, y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, 0);
    inner_panel_draw(c->x_offset + 16, y_offset + 42, c->width_blocks - 2, 21);
    data.window_area.x = c->x_offset;
    data.window_area.y = y_offset;
    data.window_area.width = 29 * BLOCK_SIZE;
    data.window_area.height = 28 * BLOCK_SIZE;
}

void window_building_draw_depot_select_resource_foreground(building_info_context *c)
{
    int y_offset = window_building_get_vertical_offset(c, 28);

    scrollbar.x = c->x_offset + 16 * (c->width_blocks - 2) - 26;
    scrollbar.y = y_offset + 46;
    scrollbar_draw(&scrollbar);

    for (int i = 0; i < MAX_RESOURCE_ROWS; i++) {
        depot_select_resource_buttons[i].parameter2 = 0;
    }

    const resource_list *list = city_resource_get_potential();
    unsigned int list_index = scrollbar.scroll_position * 2; // Two items per scroll bar step

    for (unsigned int i = 0; i < MAX_RESOURCE_ROWS && list_index < list->size; i++, list_index++) {
        resource_type resource_id = list->items[list_index];
        int image_id = resource_get_data(resource_id)->image.icon;
        const uint8_t *str = resource_get_data(resource_id)->text;
        depot_select_resource_buttons[i].parameter2 = resource_id;
        button_border_draw(c->x_offset + depot_select_resource_buttons[i].x,
            y_offset + 46 + depot_select_resource_buttons[i].y,
            depot_select_resource_buttons[i].width, depot_select_resource_buttons[i].height,
            data.depot_resource_focus_button_id == i + 1);
        image_draw(image_id, c->x_offset + depot_select_resource_buttons[i].x + 3,
            y_offset + 46 + depot_select_resource_buttons[i].y + 3, COLOR_MASK_NONE, SCALE_NONE);
        text_draw(str, c->x_offset + depot_select_resource_buttons[i].x + 33,
            y_offset + 46 + depot_select_resource_buttons[i].y + 8, FONT_NORMAL_WHITE, 0);
    }
}

int window_building_handle_mouse_depot_select_resource(const mouse *m, building_info_context *c)
{
    if (scrollbar_handle_mouse(&scrollbar, m, 1)) {
        return 1;
    }
    for (int i = 0; i < sizeof(depot_select_resource_buttons) / sizeof(generic_button); i++) {
        depot_select_resource_buttons[i].parameter1 = data.depot_building_id;
    }
    int y_offset = window_building_get_vertical_offset(c, 28) + 46;
    return generic_buttons_handle_mouse(m, c->x_offset, y_offset, depot_select_resource_buttons,
        MAX_RESOURCE_ROWS, &data.depot_resource_focus_button_id);
}
