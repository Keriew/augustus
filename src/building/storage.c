#include "storage.h"

#include "building/building.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "building/type.h"
#include "city/resource.h"
#include "core/array.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/log.h"
#include "core/string.h"
#include "city/resource.h"
#include "empire/city.h"
#include "game/resource.h"
#include "game/save_version.h"
#include "graphics/text.h"

#define STORAGE_ARRAY_SIZE_STEP 200

#define STORAGE_ORIGINAL_BUFFER_SIZE 32
#define STORAGE_STATIC_BUFFER_SIZE 10
#define STORAGE_CURRENT_BUFFER_SIZE (STORAGE_STATIC_BUFFER_SIZE + RESOURCE_MAX * 2)

static array(data_storage) storages;

static void storage_create(data_storage *storage, unsigned int position)
{
    storage->id = position;
}

static int storage_in_use(const data_storage *storage)
{
    return storage->in_use;
}

void building_storage_clear_all(void)
{
    if (!array_init(storages, STORAGE_ARRAY_SIZE_STEP, storage_create, storage_in_use) ||
        !array_next(storages)) { // Ignore first storage
        log_error("Unable to create storages. The game will likely crash.", 0, 0);
    }
}

int building_storage_get_array_size(void)
{
    return storages.size;
}

int building_storage_try_add_resource(building *b, int resource, int amount, int is_produced)
{
    if (b->type == BUILDING_GRANARY) {
        return building_granary_try_add_resource(b, resource, amount, is_produced, 1);
    } else if (b->type == BUILDING_WAREHOUSE) {
        return building_warehouse_try_add_resource(b, resource, amount, 1);
    }
    return 0;
}

void building_storage_reset_building_ids(void)
{
    data_storage *storage;
    array_foreach(storages, storage)
    {
        storage->building_id = 0;
    }

    static const building_type types[] = { BUILDING_GRANARY, BUILDING_WAREHOUSE };

    for (int i = 0; i < 2; i++) {
        building_type type = types[i];
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_UNUSED) {
                continue;
            }
            if (b->storage_id) {
                if (array_item(storages, b->storage_id)->building_id) {
                    // storage is already connected to a building: corrupt, create new
                    b->storage_id = building_storage_create(b->id);
                } else {
                    array_item(storages, b->storage_id)->building_id = b->id;
                }
            }
        }
    }
}

int building_storage_create(int building_id)
{
    data_storage *storage;
    array_new_item_after_index(storages, 1, storage);
    if (!storage) {
        return 0;
    }
    storage->in_use = 1;
    storage->building_id = building_id;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        //default the storage quantity to max
        storage->storage.resource_state[r].quantity = BUILDING_STORAGE_QUANTITY_MAX;
    }

    if (config_get(CONFIG_GP_CH_WAREHOUSES_DONT_ACCEPT)) {
        building_storage_accept_none(storage->id);
    } else {
        building_storage_accept_all(storage->id);
    }
    return storage->id;
}

int building_storage_get_building_id(int storage_id)
{
    if (storage_id < 0 || storage_id >= storages.size) {
        return 0;
    }
    return array_item(storages, storage_id)->building_id;
}

int building_storage_restore(int storage_id)
{
    if (array_item(storages, storage_id)->in_use) {
        return 0;
    }
    array_item(storages, storage_id)->in_use = 1;
    if (storage_id >= storages.size) {
        storages.size = storage_id + 1;
    }
    return storage_id;
}

void building_storage_delete(int storage_id)
{
    array_item(storages, storage_id)->in_use = 0;
    array_trim(storages);
}

const building_storage *building_storage_get(int storage_id)
{
    return &array_item(storages, storage_id)->storage;
}

const data_storage *building_storage_get_array_entry(int storage_id)
{
    return array_item(storages, storage_id);
}

void building_storage_set_data(int storage_id, building_storage new_data)
{
    array_item(storages, storage_id)->storage = new_data;
}

void building_storage_toggle_empty_all(int storage_id)
{
    array_item(storages, storage_id)->storage.empty_all ^= 1;
}

int building_storage_get_empty_all(int building_id)
{
    building *b = building_get(building_id);
    int storage_id = b->storage_id;
    if (storage_id < 0 || storage_id >= storages.size) {
        return 0;
    }
    return array_item(storages, storage_id)->storage.empty_all;
}

int building_storage_count_stored_resource_types(int building_id)
{
    building *b = building_get(building_id);
    if (!b->storage_id) {
        return 0;
    }
    int stored_types_count = 0;
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (b->resources[r] > 0) {
            stored_types_count++;
        }
    }
    return stored_types_count;
}

