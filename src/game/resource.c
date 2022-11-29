#include "resource.h"

#include "building/type.h"
#include "core/image.h"
#include "core/image_group_editor.h"
#include "game/save_version.h"
#include "scenario/building.h"
#include "translation/translation.h"

#define NUM_SPECIAL_RESOURCES 2
#define RESOURCE_MAPPING_MAX RESOURCE_MAX + NUM_SPECIAL_RESOURCES

enum {
    ORIGINAL_WHEAT_ID = 1,
    ORIGINAL_VEGETABLES_ID = 2,
    ORIGINAL_FRUIT_ID = 3,
    ORIGINAL_OLIVES_ID = 4,
    ORIGINAL_VINES_ID = 5,
    ORIGINAL_MEAT_ID = 6,
    ORIGINAL_WINE_ID = 7,
    ORIGINAL_OIL_ID = 8,
    ORIGINAL_IRON_ID = 9,
    ORIGINAL_TIMBER_ID = 10,
    ORIGINAL_CLAY_ID = 11,
    ORIGINAL_MARBLE_ID = 12,
    ORIGINAL_WEAPONS_ID = 13,
    ORIGINAL_FURNITURE_ID = 14,
    ORIGINAL_POTTERY_ID = 15,
    ORIGINAL_DENARII_ID = 16,
    ORIGINAL_TROOPS_ID = 17
};

static const resource_type resource_mappings[][RESOURCE_MAPPING_MAX] = {
    {
        RESOURCE_NONE, RESOURCE_WHEAT, RESOURCE_VEGETABLES, RESOURCE_FRUIT, RESOURCE_OLIVES, RESOURCE_VINES,
        RESOURCE_MEAT, RESOURCE_WINE, RESOURCE_OIL, RESOURCE_IRON, RESOURCE_TIMBER, RESOURCE_CLAY, RESOURCE_MARBLE,
        RESOURCE_WEAPONS, RESOURCE_FURNITURE, RESOURCE_POTTERY, RESOURCE_DENARII, RESOURCE_TROOPS
    }
};

static struct {
    const resource_type *current;
    int total_resources;
    int total_food_resources;
} mapping = { resource_mappings[0], RESOURCE_MAX, RESOURCE_MAX_FOOD_LEGACY };

