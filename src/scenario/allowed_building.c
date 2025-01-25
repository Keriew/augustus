#include "allowed_building.h"

#include "building/monument.h"
#include "building/properties.h"
#include "scenario/data.h"

#include <string.h>

#define MAX_BUILDINGS_PER_ORIGINAL_ALLOWED_SLOT (11 + 1) // 11 buildings + BUILDING_NONE

static const building_type CONVERSION_FROM_ORIGINAL[MAX_ORIGINAL_ALLOWED_BUILDINGS][MAX_BUILDINGS_PER_ORIGINAL_ALLOWED_SLOT] = {
    { BUILDING_NONE },
    { BUILDING_WHEAT_FARM, BUILDING_VEGETABLE_FARM, BUILDING_FRUIT_FARM, BUILDING_OLIVE_FARM, BUILDING_VINES_FARM, BUILDING_PIG_FARM },
    { BUILDING_CLAY_PIT, BUILDING_MARBLE_QUARRY, BUILDING_IRON_MINE, BUILDING_TIMBER_YARD, BUILDING_GOLD_MINE, BUILDING_STONE_QUARRY, BUILDING_SAND_PIT },
    { BUILDING_WINE_WORKSHOP, BUILDING_OIL_WORKSHOP, BUILDING_WEAPONS_WORKSHOP, BUILDING_FURNITURE_WORKSHOP, BUILDING_POTTERY_WORKSHOP, BUILDING_BRICKWORKS, BUILDING_CONCRETE_MAKER },
    { BUILDING_ROAD },
    { BUILDING_WALL, BUILDING_PALISADE },
    { BUILDING_DRAGGABLE_RESERVOIR, BUILDING_AQUEDUCT, BUILDING_FOUNTAIN },
    { BUILDING_HOUSE_VACANT_LOT },
    { BUILDING_AMPHITHEATER },
    { BUILDING_THEATER },
    { BUILDING_HIPPODROME },
    { BUILDING_COLOSSEUM, BUILDING_ARENA },
    { BUILDING_GLADIATOR_SCHOOL },
    { BUILDING_LION_HOUSE },
    { BUILDING_ACTOR_COLONY },
    { BUILDING_CHARIOT_MAKER },
    { BUILDING_GARDENS, BUILDING_OVERGROWN_GARDENS, BUILDING_HEDGE_DARK, BUILDING_HEDGE_LIGHT, BUILDING_PAVILION_BLUE, BUILDING_COLONNADE, BUILDING_LOOPED_GARDEN_WALL, BUILDING_ROOFED_GARDEN_WALL, BUILDING_DECORATIVE_COLUMN },
    { BUILDING_PLAZA },
    { BUILDING_SMALL_STATUE, BUILDING_SMALL_STATUE_ALT, BUILDING_SMALL_STATUE_ALT_B, BUILDING_GLADIATOR_STATUE, BUILDING_MEDIUM_STATUE, BUILDING_LEGION_STATUE, BUILDING_LARGE_STATUE, BUILDING_HORSE_STATUE, BUILDING_OBELISK, BUILDING_SMALL_POND, BUILDING_LARGE_POND },
    { BUILDING_DOCTOR },
    { BUILDING_HOSPITAL },
    { BUILDING_BATHHOUSE },
    { BUILDING_BARBER },
    { BUILDING_SCHOOL },
    { BUILDING_ACADEMY },
    { BUILDING_LIBRARY },
    { BUILDING_PREFECTURE },
    { BUILDING_FORT, BUILDING_MESS_HALL, BUILDING_FORT_LEGIONARIES, BUILDING_FORT_JAVELIN, BUILDING_FORT_MOUNTED, BUILDING_FORT_AUXILIA_INFANTRY, BUILDING_FORT_ARCHERS },
    { BUILDING_GATEHOUSE, BUILDING_PALISADE_GATE },
    { BUILDING_TOWER, BUILDING_WATCHTOWER },
    { BUILDING_SMALL_TEMPLE_CERES, BUILDING_SMALL_TEMPLE_NEPTUNE, BUILDING_SMALL_TEMPLE_MERCURY, BUILDING_SMALL_TEMPLE_MARS, BUILDING_SMALL_TEMPLE_VENUS },
    { BUILDING_LARGE_TEMPLE_CERES, BUILDING_LARGE_TEMPLE_NEPTUNE, BUILDING_LARGE_TEMPLE_MERCURY, BUILDING_LARGE_TEMPLE_MARS, BUILDING_LARGE_TEMPLE_VENUS, BUILDING_GRAND_TEMPLE_CERES, BUILDING_GRAND_TEMPLE_NEPTUNE, BUILDING_GRAND_TEMPLE_MERCURY, BUILDING_GRAND_TEMPLE_MARS, BUILDING_GRAND_TEMPLE_VENUS },
    { BUILDING_MARKET },
    { BUILDING_GRANARY },
    { BUILDING_WAREHOUSE },
    { BUILDING_TRIUMPHAL_ARCH },
    { BUILDING_DOCK },
    { BUILDING_WHARF, BUILDING_SHIPYARD },
    { BUILDING_GOVERNORS_HOUSE, BUILDING_GOVERNORS_VILLA, BUILDING_GOVERNORS_PALACE },
    { BUILDING_ENGINEERS_POST },
    { BUILDING_SENATE },
    { BUILDING_FORUM },
    { BUILDING_WELL },
    { BUILDING_ORACLE, BUILDING_SMALL_MAUSOLEUM, BUILDING_LARGE_MAUSOLEUM, BUILDING_NYMPHAEUM },
    { BUILDING_MISSION_POST },
    { BUILDING_LOW_BRIDGE, BUILDING_SHIP_BRIDGE },
    { BUILDING_BARRACKS, BUILDING_ARMOURY },
    { BUILDING_MILITARY_ACADEMY },
    { BUILDING_GRAND_TEMPLE_CERES, BUILDING_GRAND_TEMPLE_NEPTUNE, BUILDING_GRAND_TEMPLE_MERCURY, BUILDING_GRAND_TEMPLE_MARS, BUILDING_GRAND_TEMPLE_VENUS },
};

