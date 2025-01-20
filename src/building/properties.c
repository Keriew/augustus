#include "properties.h"

#include "assets/assets.h"
#include "core/image_group.h" 
#include "type.h"

#include <stddef.h>

static building_properties properties[BUILDING_TYPE_MAX] = {
    [BUILDING_ROAD] = {
        .size = 1,
        .image_group = 112
    },
    [BUILDING_WALL] = {
        .size = 1,
        .disallowable = 1,
        .image_group = 24,
        .image_offset = 26
    },
    [BUILDING_DRAGGABLE_RESERVOIR] = {
        .size = 1
    },
    [BUILDING_AQUEDUCT] = {
        .size = 1,
        .disallowable = 1,
        .image_group = 19,
        .image_offset = 2
    },
    [BUILDING_HOUSE_SMALL_TENT] = {
        .size = 1
    },
    [BUILDING_HOUSE_LARGE_TENT] = {
        .size = 1
    },
    [BUILDING_HOUSE_SMALL_SHACK] = {
        .size = 1
    },
    [BUILDING_HOUSE_LARGE_SHACK] = {
        .size = 1
    },
    [BUILDING_HOUSE_SMALL_HOVEL] = {
        .size = 1
    },
    [BUILDING_HOUSE_LARGE_HOVEL] = {
        .size = 1
    },
    [BUILDING_HOUSE_SMALL_CASA] = {
        .size = 1
    },
    [BUILDING_HOUSE_LARGE_CASA] = {
        .size = 1
    },
    [BUILDING_HOUSE_SMALL_INSULA] = {
        .size = 1
    },
    [BUILDING_HOUSE_MEDIUM_INSULA] = {
        .size = 1
    },
    [BUILDING_HOUSE_LARGE_INSULA] = {
        .size = 2
    },
    [BUILDING_HOUSE_GRAND_INSULA] = {
        .size = 2
    },
    [BUILDING_HOUSE_SMALL_VILLA] = {
        .size = 2
    },
    [BUILDING_HOUSE_MEDIUM_VILLA] = {
        .size = 2
    },
    [BUILDING_HOUSE_LARGE_VILLA] = {
        .size = 3
    },
    [BUILDING_HOUSE_GRAND_VILLA] = {
        .size = 3
    },
    [BUILDING_HOUSE_SMALL_PALACE] = {
        .size = 3
    },
    [BUILDING_HOUSE_MEDIUM_PALACE] = {
        .size = 3
    },
    [BUILDING_HOUSE_LARGE_PALACE] = {
        .size = 4
    },
    [BUILDING_HOUSE_LUXURY_PALACE] = {
        .size = 4
    },
    [BUILDING_AMPHITHEATER] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 45
    },
    [BUILDING_THEATER] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 46,
    },
    [BUILDING_HIPPODROME] = {
        .size = 5,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 213
    },
    [BUILDING_COLOSSEUM] = {
        .size = 5,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 48
    },
    [BUILDING_GLADIATOR_SCHOOL] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 49
    },
    [BUILDING_LION_HOUSE] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 50
    },
    [BUILDING_ACTOR_COLONY] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 51
    },
    [BUILDING_CHARIOT_MAKER] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 52
    },
    [BUILDING_PLAZA] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 58
    },
    [BUILDING_GARDENS] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 59,
        .image_offset = 1
    },
    [BUILDING_FORT_LEGIONARIES] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 66
    },
    [BUILDING_SMALL_STATUE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .rotation_offset = -12,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "V Small Statue"
    },
    [BUILDING_MEDIUM_STATUE] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Med_Statue_R"
    },
    [BUILDING_LARGE_STATUE] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 61,
        .image_offset = 2
    },
    [BUILDING_FORT_JAVELIN] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 66
    },
    [BUILDING_FORT_MOUNTED] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 66
    },
    [BUILDING_DOCTOR] = {
        .size = 1,
        .disallowable = 1,
        .image_group = 68
    },
    [BUILDING_HOSPITAL] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 70
    },
    [BUILDING_BATHHOUSE] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 185
    },
    [BUILDING_BARBER] = {
        .size = 1,
        .disallowable = 1,
        .image_group = 67
    },
    [BUILDING_DISTRIBUTION_CENTER_UNUSED] = {
        .size = 3,
        .image_group = 66
    },
    [BUILDING_SCHOOL] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 41
    },
    [BUILDING_ACADEMY] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 43
    },
    [BUILDING_LIBRARY] = {
        .size = 2,
        .disallowable = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Downgraded_Library"
    },
    [BUILDING_FORT_GROUND] = {
        .size = 4,
        .fire_proof = 1,
        .image_group = 66,
        .image_offset = 1
    },
    [BUILDING_PREFECTURE] = {
        .size = 1,
        .disallowable = 1,
        .image_group = 64
    },
    [BUILDING_TRIUMPHAL_ARCH] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 205
    },
    [BUILDING_FORT] = {
        .size = 3,
        .fire_proof = 1,
        .image_group = 66
    },
    [BUILDING_GATEHOUSE] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 17,
        .image_offset = 1
    },
    [BUILDING_TOWER] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 17
    },
    [BUILDING_SMALL_TEMPLE_CERES] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 71
    },
    [BUILDING_SMALL_TEMPLE_NEPTUNE] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 72
    },
    [BUILDING_SMALL_TEMPLE_MERCURY] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 73
    },
    [BUILDING_SMALL_TEMPLE_MARS] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 74
    },
    [BUILDING_SMALL_TEMPLE_VENUS] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 75
    },
    [BUILDING_LARGE_TEMPLE_CERES] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 71,
        .image_offset = 1
    },
    [BUILDING_LARGE_TEMPLE_NEPTUNE] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 72,
        .image_offset = 1
    },
    [BUILDING_LARGE_TEMPLE_MERCURY] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 73,
        .image_offset = 1
    },
    [BUILDING_LARGE_TEMPLE_MARS] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 74,
        .image_offset = 1
    },
    [BUILDING_LARGE_TEMPLE_VENUS] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 75,
        .image_offset = 1
    },
    [BUILDING_MARKET] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 22
    },
    [BUILDING_GRANARY] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 99
    },
    [BUILDING_WAREHOUSE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 82
    },
    [BUILDING_WAREHOUSE_SPACE] = {
        .size = 1,
        .fire_proof = 1,
        .image_group = 82
    },
    [BUILDING_SHIPYARD] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 77
    },
    [BUILDING_DOCK] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 78
    },
    [BUILDING_WHARF] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 79
    },
    [BUILDING_GOVERNORS_HOUSE] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 85
    },
    [BUILDING_GOVERNORS_VILLA] = {
        .size = 4,
        .disallowable = 1,
        .image_group = 86
    },
    [BUILDING_GOVERNORS_PALACE] = {
        .size = 5,
        .disallowable = 1,
        .image_group = 87
    },
    [BUILDING_MISSION_POST] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 184
    },
    [BUILDING_ENGINEERS_POST] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 81
    },
    [BUILDING_SENATE] = {
        .size = 5,
        .disallowable = 1,
        .image_group = 62
    },
    [BUILDING_FORUM] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 63
    },
    [BUILDING_NATIVE_HUT] = {
        .size = 1,
        .fire_proof = 1,
        .image_group = 183
    },
    [BUILDING_NATIVE_MEETING] = {
        .size = 2,
        .fire_proof = 1,
        .image_group = 183,
        .image_offset = 2
    },
    [BUILDING_RESERVOIR] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 25
    },
    [BUILDING_FOUNTAIN] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 54
    },
    [BUILDING_WELL] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 23
    },
    [BUILDING_NATIVE_CROPS] = {
        .size = 1,
        .fire_proof = 1,
        .image_group = 100
    },
    [BUILDING_MILITARY_ACADEMY] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 201
    },
    [BUILDING_BARRACKS] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 166
    },
    [BUILDING_ORACLE] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .image_group = 76
    },
    [BUILDING_WHEAT_FARM] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 37
    },
    [BUILDING_VEGETABLE_FARM] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 37
    },
    [BUILDING_FRUIT_FARM] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 37
    },
    [BUILDING_OLIVE_FARM] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 37
    },
    [BUILDING_VINES_FARM] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 37
    },
    [BUILDING_PIG_FARM] = {
        .size = 3,
        .disallowable = 1,
        .image_group = 37
    },
    [BUILDING_MARBLE_QUARRY] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 38
    },
    [BUILDING_IRON_MINE] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 39
    },
    [BUILDING_TIMBER_YARD] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 65
    },
    [BUILDING_CLAY_PIT] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 40
    },
    [BUILDING_WINE_WORKSHOP] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 44
    },
    [BUILDING_OIL_WORKSHOP] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 122
    },
    [BUILDING_WEAPONS_WORKSHOP] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 123
    },
    [BUILDING_FURNITURE_WORKSHOP] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 124
    },
    [BUILDING_POTTERY_WORKSHOP] = {
        .size = 2,
        .disallowable = 1,
        .image_group = 125
    },
    [BUILDING_ROADBLOCK] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Admin_Logistics"
    },
    [BUILDING_WORKCAMP] = {
        .size = 3,
        .disallowable = 1,
        .custom_asset.group = "Admin_Logistics",
        .custom_asset.id = "Workcamp Central"
    },
    [BUILDING_GRAND_TEMPLE_CERES] = {
        .size = 7,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Ceres Complex Off"
    },
    [BUILDING_GRAND_TEMPLE_NEPTUNE] = {
        .size = 7,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Neptune Complex Off"
    },
    [BUILDING_GRAND_TEMPLE_MERCURY] = {
        .size = 7,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Mercury Complex Off"
    },
    [BUILDING_GRAND_TEMPLE_MARS] = {
        .size = 7,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Mars Complex Off"
    },
    [BUILDING_GRAND_TEMPLE_VENUS] = {
        .size = 7,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Venus Complex Off"
    },
    [BUILDING_SMALL_POND] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "s pond south off"
    },
    [BUILDING_LARGE_POND] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "l pond south off"
    },
    [BUILDING_PINE_TREE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental pine"
    },
    [BUILDING_FIR_TREE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental fir"
    },
    [BUILDING_OAK_TREE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental oak"
    },
    [BUILDING_ELM_TREE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental elm"
    },
    [BUILDING_FIG_TREE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental fig"
    },
    [BUILDING_PLUM_TREE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental plum"
    },
    [BUILDING_PALM_TREE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental palm"
    },
    [BUILDING_DATE_TREE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "ornamental date"
    },
    [BUILDING_PINE_PATH] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn pine"
    },
    [BUILDING_FIR_PATH] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn fir"
    },
    [BUILDING_OAK_PATH] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn oak"
    },
    [BUILDING_ELM_PATH] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn elm"
    },
    [BUILDING_FIG_PATH] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn fig"
    },
    [BUILDING_PLUM_PATH] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn plum"
    },
    [BUILDING_PALM_PATH] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn palm"
    },
    [BUILDING_DATE_PATH] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "path orn date"
    },
    [BUILDING_PAVILION_BLUE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "pavilion blue"
    },
    [BUILDING_PAVILION_RED] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "pavilion red"
    },
    [BUILDING_PAVILION_ORANGE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "pavilion orange"
    },
    [BUILDING_PAVILION_YELLOW] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "pavilion yellow"
    },
    [BUILDING_PAVILION_GREEN] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "pavilion green"
    },
    [BUILDING_SMALL_STATUE_ALT] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .rotation_offset = 13,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "sml statue 2"
    },
    [BUILDING_SMALL_STATUE_ALT_B] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .rotation_offset = 13,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "sml statue 3"
    },
    [BUILDING_OBELISK] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "obelisk"
    },
    [BUILDING_PANTHEON] = {
        .size = 7,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Pantheon Off"
    },
    [BUILDING_ARCHITECT_GUILD] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Admin_Logistics",
        .custom_asset.id = "Arch Guild OFF"
    },
    [BUILDING_MESS_HALL] = {
        .size = 3,
        .disallowable = 1,
        .custom_asset.group = "Military",
        .custom_asset.id = "Mess OFF Central"
    },
    [BUILDING_LIGHTHOUSE] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Lighthouse OFF"
    },
    [BUILDING_TAVERN] = {
        .size = 2,
        .disallowable = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Tavern OFF"
    },
    [BUILDING_GRAND_GARDEN] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1
    },
    [BUILDING_ARENA] = {
        .size = 3,
        .disallowable = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Arena OFF"
    },
    [BUILDING_HORSE_STATUE] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Eque Statue"
    },
    [BUILDING_DOLPHIN_FOUNTAIN] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1
    },
    [BUILDING_HEDGE_DARK] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "D Hedge 01"
    },
    [BUILDING_HEDGE_LIGHT] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "L Hedge 01"
    },
    [BUILDING_LOOPED_GARDEN_WALL] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "C Garden Wall 01"
    },
    [BUILDING_LEGION_STATUE] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "legio statue"
    },
    [BUILDING_DECORATIVE_COLUMN] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "sml col B"
    },
    [BUILDING_COLONNADE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "G Colonnade 01"
    },
    [BUILDING_GARDEN_PATH] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Garden Path 01"
    },
    [BUILDING_LARARIUM] = {
        .size = 1,
        .disallowable = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Lararium 01"
    },
    [BUILDING_NYMPHAEUM] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Nymphaeum OFF"
    },
    [BUILDING_SMALL_MAUSOLEUM] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Mausoleum S"
    },
    [BUILDING_LARGE_MAUSOLEUM] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Mausoleum L"
    },
    [BUILDING_WATCHTOWER] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Military",
        .custom_asset.id = "Watchtower C OFF"
    },
    [BUILDING_CARAVANSERAI] = {
        .size = 4,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "Caravanserai_C_OFF"
    },
    [BUILDING_ROOFED_GARDEN_WALL] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "R Garden Wall 01"
    },
    [BUILDING_ROOFED_GARDEN_WALL_GATE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Garden_Gate_B"
    },
    [BUILDING_PALISADE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Military",
        .custom_asset.id = "Pal Wall C 01"
    },
    [BUILDING_HEDGE_GATE_DARK] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "D Hedge Gate"
    },
    [BUILDING_HEDGE_GATE_LIGHT] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "L Hedge Gate"
    },
    [BUILDING_PALISADE_GATE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Military",
        .custom_asset.id = "Palisade_Gate"
    },
    [BUILDING_GLADIATOR_STATUE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Aesthetics"
    },
    [BUILDING_HIGHWAY] = {
        .size = 2,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Admin_Logistics",
        .custom_asset.id = "Highway_Placement"
    },
    [BUILDING_GOLD_MINE] = {
        .size = 2,
        .disallowable = 1,
        .custom_asset.group = "Industry",
        .custom_asset.id = "Gold_Mine_C_OFF"
    },
    [BUILDING_STONE_QUARRY] = {
        .size = 2,
        .disallowable = 1,
        .custom_asset.group = "Industry",
        .custom_asset.id = "Stone_Quarry_C_OFF"
    },
    [BUILDING_SAND_PIT] = {
        .size = 2,
        .disallowable = 1,
        .custom_asset.group = "Industry",
        .custom_asset.id = "Sand_Pit_C_OFF"
    },
    [BUILDING_BRICKWORKS] = {
        .size = 2,
        .disallowable = 1,
        .custom_asset.group = "Industry",
        .custom_asset.id = "Brickworks_C_OFF"
    },
    [BUILDING_CONCRETE_MAKER] = {
        .size = 2,
        .disallowable = 1,
        .custom_asset.group = "Industry",
        .custom_asset.id = "Concrete_Maker_C_OFF"
    },
    [BUILDING_CITY_MINT] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Monuments",
        .custom_asset.id = "City_Mint_ON"
    },
    [BUILDING_DEPOT] = {
        .size = 2,
        .disallowable = 1,
        .custom_asset.group = "Admin_Logistics",
        .custom_asset.id = "Cart Depot N OFF"
    },
    [BUILDING_LOOPED_GARDEN_GATE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Garden_Gate"
    },
    [BUILDING_PANELLED_GARDEN_GATE] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Garden_Gate_C"
    },
    [BUILDING_PANELLED_GARDEN_WALL] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Garden_Wall_C"
    },
    [BUILDING_SHRINE_CERES] = {
        .size = 1,
        .disallowable = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Altar_Ceres"
    },
    [BUILDING_SHRINE_MARS] = {
        .size = 1,
        .disallowable = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Altar_Mars"
    },
    [BUILDING_SHRINE_MERCURY] = {
        .size = 1,
        .disallowable = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Altar_Mercury"
    },
    [BUILDING_SHRINE_NEPTUNE] = {
        .size = 1,
        .disallowable = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Altar_Neptune"
    },
    [BUILDING_SHRINE_VENUS] = {
        .size = 1,
        .disallowable = 1,
        .rotation_offset = 1,
        .custom_asset.group = "Health_Culture",
        .custom_asset.id = "Altar_Venus"
    },
    [BUILDING_OVERGROWN_GARDENS] = {
        .size = 1,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Aesthetics",
        .custom_asset.id = "Overgrown_Garden_01"
    },
    [BUILDING_FORT_AUXILIA_INFANTRY] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Military"
    },
    [BUILDING_FORT_ARCHERS] = {
        .size = 3,
        .disallowable = 1,
        .fire_proof = 1,
        .custom_asset.group = "Military"
    },
    [BUILDING_ARMOURY] = {
        .size = 2,
        .disallowable = 1,
        .custom_asset.group = "Military",
        .custom_asset.id = "Armoury_OFF_C"
    }
};

void building_properties_init(void)
{
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type++) {
        if (!properties[type].custom_asset.group) {
            continue;
        }
        if (properties[type].custom_asset.id) {
            properties[type].image_group = assets_get_image_id(properties[type].custom_asset.group,
                properties[type].custom_asset.id);
        } else {
            properties[type].image_group = assets_get_group_id(properties[type].custom_asset.group);
        }
    }
}

const building_properties *building_properties_for_type(building_type type)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return &properties[0];
    }
    return &properties[type];
}