static resource_data resource_info[RESOURCE_MAX + RESOURCE_SPECIAL] = {
    [RESOURCE_NONE] =       { .type = RESOURCE_NONE,       .inventory_id = INVENTORY_NONE },
    [RESOURCE_WHEAT] =      { .type = RESOURCE_WHEAT,      .inventory_id = INVENTORY_WHEAT,      .flags = RESOURCE_FLAG_FOOD,         .default_trade_price = {  28,  22 } },
    [RESOURCE_VEGETABLES] = { .type = RESOURCE_VEGETABLES, .inventory_id = INVENTORY_VEGETABLES, .flags = RESOURCE_FLAG_FOOD,         .default_trade_price = {  38,  30 } },
    [RESOURCE_FRUIT] =      { .type = RESOURCE_FRUIT,      .inventory_id = INVENTORY_FRUIT,      .flags = RESOURCE_FLAG_FOOD,         .default_trade_price = {  38,  30 } },
    [RESOURCE_OLIVES] =     { .type = RESOURCE_OLIVES,     .inventory_id = INVENTORY_NONE,       .flags = RESOURCE_FLAG_RAW_MATERIAL, .default_trade_price = {  42,  34 } },
    [RESOURCE_VINES] =      { .type = RESOURCE_VINES,      .inventory_id = INVENTORY_NONE,       .flags = RESOURCE_FLAG_RAW_MATERIAL, .default_trade_price = {  44,  36 } },
    [RESOURCE_MEAT] =       { .type = RESOURCE_MEAT,       .inventory_id = INVENTORY_MEAT,       .flags = RESOURCE_FLAG_FOOD,         .default_trade_price = {  44,  36 } },
    [RESOURCE_WINE] =       { .type = RESOURCE_WINE,       .inventory_id = INVENTORY_WINE,       .flags = RESOURCE_FLAG_GOOD,         .default_trade_price = { 215, 160 } },
    [RESOURCE_OIL] =        { .type = RESOURCE_OIL,        .inventory_id = INVENTORY_OIL,        .flags = RESOURCE_FLAG_GOOD,         .default_trade_price = { 180, 140 } },
    [RESOURCE_IRON] =       { .type = RESOURCE_IRON,       .inventory_id = INVENTORY_NONE,       .flags = RESOURCE_FLAG_RAW_MATERIAL, .default_trade_price = {  60,  40 } },
    [RESOURCE_TIMBER] =     { .type = RESOURCE_TIMBER,     .inventory_id = INVENTORY_NONE,       .flags = RESOURCE_FLAG_RAW_MATERIAL, .default_trade_price = {  50,  35 } },
    [RESOURCE_CLAY] =       { .type = RESOURCE_CLAY,       .inventory_id = INVENTORY_NONE,       .flags = RESOURCE_FLAG_RAW_MATERIAL, .default_trade_price = {  40,  30 } },
    [RESOURCE_MARBLE] =     { .type = RESOURCE_MARBLE,     .inventory_id = INVENTORY_NONE,       .flags = RESOURCE_FLAG_RAW_MATERIAL, .default_trade_price = { 200, 140 } },
    [RESOURCE_WEAPONS] =    { .type = RESOURCE_WEAPONS,    .inventory_id = INVENTORY_NONE,       .flags = RESOURCE_FLAG_GOOD,         .default_trade_price = { 250, 180 } },
    [RESOURCE_FURNITURE] =  { .type = RESOURCE_FURNITURE,  .inventory_id = INVENTORY_FURNITURE,  .flags = RESOURCE_FLAG_GOOD,         .default_trade_price = { 200, 150 } },
    [RESOURCE_POTTERY] =    { .type = RESOURCE_POTTERY,    .inventory_id = INVENTORY_POTTERY,    .flags = RESOURCE_FLAG_GOOD,         .default_trade_price = { 180, 140 } },
    [RESOURCE_DENARII] =    { .type = RESOURCE_DENARII,    .inventory_id = INVENTORY_NONE,       .flags = RESOURCE_FLAG_SPECIAL },
    [RESOURCE_TROOPS] =     { .type = RESOURCE_TROOPS,     .inventory_id = INVENTORY_NONE,       .flags = RESOURCE_FLAG_SPECIAL }
};

static int resource_image_offset(resource_type resource, int type)
{
    return 0;
    if (resource == RESOURCE_MEAT && scenario_building_allowed(BUILDING_WHARF)) {
        switch (type) {
       //     case RESOURCE_IMAGE_STORAGE: return 40;
        //    case RESOURCE_IMAGE_CART: return 648;
         //   case RESOURCE_IMAGE_FOOD_CART: return 8;
          //  case RESOURCE_IMAGE_ICON: return 11;
            default: return 0;
        }
    } else {
        return 0;
    }
}

int resource_is_food(resource_type resource)
{
    return (resource_info[resource].flags & RESOURCE_FLAG_FOOD) != 0;
}

workshop_type resource_to_workshop_type(resource_type resource)
{
    switch (resource) {
        case RESOURCE_OLIVES:
            return WORKSHOP_OLIVES_TO_OIL;
        case RESOURCE_VINES:
            return WORKSHOP_VINES_TO_WINE;
        case RESOURCE_IRON:
            return WORKSHOP_IRON_TO_WEAPONS;
        case RESOURCE_TIMBER:
            return WORKSHOP_TIMBER_TO_FURNITURE;
        case RESOURCE_CLAY:
            return WORKSHOP_CLAY_TO_POTTERY;
        default:
            return WORKSHOP_NONE;
    }
}

