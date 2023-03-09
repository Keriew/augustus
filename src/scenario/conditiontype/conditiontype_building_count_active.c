#include "conditiontype_building_count_active.h"

#include "building/count.h"
#include "building/type.h"
#include "scenario/conditiontype/comparison_helper.h"

void scenario_condition_building_count_active_init(scenario_condition_t *condition)
{
    // Nothing to init.
}

int scenario_condition_building_count_active_met(scenario_condition_t *condition)
{
    int type = condition->parameter1;
    int32_t value = condition->parameter2;
    int32_t building_type = condition->parameter3;

    int total_active_count = 0;
    switch(building_type) {
        case BUILDING_MENU_FARMS:
            total_active_count += building_count_active(BUILDING_WHEAT_FARM);
            total_active_count += building_count_active(BUILDING_VEGETABLE_FARM);
            total_active_count += building_count_active(BUILDING_FRUIT_FARM);
            total_active_count += building_count_active(BUILDING_OLIVE_FARM);
            total_active_count += building_count_active(BUILDING_VINES_FARM);
            total_active_count += building_count_active(BUILDING_PIG_FARM);
            break;
        case BUILDING_MENU_RAW_MATERIALS:
            total_active_count += building_count_active(BUILDING_MARBLE_QUARRY);
            total_active_count += building_count_active(BUILDING_IRON_MINE);
            total_active_count += building_count_active(BUILDING_TIMBER_YARD);
            total_active_count += building_count_active(BUILDING_CLAY_PIT);
            total_active_count += building_count_active(BUILDING_GOLD_MINE);
            break;
        case BUILDING_MENU_WORKSHOPS:
            total_active_count += building_count_active(BUILDING_WINE_WORKSHOP);
            total_active_count += building_count_active(BUILDING_OIL_WORKSHOP);
            total_active_count += building_count_active(BUILDING_WEAPONS_WORKSHOP);
            total_active_count += building_count_active(BUILDING_FURNITURE_WORKSHOP);
            total_active_count += building_count_active(BUILDING_POTTERY_WORKSHOP);
            total_active_count += building_count_active(BUILDING_CITY_MINT);
            break;
        case BUILDING_MENU_SMALL_TEMPLES:
            total_active_count += building_count_active(BUILDING_SMALL_TEMPLE_CERES);
            total_active_count += building_count_active(BUILDING_SMALL_TEMPLE_NEPTUNE);
            total_active_count += building_count_active(BUILDING_SMALL_TEMPLE_MERCURY);
            total_active_count += building_count_active(BUILDING_SMALL_TEMPLE_MARS);
            total_active_count += building_count_active(BUILDING_SMALL_TEMPLE_VENUS);
            break;
        case BUILDING_MENU_LARGE_TEMPLES:
            total_active_count += building_count_active(BUILDING_LARGE_TEMPLE_CERES);
            total_active_count += building_count_active(BUILDING_LARGE_TEMPLE_NEPTUNE);
            total_active_count += building_count_active(BUILDING_LARGE_TEMPLE_MERCURY);
            total_active_count += building_count_active(BUILDING_LARGE_TEMPLE_MARS);
            total_active_count += building_count_active(BUILDING_LARGE_TEMPLE_VENUS);
            break;
        case BUILDING_MENU_GRAND_TEMPLES:
            total_active_count += building_count_active(BUILDING_GRAND_TEMPLE_CERES);
            total_active_count += building_count_active(BUILDING_GRAND_TEMPLE_NEPTUNE);
            total_active_count += building_count_active(BUILDING_GRAND_TEMPLE_MERCURY);
            total_active_count += building_count_active(BUILDING_GRAND_TEMPLE_MARS);
            total_active_count += building_count_active(BUILDING_GRAND_TEMPLE_VENUS);
            break;
        case BUILDING_MENU_TREES:
            total_active_count += building_count_active(BUILDING_PINE_TREE);
            total_active_count += building_count_active(BUILDING_FIR_TREE);
            total_active_count += building_count_active(BUILDING_OAK_TREE);
            total_active_count += building_count_active(BUILDING_ELM_TREE);
            total_active_count += building_count_active(BUILDING_FIG_TREE);
            total_active_count += building_count_active(BUILDING_PLUM_TREE);
            total_active_count += building_count_active(BUILDING_PALM_TREE);
            total_active_count += building_count_active(BUILDING_DATE_TREE);
            break;
        case BUILDING_MENU_PATHS:
            total_active_count += building_count_active(BUILDING_PINE_PATH);
            total_active_count += building_count_active(BUILDING_FIR_PATH);
            total_active_count += building_count_active(BUILDING_OAK_PATH);
            total_active_count += building_count_active(BUILDING_ELM_PATH);
            total_active_count += building_count_active(BUILDING_FIG_PATH);
            total_active_count += building_count_active(BUILDING_PLUM_PATH);
            total_active_count += building_count_active(BUILDING_PALM_PATH);
            total_active_count += building_count_active(BUILDING_DATE_PATH);
            total_active_count += building_count_active(BUILDING_GARDEN_PATH);
            break;
        case BUILDING_MENU_PARKS:
            total_active_count += building_count_active(BUILDING_GARDENS);
            total_active_count += building_count_active(BUILDING_GRAND_GARDEN);
            total_active_count += building_count_active(BUILDING_SMALL_STATUE);
            total_active_count += building_count_active(BUILDING_MEDIUM_STATUE);
            total_active_count += building_count_active(BUILDING_LARGE_STATUE);
            total_active_count += building_count_active(BUILDING_SMALL_STATUE_ALT);
            total_active_count += building_count_active(BUILDING_SMALL_STATUE_ALT_B);
            total_active_count += building_count_active(BUILDING_LEGION_STATUE);
            total_active_count += building_count_active(BUILDING_GLADIATOR_STATUE);
            total_active_count += building_count_active(BUILDING_SMALL_POND);
            total_active_count += building_count_active(BUILDING_LARGE_POND);
            total_active_count += building_count_active(BUILDING_DOLPHIN_FOUNTAIN);
            break;
        default:
            total_active_count += building_count_active(building_type);
            break;
    }
    

    return scenario_event_condition_compare_values(type, total_active_count, (int)value);
}