int building_storage_get_amount(building *b, resource_type resource)
{
    if (b->type == BUILDING_GRANARY) {
        return building_granary_get_amount(b, resource);
    } else if (b->type == BUILDING_WAREHOUSE) {
        return building_warehouse_get_amount(b, resource);
    }
    return 0;
}

int building_storage_get_storage_state_quantity(building *b, resource_type resource)
{
    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];
    return entry->quantity;
}

const building_storage_state building_storage_get_state(building *b, int resource, int relative)
{
    if (b->has_plague || (b->state != BUILDING_STATE_IN_USE && b->state != BUILDING_STATE_CREATED)) {
        return BUILDING_STORAGE_STATE_NOT_ACCEPTING;
    }
    if (b->type != BUILDING_GRANARY && b->type != BUILDING_WAREHOUSE) { //safeguard
        return BUILDING_STORAGE_STATE_NOT_ACCEPTING;
    }
    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];

    if (!relative) {
        // If relative is 0, return raw state without checking amounts
        return entry->state;
    }
    int amount = (b->type == BUILDING_WAREHOUSE) ?
        building_warehouse_get_amount(b, resource) : building_granary_get_amount(b, resource);

    switch (entry->state) {
        case BUILDING_STORAGE_STATE_ACCEPTING:
            if (amount < entry->quantity) {
                return BUILDING_STORAGE_STATE_ACCEPTING;
            }
            break;

        case BUILDING_STORAGE_STATE_GETTING:
            if (amount <= entry->quantity) {
                return BUILDING_STORAGE_STATE_GETTING;
            }
            break;

        case BUILDING_STORAGE_STATE_MAINTAINING:
            if (amount <= entry->quantity) {
                return BUILDING_STORAGE_STATE_MAINTAINING;
            }
            break;

        default:
            break;
    }
    return BUILDING_STORAGE_STATE_NOT_ACCEPTING;
}

resource_type building_storage_get_highest_quantity_resource(building *b)
{
    unsigned char i;
    unsigned char highest_resource = RESOURCE_NONE;
    if (b->type == BUILDING_WAREHOUSE) {
        building_warehouse_recount_resources(b);
    }
    for (i = RESOURCE_NONE + 1; i < RESOURCE_MAX; i++) { //not interested in RESOURCE_NONE
        if (b->resources[i] > highest_resource) {
            highest_resource = i;
        }
    }
    return highest_resource;
}

void building_storage_cycle_resource_state(int storage_id, resource_type resource_id, int reverse_order)
{
    resource_storage_entry *entry = &array_item(storages, storage_id)->storage.resource_state[resource_id];
    int num_states = BUILDING_STORAGE_STATE_MAX;

    if (reverse_order) {
        entry->state = (entry->state - 1 + num_states) % num_states;
    } else {
        entry->state = (entry->state + 1) % num_states;
    }
}

void building_storage_toggle_permission(building_storage_permission_states p, building *b)
{
    int permission_bit = 1 << p;
    array_item(storages, b->storage_id)->storage.permissions ^= permission_bit;
}

int building_storage_get_permission(building_storage_permission_states p, building *b)
{
    const building_storage *s = building_storage_get(b->storage_id);
    int permission_bit = 1 << p;
    return !(s->permissions & permission_bit);
}

void building_storage_set_permission(building_storage_permission_states p, building *b, int enable)
{
    int permission_bit = 1 << p;
    int *permissions = &array_item(storages, b->storage_id)->storage.permissions;

    if (enable) {
        *permissions |= permission_bit;
    } else {
        *permissions &= ~permission_bit;
    }
}

void building_storage_cycle_partial_resource_state(int storage_id, resource_type resource_id, int reverse_order)
{
    resource_storage_entry *entry = &array_item(storages, storage_id)->storage.resource_state[resource_id];

    if (entry->state == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return; // not accepting is always MAX
    }

    int step = config_get(CONFIG_GP_STORAGE_INCREMENT_4) ? 4 : 8;

    int current = entry->quantity;

    // If current quantity is out of bounds, reset it.
    if (current > BUILDING_STORAGE_QUANTITY_MAX || current < step) {
        entry->quantity = BUILDING_STORAGE_QUANTITY_MAX;
        return;
    }

    if (reverse_order) {
        current -= step;
        if (current < step) {
            current = BUILDING_STORAGE_QUANTITY_MAX;
        }
    } else {
        current += step;
        if (current > BUILDING_STORAGE_QUANTITY_MAX) {
            current = step;
        }
    }

    entry->quantity = (building_storage_quantity) current;
}