static uint8_t allowed_buildings[BUILDING_TYPE_MAX];

int scenario_allowed_building(building_type type)
{
    return !building_properties_for_type(type)->disallowable || allowed_buildings[type];
}

static void set_group(unsigned int id, int allowed)
{
    const building_type *group = CONVERSION_FROM_ORIGINAL[id];
    unsigned int index = 0;
    while (group[index]) {
        allowed_buildings[group[index]] = allowed;
        index++;
    }
}

void scenario_allowed_building_set(building_type type, int allowed)
{
    switch (type) {
        case BUILDING_MENU_FARMS:
            set_group(1, allowed);
            return;
        case BUILDING_MENU_RAW_MATERIALS:
            set_group(2, allowed);
            return;
        case BUILDING_MENU_WORKSHOPS:
            set_group(3, allowed);
            return;
        case BUILDING_MENU_SMALL_TEMPLES:
            set_group(30, allowed);
            return;
        case BUILDING_MENU_LARGE_TEMPLES:
            set_group(31, allowed);
            return;
        case BUILDING_MENU_GRAND_TEMPLES:
            set_group(48, allowed);
            return;
        case BUILDING_MENU_TREES:
            set_group(16, allowed);
            return;
        case BUILDING_MENU_PATHS:
            set_group(16, allowed);
            return;
        case BUILDING_MENU_PARKS:
            set_group(16, allowed);
            return;
        case BUILDING_MENU_STATUES:
            set_group(18, allowed);
            return;
        case BUILDING_MENU_GOV_RES:
            set_group(38, allowed);
            return;
        default:
            allowed_buildings[type] = allowed;
            return;
    }
}

void scenario_allowed_building_enable_all(void)
{
    for (unsigned int i = 0; i < BUILDING_TYPE_MAX; i++) {
        allowed_buildings[i] = 1;
    }
}

void scenario_allowed_building_disable_all(void)
{
    for (unsigned int i = 0; i < BUILDING_TYPE_MAX; i++) {
        allowed_buildings[i] = 0;
    }
}

const building_type *scenario_allowed_building_get_buildings_from_original_id(unsigned int original)
{
    return CONVERSION_FROM_ORIGINAL[original];
}

void scenario_allowed_building_load_state(buffer *buf)
{
    scenario_allowed_building_enable_all();

    unsigned int buildings = buffer_load_dynamic_array(buf);

    for (unsigned int i = 0; i < buildings; i++) {
        allowed_buildings[i] = buffer_read_i8(buf);
    }
}

void scenario_allowed_building_load_state_old_version(buffer *buf)
{
    scenario_allowed_building_enable_all();

    for (unsigned int i = 0; i < MAX_ORIGINAL_ALLOWED_BUILDINGS; i++) {
        short allowed = buffer_read_i16(buf);
        if (allowed) {
            continue;
        }
        for (unsigned int j = 0; j < MAX_BUILDINGS_PER_ORIGINAL_ALLOWED_SLOT; j++) {
            if (CONVERSION_FROM_ORIGINAL[i][j] == BUILDING_NONE) {
                break;
            }
            allowed_buildings[CONVERSION_FROM_ORIGINAL[i][j]] = 0;
        }
    }
}

void scenario_allowed_building_save_state(buffer *buf)
{
    buffer_init_dynamic_array(buf, BUILDING_TYPE_MAX, sizeof(int8_t));

    for (unsigned int i = 0; i < BUILDING_TYPE_MAX; i++) {
        buffer_write_i8(buf, allowed_buildings[i]);
    }
}