void resource_init(void)
{
    int food_index = 0;
    int good_index = 0;

    for (int i = RESOURCE_MIN; i < RESOURCE_MAX_LEGACY; i++) {
        resource_data *info = &resource_info[resource_mappings[0][i]];
        info->text = lang_get_string(23, i);
        info->image.cart.single_load = image_group(GROUP_FIGURE_CARTPUSHER_CART) + 8 * i;
        if (resource_is_food(info->type)) {
            info->image.cart.multiple_loads = image_group(GROUP_FIGURE_CARTPUSHER_CART_MULTIPLE_FOOD) + 8 * food_index;
            info->image.cart.eight_loads = image_group(GROUP_FIGURE_CARTPUSHER_CART_MULTIPLE_FOOD) + 32 + 8 * (food_index + 1);
            food_index++;
        } else {
            info->image.cart.multiple_loads = image_group(GROUP_FIGURE_CARTPUSHER_CART_MULTIPLE_RESOURCE) + 8 * good_index;
            info->image.cart.eight_loads = image_group(GROUP_FIGURE_CARTPUSHER_CART_MULTIPLE_RESOURCE) + 8 * good_index;
            good_index++;
        }
        info->image.empire = image_group(GROUP_EMPIRE_RESOURCES) + i;
        info->image.icon = image_group(GROUP_RESOURCE_ICONS) + i;
        info->image.storage = image_group(GROUP_BUILDING_WAREHOUSE_STORAGE_FILLED) + 4 * (i - 1);
        info->image.editor.icon = image_group(GROUP_EDITOR_RESOURCE_ICONS) + i;
        info->image.editor.empire = image_group(GROUP_EDITOR_EMPIRE_RESOURCES) + i;
    }

    if (scenario_building_allowed(BUILDING_WHARF)) {
        resource_data *info = &resource_info[RESOURCE_FISH];
        info->text = lang_get_string(CUSTOM_TRANSLATION, TR_RESOURCE_FISH);
        info->image.cart.single_load += 648;
        info->image.cart.multiple_loads += 8;
        info->image.cart.eight_loads += 8;
        info->image.storage += 40;
        info->image.icon += 11;
    }
}

const resource_data *resource_get_data(resource_type resource)
{
    return &resource_info[resource];
}


int inventory_is_set(int inventory, int flag)
{
    return (inventory >> flag) & 1;
}

void inventory_set(int *inventory, int flag)
{
    *inventory |= 1 << flag; 
}

int resource_from_inventory(int inventory_id)
{
    switch (inventory_id) {
        case INVENTORY_WHEAT: return RESOURCE_WHEAT;
        case INVENTORY_VEGETABLES: return RESOURCE_VEGETABLES;
        case INVENTORY_FRUIT: return RESOURCE_FRUIT;
        case INVENTORY_MEAT: return RESOURCE_MEAT;
        case INVENTORY_POTTERY: return RESOURCE_POTTERY;
        case INVENTORY_FURNITURE: return RESOURCE_FURNITURE;
        case INVENTORY_OIL: return RESOURCE_OIL;
        case INVENTORY_WINE: return RESOURCE_WINE;
        default: return RESOURCE_NONE;
    } 
}

int resource_to_inventory(resource_type resource)
{
    return resource_info[resource].inventory_id;
}

void resource_set_mapping(int version)
{
    if (version < SAVE_GAME_LAST_STATIC_RESOURCES) {
        mapping.current = resource_mappings[0];
        mapping.total_resources = RESOURCE_MAX_LEGACY;
        mapping.total_food_resources = RESOURCE_MAX_FOOD_LEGACY;
    } else {
        mapping.current = resource_mappings[0];
        mapping.total_resources = RESOURCE_MAX;
        mapping.total_food_resources = RESOURCE_MAX_FOOD;
    }
}

resource_type resource_remap(int id)
{
    return mapping.current[id];
}

int resource_total_mapped(void)
{
    return mapping.total_resources;
}

int resource_total_food_mapped(void)
{
    return mapping.total_food_resources;
}