int building_storage_accepts_storage(building *b, resource_type resource, int *understaffed)
{
    if (b->type == BUILDING_WAREHOUSE) {
        return building_warehouse_accepts_storage(b, resource, understaffed);
    } else if (b->type == BUILDING_GRANARY) {
        return building_granary_accepts_storage(b, resource, understaffed);
    }
}

void building_storage_accept_none(int storage_id)
{
    data_storage *s = array_item(storages, storage_id);
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        s->storage.resource_state[r].state = BUILDING_STORAGE_STATE_NOT_ACCEPTING;
    }
}

void building_storage_accept_all(int storage_id)
{
    data_storage *s = array_item(storages, storage_id);
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        s->storage.resource_state[r].state = BUILDING_STORAGE_STATE_ACCEPTING;
    }
}

int building_storage_check_if_accepts_nothing(int storage_id)
{
    data_storage *s = array_item(storages, storage_id);
    for (int r = RESOURCE_MIN; r < RESOURCE_MAX; r++) {
        if (s->storage.resource_state[r].state != BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
            return 0;
        }
    }
    return 1;
}

static const uint8_t *storage_state_text(building_storage_state state, building_type bt)
{
    switch (state) {
        case BUILDING_STORAGE_STATE_ACCEPTING:   return lang_get_string(99, 7);
        case BUILDING_STORAGE_STATE_NOT_ACCEPTING: return lang_get_string(99, 8);
        case BUILDING_STORAGE_STATE_GETTING:     return lang_get_string(99, 9 + (bt == BUILDING_GRANARY));
        case BUILDING_STORAGE_STATE_MAINTAINING: return lang_get_string(CUSTOM_TRANSLATION, TR_WINDOW_BUILDING_DISTRIBUTION_MAINTAINING);
        default: return (const uint8_t *) "";
    }
}

int building_storage_summary_tooltip(building *b, char *tooltip_text, int max_length, storage_summary_style style)
{
    if (!b || !tooltip_text || max_length <= 0) return 0;
    if (style == STORAGE_SUMMARY_STYLE_NONE) {
        tooltip_text[0] = '\0';
        return 0;
    } else if (b->state == BUILDING_STATE_MOTHBALLED) {
        const uint8_t *mothballed_text = lang_get_string(CUSTOM_TRANSLATION, TR_TOOLTIP_OVERLAY_PROBLEMS_MOTHBALLED);
        string_copy(mothballed_text, (uint8_t *) tooltip_text, max_length);
        return 1;
    }

    const resource_list *list = (b->type == BUILDING_WAREHOUSE)
        ? city_resource_get_potential() : city_resource_get_potential_foods();

    building_storage_state state;
    int name_w = 0, state_w = 0;

    // Measure max widths
    for (unsigned int i = 0; i < list->size; i++) {
        const resource_data *res = resource_get_data(list->items[i]);
        if (!res) continue;
        state = building_storage_get_state(b, list->items[i], 0);
        if (style == STORAGE_SUMMARY_STYLE_MINIMAL && state == BUILDING_STORAGE_STATE_NOT_ACCEPTING) continue;

        int rn_pixels = text_get_width(res->text, FONT_SMALL_PLAIN);
        int sn_pixels = text_get_width(storage_state_text(state, b->type), FONT_SMALL_PLAIN);
        if (rn_pixels > name_w) name_w = rn_pixels;
        if (sn_pixels > state_w) state_w = sn_pixels;
    }

    uint8_t *out = (uint8_t *) tooltip_text, *start = out;
    out[0] = '\0'; int written = 0;

    int space_width = font_definition_for(FONT_SMALL_PLAIN)->space_width;
    const uint8_t *space_str = (const uint8_t *) " ";
    const uint8_t *newline_str = (const uint8_t *) "\n";
    const uint8_t *dash_str = (const uint8_t *) " - ";
    const uint8_t empty_char[] = { 0x01, 0x00 };  // 1px spacer (NOT allowed before newline)

    for (unsigned int i = 0; i < list->size; i++) {
        resource_type r = list->items[i];
        const resource_data *res = resource_get_data(r);
        if (!res) continue;

        state = building_storage_get_state(b, r, 0);
        if (style == STORAGE_SUMMARY_STYLE_MINIMAL && state == BUILDING_STORAGE_STATE_NOT_ACCEPTING) continue;

        int qty = building_storage_get_storage_state_quantity(b, r);
        const uint8_t *rn = res->text;
        const uint8_t *st = storage_state_text(state, b->type);

        // Resource column
        out = string_copy(rn, out, max_length - (out - start));
        int resource_name_width = text_get_width(rn, FONT_SMALL_PLAIN);

        // Pad to resource name column width (mid-line padding is OK)
        int j = resource_name_width;
        while (j < name_w) {
            if (j + space_width <= name_w) {
                out = string_copy(space_str, out, max_length - (out - start));
                j += space_width;
            } else {
                out = string_copy(empty_char, out, max_length - (out - start));
                j += 1;
            }
        }

        // Separator
        out = string_copy(dash_str, out, max_length - (out - start));

        // State column text
        out = string_copy(st, out, max_length - (out - start));
        int state_name_width = text_get_width(st, FONT_SMALL_PLAIN);

        if (state != BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
            j = state_name_width;
            while (j < state_w) {
                if (j + space_width <= state_w) {
                    out = string_copy(space_str, out, max_length - (out - start));
                    j += space_width;
                } else {
                    out = string_copy(empty_char, out, max_length - (out - start));
                    j += 1;
                }
            }

            // Quantity column
            out = string_copy(dash_str, out, max_length - (out - start));
            uint8_t qty_buf[12];
            string_from_int(qty_buf, qty, 0);
            out = string_copy(qty_buf, out, max_length - (out - start)); // Line must end on the last digit 
            out = string_copy(newline_str, out, max_length - (out - start));// (no padding before \n)
        } else {
            // IMPORTANT: For NOT_ACCEPTING we must NOT pad after 'st' and must
            // NOT let newline follow a space or empty_char. Go straight to \n.
            out = string_copy(newline_str, out, max_length - (out - start));
        }

        written = 1;
        if ((out - start) >= max_length) {
            start[max_length - 1] = '\0';
            break;
        }
    }
    // Remove trailing '\n' on the very last line (if any).
    if (written && out > (uint8_t *) tooltip_text && out[-1] == '\n') {
        out[-1] = '\0';
    }
    return written;
}

