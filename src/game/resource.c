#include "resource.h"

#include "building/type.h"
#include "core/image.h"
#include "core/image_group_editor.h"
#include "game/save_version.h"
#include "scenario/building.h"

#define NUM_SPECIAL_RESOURCES 2
#define RESOURCE_MAPPING_MAX RESOURCE_MAX + NUM_SPECIAL_RESOURCES

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
    [RESOURCE_NONE] = { type: RESOURCE_NONE, inventory_id: INVENTORY_NONE },
    [RESOURCE_WHEAT] = { type: RESOURCE_WHEAT, inventory_id: INVENTORY_WHEAT, flags: RESOURCE_FLAG_FOOD, default_trade_price: { 28, 22 } },
    [RESOURCE_VEGETABLES] = { type: RESOURCE_VEGETABLES, inventory_id: INVENTORY_VEGETABLES, flags: RESOURCE_FLAG_FOOD, default_trade_price: { 38, 30 } },
    [RESOURCE_FRUIT] = { type: RESOURCE_FRUIT, inventory_id : INVENTORY_VEGETABLES,flags : RESOURCE_FLAG_FOOD, default_trade_price : { 38, 30 } },
    [RESOURCE_OLIVES] = { type: RESOURCE_OLIVES, inventory_id : INVENTORY_NONE, flags : RESOURCE_FLAG_RAW_MATERIAL, default_trade_price : { 42, 34 } },
    [RESOURCE_VINES] = { type: RESOURCE_VINES, inventory_id : INVENTORY_NONE, flags : RESOURCE_FLAG_RAW_MATERIAL, default_trade_price : { 44, 36 } },
    [RESOURCE_MEAT] = { type: RESOURCE_MEAT, inventory_id : INVENTORY_VEGETABLES,flags : RESOURCE_FLAG_FOOD, default_trade_price : { 44, 36 } },
    [RESOURCE_WINE] = { type: RESOURCE_WINE, inventory_id : INVENTORY_VEGETABLES,flags : RESOURCE_FLAG_GOOD, default_trade_price : { 215, 160 } },
    [RESOURCE_OIL] = { type: RESOURCE_OIL, inventory_id : INVENTORY_VEGETABLES,flags : RESOURCE_FLAG_GOOD, default_trade_price : { 180, 140 } },
    [RESOURCE_IRON] = { type: RESOURCE_IRON, inventory_id : INVENTORY_NONE, flags : RESOURCE_FLAG_RAW_MATERIAL, default_trade_price : { 60, 40 } },
    [RESOURCE_TIMBER] = { type: RESOURCE_TIMBER, inventory_id : INVENTORY_NONE, flags : RESOURCE_FLAG_RAW_MATERIAL, default_trade_price : { 50, 35 } },
    [RESOURCE_CLAY] = { type: RESOURCE_CLAY, inventory_id : INVENTORY_NONE, flags : RESOURCE_FLAG_RAW_MATERIAL, default_trade_price : { 40, 30 } },
    [RESOURCE_MARBLE] = { type: RESOURCE_MARBLE, inventory_id : INVENTORY_NONE, flags : RESOURCE_FLAG_RAW_MATERIAL, default_trade_price : { 200, 140 } },
    [RESOURCE_WEAPONS] = { type: RESOURCE_WEAPONS, inventory_id : INVENTORY_NONE, flags : RESOURCE_FLAG_GOOD, default_trade_price : { 250, 180 } },
    [RESOURCE_FURNITURE] = { type: RESOURCE_FURNITURE, inventory_id : INVENTORY_VEGETABLES, flags : RESOURCE_FLAG_GOOD, default_trade_price : { 200, 150 } },
    [RESOURCE_POTTERY] = { type: RESOURCE_POTTERY, inventory_id : INVENTORY_VEGETABLES, flags : RESOURCE_FLAG_GOOD, default_trade_price : { 180, 140 } },
    [RESOURCE_DENARII] = { type: RESOURCE_DENARII, inventory_id : INVENTORY_NONE, flags : RESOURCE_FLAG_SPECIAL },
    [RESOURCE_TROOPS] = { type: RESOURCE_TROOPS, inventory_id : INVENTORY_NONE, flags : RESOURCE_FLAG_SPECIAL }
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
    // storage:
    // image_group(GROUP_BUILDING_WAREHOUSE_STORAGE_FILLED) +
    //4 * (resource - 1) +
        //resource_image_offset(b->subtype.warehouse_resource_id, RESOURCE_IMAGE_STORAGE)

    // cartpusher:
    //image_group(GROUP_FIGURE_CARTPUSHER_CART) +
        //8 * f->resource_id + resource_image_offset(f->resource_id, RESOURCE_IMAGE_CART);
    // single 4650 (starts empty)
    // 8 food 5266
    // multiple goods 8288
//    static const int CART_OFFSET_MULTIPLE_LOADS_FOOD[] = { 0, 0, 8, 16, 0, 0, 24, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
//    static const int CART_OFFSET_MULTIPLE_LOADS_NON_FOOD[] = { 0, 0, 0, 0, 0, 8, 0, 16, 24, 32, 40, 48, 56, 64, 72, 80 };
//    static const int CART_OFFSET_8_LOADS_FOOD[] = { 0, 40, 48, 56, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    resource_data *info = &resource_info[RESOURCE_WHEAT];
    info->text = lang_get_string(23, RESOURCE_WHEAT);
    info->image.cart.single_load = image_group(GROUP_FIGURE_CARTPUSHER_CART) + 8;
    info->image.cart.multiple_loads = image_group(GROUP_FIGURE_CARTPUSHER_CART_MULTIPLE_FOOD);
    info->image.cart.eight_loads = image_group(GROUP_FIGURE_CARTPUSHER_CART_MULTIPLE_FOOD) + 40;
    info->image.empire = image_group(GROUP_EMPIRE_RESOURCES) + 1;
    info->image.icon = image_group(GROUP_RESOURCE_ICONS) + 1;
    info->image.storage = image_group(GROUP_BUILDING_WAREHOUSE_STORAGE_FILLED);
    info->image.editor.icon = image_group(GROUP_EDITOR_RESOURCE_ICONS) + 1;
    info->image.editor.empire = image_group(GROUP_EDITOR_EMPIRE_RESOURCES) + 1;

    for (int i = 0; i < RESOURCE_MAX + RESOURCE_SPECIAL; i++) {
        
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