int building_storage_resource_max_storable(building *b, resource_type resource_id)
{
    if (b->type == BUILDING_GRANARY && resource_id >= RESOURCE_MIN_NON_FOOD) {
        return 0;
    }

    const building_storage *s = building_storage_get(b->storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource_id];

    if (entry->state != BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return entry->quantity;
    }

    return 0;
}

void decode_legacy_storage_state(uint8_t legacy, resource_storage_entry *entry)
{
    // helper function for the legacy 12 storage states
    switch (legacy) {
        case 0: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_MAX; break;
        case 1: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_MAX; break;
        case 2: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_MAX; break;
        case 3: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_16; break;
        case 4: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_8; break;
        case 5: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_16; break;
        case 6: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_8; break;
        case 7: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_24; break;
        case 8: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_24; break;
        case 9: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_16; break;
        case 10: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_8; break;
        case 11: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_24; break;
        default:
            entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING;
            entry->quantity = BUILDING_STORAGE_QUANTITY_MAX;
            break;
    }
}


void building_storage_save_state(buffer *buf)
{
    int buf_size = 4 + storages.size * STORAGE_CURRENT_BUFFER_SIZE;
    uint8_t *buf_data = malloc(buf_size);
    buffer_init(buf, buf_data, buf_size);
    buffer_write_i32(buf, STORAGE_CURRENT_BUFFER_SIZE);

    data_storage *s;
    array_foreach(storages, s)
    {
        buffer_write_i32(buf, s->storage.permissions); // Originally unused
        buffer_write_i32(buf, s->building_id);
        buffer_write_u8(buf, (uint8_t) s->in_use);
        buffer_write_u8(buf, (uint8_t) s->storage.empty_all);

        for (int r = 0; r < RESOURCE_MAX; r++) {
            buffer_write_u8(buf, (uint8_t) s->storage.resource_state[r].state);
            buffer_write_u8(buf, (uint8_t) s->storage.resource_state[r].quantity);
        }
    }
}

building_storage_permission_states building_storage_get_permission_from_building_type(building_type type)
{
    switch (type) {
        case BUILDING_MARKET:
            return BUILDING_STORAGE_PERMISSION_MARKET;
        case BUILDING_MESS_HALL:
            return BUILDING_STORAGE_PERMISSION_QUARTERMASTER;
        case BUILDING_TAVERN:
            return BUILDING_STORAGE_PERMISSION_BARKEEP;
        case BUILDING_CARAVANSERAI:
            return BUILDING_STORAGE_PERMISSION_CARAVANSERAI;
        case BUILDING_LIGHTHOUSE:
            return BUILDING_STORAGE_PERMISSION_LIGHTHOUSE;
        case BUILDING_ARMOURY:
            return BUILDING_STORAGE_PERMISSION_ARMOURY;
        default:
            return BUILDING_STORAGE_PERMISSION_MARKET; // assume market for other types (usually priests)
    }
}

void building_storage_load_state(buffer *buf, int version)
{
    int storage_buf_size;
    size_t buf_size = buf->size;
    int storages_to_load;
    int highest_id_in_use = 0;

    //  state + quantity stored separately
    if (version > SAVE_GAME_LAST_STORAGE_STATE_AND_QUANTITY_TOGETHER) {
        storage_buf_size = buffer_read_i32(buf);
        buf_size -= 4;

        int num_resources = (storage_buf_size - STORAGE_STATIC_BUFFER_SIZE) / 2;
        storages_to_load = (int) buf_size / storage_buf_size;

        if (!array_init(storages, STORAGE_ARRAY_SIZE_STEP, storage_create, storage_in_use) ||
            !array_expand(storages, storages_to_load)) {
            log_error("Unable to create storages. The game will likely crash.", 0, 0);
        }

        for (int i = 0; i < storages_to_load; i++) {
            data_storage *s = array_next(storages);

            s->storage.permissions = buffer_read_i32(buf);
            s->building_id = buffer_read_i32(buf);
            s->in_use = buffer_read_u8(buf);
            s->storage.empty_all = buffer_read_u8(buf);

            if (config_get(CONFIG_GP_CH_WAREHOUSES_DONT_ACCEPT)) {
                building_storage_accept_none(s->id);
            }

            for (int r = 0; r < num_resources; r++) {
                int remapped = resource_remap(r);
                s->storage.resource_state[remapped].state = buffer_read_u8(buf);
                s->storage.resource_state[remapped].quantity = buffer_read_u8(buf);
            }

            if (storage_buf_size > STORAGE_CURRENT_BUFFER_SIZE) {
                buffer_skip(buf, storage_buf_size - STORAGE_CURRENT_BUFFER_SIZE);
            }

            if (s->in_use) {
                highest_id_in_use = i;
            }
        }

        storages.size = highest_id_in_use + 1;
        return;
    }


    int includes_storage_size = version > SAVE_GAME_LAST_STATIC_VERSION;
    int num_resources = RESOURCE_MAX_LEGACY;

    if (includes_storage_size) { // Augustus 4.0 
        storage_buf_size = buffer_read_i32(buf);
        buf_size -= 4;
        num_resources = storage_buf_size - STORAGE_STATIC_BUFFER_SIZE;
    } else {
        storage_buf_size = STORAGE_ORIGINAL_BUFFER_SIZE; //Julius and vanilla
    }

    storages_to_load = (int) buf_size / storage_buf_size;

    if (!array_init(storages, STORAGE_ARRAY_SIZE_STEP, storage_create, storage_in_use) ||
        !array_expand(storages, storages_to_load)) {
        log_error("Unable to create storages. The game will likely crash.", 0, 0);
    }

    for (int i = 0; i < storages_to_load; i++) {
        data_storage *s = array_next(storages);

        s->storage.permissions = buffer_read_i32(buf);
        s->building_id = buffer_read_i32(buf);
        s->in_use = buffer_read_u8(buf);
        s->storage.empty_all = buffer_read_u8(buf);

        if (config_get(CONFIG_GP_CH_WAREHOUSES_DONT_ACCEPT)) {
            building_storage_accept_none(s->id);
        }

        for (int r = 0; r < num_resources; r++) {
            int remapped = resource_remap(r);
            uint8_t legacy = buffer_read_u8(buf);
            decode_legacy_storage_state(legacy, &s->storage.resource_state[remapped]);
        }

        if (!includes_storage_size) {
            buffer_skip(buf, 6); // hardcoded old unused bytes
        } else if (storage_buf_size > STORAGE_CURRENT_BUFFER_SIZE) {
            buffer_skip(buf, storage_buf_size - STORAGE_CURRENT_BUFFER_SIZE);
        }

        if (s->in_use) {
            highest_id_in_use = i;
        }
    }

    // fix granary storage
    for (building *b = building_first_of_type(BUILDING_GRANARY); b; b = b->next_of_type) {
        int granary_free_space = BUILDING_STORAGE_QUANTITY_MAX;
        for (int r = RESOURCE_MIN_FOOD; r < RESOURCE_MAX_FOOD; r++) {
            b->resources[r] /= 100;
            granary_free_space -= b->resources[r];
        }
        b->resources[RESOURCE_NONE] = granary_free_space;
    }

    storages.size = highest_id_in_use + 1;
}

