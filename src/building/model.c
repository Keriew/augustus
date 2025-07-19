#include "building/model.h"

#include "city/resource.h"
#include "core/io.h"
#include "core/log.h"
#include "core/string.h"

#include <stdlib.h>
#include <string.h>

#define TMP_BUFFER_SIZE 100000

#define NUM_BUILDINGS 130
#define NUM_HOUSES 20

static const uint8_t ALL_BUILDINGS[] = { 'A', 'L', 'L', ' ', 'B', 'U', 'I', 'L', 'D', 'I', 'N', 'G', 'S', 0 };
static const uint8_t ALL_HOUSES[] = { 'A', 'L', 'L', ' ', 'H', 'O', 'U', 'S', 'E', 'S', 0 };

static model_building buildings[NUM_BUILDINGS];
static model_house houses[NUM_HOUSES];

static int strings_equal(const uint8_t *a, const uint8_t *b, int len)
{
    for (int i = 0; i < len; i++, a++, b++) {
        if (*a != *b) {
            return 0;
        }
    }
    return 1;
}

static int index_of_string(const uint8_t *haystack, const uint8_t *needle, int haystack_length)
{
    int needle_length = string_length(needle);
    for (int i = 0; i < haystack_length; i++) {
        if (haystack[i] == needle[0] && strings_equal(&haystack[i], needle, needle_length)) {
            return i + 1;
        }
    }
    return 0;
}

static int index_of(const uint8_t *haystack, uint8_t needle, int haystack_length)
{
    for (int i = 0; i < haystack_length; i++) {
        if (haystack[i] == needle) {
            return i + 1;
        }
    }
    return 0;
}

static const uint8_t *skip_non_digits(const uint8_t *str)
{
    int safeguard = 0;
    while (1) {
        if (++safeguard >= 1000) {
            break;
        }
        if ((*str >= '0' && *str <= '9') || *str == '-') {
            break;
        }
        str++;
    }
    return str;
}

static const uint8_t *get_value(const uint8_t *ptr, const uint8_t *end_ptr, int *value)
{
    ptr = skip_non_digits(ptr);
    *value = string_to_int(ptr);
    ptr += index_of(ptr, ',', (int) (end_ptr - ptr));
    return ptr;
}

static void override_model_data(void)
{
    buildings[BUILDING_LARGE_TEMPLE_CERES].desirability_value = 14;
    buildings[BUILDING_LARGE_TEMPLE_CERES].desirability_step = 2;
    buildings[BUILDING_LARGE_TEMPLE_CERES].desirability_step_size = -2;
    buildings[BUILDING_LARGE_TEMPLE_CERES].desirability_range = 5;

    buildings[BUILDING_LARGE_TEMPLE_NEPTUNE].desirability_value = 14;
    buildings[BUILDING_LARGE_TEMPLE_NEPTUNE].desirability_step = 2;
    buildings[BUILDING_LARGE_TEMPLE_NEPTUNE].desirability_step_size = -2;
    buildings[BUILDING_LARGE_TEMPLE_NEPTUNE].desirability_range = 5;

    buildings[BUILDING_LARGE_TEMPLE_MERCURY].desirability_value = 14;
    buildings[BUILDING_LARGE_TEMPLE_MERCURY].desirability_step = 2;
    buildings[BUILDING_LARGE_TEMPLE_MERCURY].desirability_step_size = -2;
    buildings[BUILDING_LARGE_TEMPLE_MERCURY].desirability_range = 5;

    buildings[BUILDING_LARGE_TEMPLE_MARS].desirability_value = 14;
    buildings[BUILDING_LARGE_TEMPLE_MARS].desirability_step = 2;
    buildings[BUILDING_LARGE_TEMPLE_MARS].desirability_step_size = -2;
    buildings[BUILDING_LARGE_TEMPLE_MARS].desirability_range = 5;

    buildings[BUILDING_LARGE_TEMPLE_VENUS].desirability_value = 14;
    buildings[BUILDING_LARGE_TEMPLE_VENUS].desirability_step = 2;
    buildings[BUILDING_LARGE_TEMPLE_VENUS].desirability_step_size = -2;
    buildings[BUILDING_LARGE_TEMPLE_VENUS].desirability_range = 5;

    buildings[BUILDING_WELL].laborers = 0;
    buildings[BUILDING_GATEHOUSE].laborers = 0;
    buildings[BUILDING_FORT_JAVELIN].laborers = 0;
    buildings[BUILDING_FORT_LEGIONARIES].laborers = 0;
    buildings[BUILDING_FORT_MOUNTED].laborers = 0;
    buildings[BUILDING_FORT].laborers = 0;
}

int model_load(void)
{
    uint8_t *buffer = (uint8_t *) malloc(TMP_BUFFER_SIZE);
    if (!buffer) {
        log_error("No memory for model", 0, 0);
        return 0;
    }
    memset(buffer, 0, TMP_BUFFER_SIZE);
    int filesize = io_read_file_into_buffer("c3_model.txt", NOT_LOCALIZED, buffer, TMP_BUFFER_SIZE);
    if (filesize == 0) {
        log_error("No c3_model.txt file", 0, 0);
        free(buffer);
        return 0;
    }

    int num_lines = 0;
    int guard = 200;
    int brace_index;
    const uint8_t *ptr = &buffer[index_of_string(buffer, ALL_BUILDINGS, filesize)];
    do {
        guard--;
        brace_index = index_of(ptr, '{', filesize);
        if (brace_index) {
            ptr += brace_index;
            num_lines++;
        }
    } while (brace_index && guard > 0);

    if (num_lines != NUM_BUILDINGS + NUM_HOUSES) {
        log_error("Model has incorrect no of lines ", 0, num_lines + 1);
        free(buffer);
        return 0;
    }

    int dummy;
    ptr = &buffer[index_of_string(buffer, ALL_BUILDINGS, filesize)];
    const uint8_t *end_ptr = &buffer[filesize];
    for (int i = 0; i < NUM_BUILDINGS; i++) {
        ptr += index_of(ptr, '{', filesize);

        ptr = get_value(ptr, end_ptr, &buildings[i].cost);
        ptr = get_value(ptr, end_ptr, &buildings[i].desirability_value);
        ptr = get_value(ptr, end_ptr, &buildings[i].desirability_step);
        ptr = get_value(ptr, end_ptr, &buildings[i].desirability_step_size);
        ptr = get_value(ptr, end_ptr, &buildings[i].desirability_range);
        ptr = get_value(ptr, end_ptr, &buildings[i].laborers);
        ptr = get_value(ptr, end_ptr, &dummy);
        ptr = get_value(ptr, end_ptr, &dummy);
    }

    ptr = &buffer[index_of_string(buffer, ALL_HOUSES, filesize)];

    for (int i = 0; i < NUM_HOUSES; i++) {
        ptr += index_of(ptr, '{', filesize);

        ptr = get_value(ptr, end_ptr, &houses[i].devolve_desirability);
        ptr = get_value(ptr, end_ptr, &houses[i].evolve_desirability);
        ptr = get_value(ptr, end_ptr, &houses[i].entertainment);
        ptr = get_value(ptr, end_ptr, &houses[i].water);
        ptr = get_value(ptr, end_ptr, &houses[i].religion);
        ptr = get_value(ptr, end_ptr, &houses[i].education);
        ptr = get_value(ptr, end_ptr, &dummy);
        ptr = get_value(ptr, end_ptr, &houses[i].barber);
        ptr = get_value(ptr, end_ptr, &houses[i].bathhouse);
        ptr = get_value(ptr, end_ptr, &houses[i].health);
        ptr = get_value(ptr, end_ptr, &houses[i].food_types);
        ptr = get_value(ptr, end_ptr, &houses[i].pottery);
        ptr = get_value(ptr, end_ptr, &houses[i].oil);
        ptr = get_value(ptr, end_ptr, &houses[i].furniture);
        ptr = get_value(ptr, end_ptr, &houses[i].wine);
        ptr = get_value(ptr, end_ptr, &dummy);
        ptr = get_value(ptr, end_ptr, &dummy);
        ptr = get_value(ptr, end_ptr, &houses[i].prosperity);
        ptr = get_value(ptr, end_ptr, &houses[i].max_people);
        ptr = get_value(ptr, end_ptr, &houses[i].tax_multiplier);
    }

    override_model_data();

    log_info("Model loaded", 0, 0);
    free(buffer);
    return 1;
}

const model_building MODEL_ROADBLOCK = { .cost = 12, .desirability_value = 0, .desirability_step = 0,
 .desirability_step_size = 0, .desirability_range = 0, .laborers = 0 };
const model_building MODEL_WORK_CAMP = { .cost = 150, .desirability_value = -10, .desirability_step = 2,
 .desirability_step_size = 3, .desirability_range = 4, .laborers = 20 };
const model_building MODEL_ARCHITECT_GUILD = { .cost = 200, .desirability_value = -8, .desirability_step = 1,
 .desirability_step_size = 2, .desirability_range = 4, .laborers = 12 };
const model_building MODEL_GRAND_TEMPLE_CERES = { .cost = 2500, .desirability_value = 20, .desirability_step = 2,
 .desirability_step_size = -4, .desirability_range = 8, .laborers = 50 };
const model_building MODEL_GRAND_TEMPLE_NEPTUNE = { .cost = 2500, .desirability_value = 20, .desirability_step = 2,
 .desirability_step_size = -4, .desirability_range = 8, .laborers = 50 };
const model_building MODEL_GRAND_TEMPLE_MERCURY = { .cost = 2500, .desirability_value = 20, .desirability_step = 2,
 .desirability_step_size = -4, .desirability_range = 8, .laborers = 50 };
const model_building MODEL_GRAND_TEMPLE_MARS = { .cost = 2500, .desirability_value = 20, .desirability_step = 2,
 .desirability_step_size = -4, .desirability_range = 8, .laborers = 50 };
const model_building MODEL_GRAND_TEMPLE_VENUS = { .cost = 2500, .desirability_value = 20, .desirability_step = 2,
 .desirability_step_size = -4, .desirability_range = 8, .laborers = 50 };
const model_building MODEL_PANTHEON = { .cost = 3500, .desirability_value = 20, .desirability_step = 2,
 .desirability_step_size = -4, .desirability_range = 8, .laborers = 50 };
const model_building MODEL_LIGHTHOUSE = { .cost = 1000, .desirability_value = 6, .desirability_step = 1,
 .desirability_step_size = -1, .desirability_range = 4, .laborers = 20 };
const model_building MODEL_MESS_HALL = { .cost = 100, .desirability_value = -8, .desirability_step = 1,
 .desirability_step_size = 2, .desirability_range = 4, .laborers = 10 };
const model_building MODEL_TAVERN = { .cost = 40, .desirability_value = -2, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 6, .laborers = 8 };
const model_building MODEL_GRAND_GARDEN = { .cost = 400, .desirability_value = 0, .desirability_step = 0,
 .desirability_step_size = 0, .desirability_range = 0, .laborers = 0 };
const model_building MODEL_ARENA = { .cost = 500, .desirability_value = -3, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 3, .laborers = 25 };
const model_building MODEL_COLOSSEUM = { .cost = 1500, .desirability_value = -3, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 3, .laborers = 100 };
const model_building MODEL_HIPPODROME = { .cost = 3500, .desirability_value = -3, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 3, .laborers = 150 };
const model_building MODEL_NULL = { .cost = 0, .desirability_value = 0, .desirability_step = 0,
 .desirability_step_size = 0, .desirability_range = 0, .laborers = 0 };
const model_building MODEL_LARARIUM = { .cost = 45, .desirability_value = 4, .desirability_step = 1,
 .desirability_step_size = -1, .desirability_range = 3, .laborers = 0 };
const model_building MODEL_NYMPHAEUM = { .cost = 400, .desirability_value = 12, .desirability_step = 2,
 .desirability_step_size = -1, .desirability_range = 6, .laborers = 0 };
const model_building MODEL_SMALL_MAUSOLEUM = { .cost = 250, .desirability_value = -8, .desirability_step = 1,
 .desirability_step_size = 3, .desirability_range = 5, .laborers = 0 };
const model_building MODEL_LARGE_MAUSOLEUM = { .cost = 500, .desirability_value = -10, .desirability_step = 1,
 .desirability_step_size = 3, .desirability_range = 6, .laborers = 0 };
const model_building MODEL_WATCHTOWER = { .cost = 100, .desirability_value = -6, .desirability_step = 1,
 .desirability_step_size = 2, .desirability_range = 3, .laborers = 8 };
const model_building MODEL_CARAVANSERAI = { .cost = 500, .desirability_value = -10, .desirability_step = 2,
 .desirability_step_size = 3, .desirability_range = 4, .laborers = 20 };
const model_building MODEL_PALISADE = { .cost = 6, .desirability_value = 0, .desirability_step = 0,
 .desirability_step_size = 0, .desirability_range = 0, .laborers = 0 };
const model_building MODEL_HIGHWAY = { .cost = 100, .desirability_value = -4, .desirability_step = 1,
 .desirability_step_size = 2, .desirability_range = 2, .laborers = 0 };
const model_building MODEL_GOLD_MINE = { .cost = 100, .desirability_value = -6, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 4, .laborers = 30 };
const model_building MODEL_STONE_QUARRY = { .cost = 60, .desirability_value = -6, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 4, .laborers = 10 };
const model_building MODEL_SAND_PIT = { .cost = 40, .desirability_value = -6, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 4, .laborers = 10 };
const model_building MODEL_BRICKWORKS = { .cost = 80, .desirability_value = -3, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 3, .laborers = 10 };
const model_building MODEL_CONCRETE_MAKER = { .cost = 60, .desirability_value = -3, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 3, .laborers = 10 };
const model_building MODEL_CITY_MINT = { .cost = 250, .desirability_value = -3, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 3, .laborers = 40 };
const model_building MODEL_DEPOT = { .cost = 100, .desirability_value = -3, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 2, .laborers = 15 };
const model_building MODEL_ARMOURY = { .cost = 50, .desirability_value = -5, .desirability_step = 1,
 .desirability_step_size = 1, .desirability_range = 4, .laborers = 6 };
const model_building MODEL_LATRINE = { .cost = 15, .desirability_value = 0, .desirability_step = 0,
 .desirability_step_size = 0, .desirability_range = 0, .laborers = 2 };
const model_building MODEL_NATIVE_DECORATION = { .cost = 0, .desirability_value = 6, .desirability_step = 1,
 .desirability_step_size = -1, .desirability_range = 4, .laborers = 0 };
const model_building MODEL_NATIVE_MONUMENT = { .cost = 0, .desirability_value = 15, .desirability_step = 2,
 .desirability_step_size = -2, .desirability_range = 8, .laborers = 0 };
const model_building MODEL_NATIVE_WATCHTOWER = { .cost = 0, .desirability_value = -10, .desirability_step = 1,
 .desirability_step_size = 2, .desirability_range = 4, .laborers = 0 };



const model_building *model_get_building(building_type type)
{
    switch (type) {
        case BUILDING_ROADBLOCK:
        case BUILDING_ROOFED_GARDEN_WALL_GATE:
        case BUILDING_LOOPED_GARDEN_GATE:
        case BUILDING_PANELLED_GARDEN_GATE:
        case BUILDING_HEDGE_GATE_DARK:
        case BUILDING_HEDGE_GATE_LIGHT:
            return &MODEL_ROADBLOCK;
        case BUILDING_WORKCAMP:
            return &MODEL_WORK_CAMP;
        case BUILDING_ARCHITECT_GUILD:
            return &MODEL_ARCHITECT_GUILD;
        case BUILDING_GRAND_TEMPLE_CERES:
            return &MODEL_GRAND_TEMPLE_CERES;
        case BUILDING_GRAND_TEMPLE_NEPTUNE:
            return &MODEL_GRAND_TEMPLE_NEPTUNE;
        case BUILDING_GRAND_TEMPLE_MERCURY:
            return &MODEL_GRAND_TEMPLE_MERCURY;
        case BUILDING_GRAND_TEMPLE_MARS:
            return &MODEL_GRAND_TEMPLE_MARS;
        case BUILDING_GRAND_TEMPLE_VENUS:
            return &MODEL_GRAND_TEMPLE_VENUS;
        case BUILDING_PANTHEON:
            return &MODEL_PANTHEON;
        case BUILDING_MESS_HALL:
            return &MODEL_MESS_HALL;
        case BUILDING_LIGHTHOUSE:
            return &MODEL_LIGHTHOUSE;
        case BUILDING_TAVERN:
            return &MODEL_TAVERN;
        case BUILDING_GRAND_GARDEN:
            return &MODEL_GRAND_GARDEN;
        case BUILDING_ARENA:
            return &MODEL_ARENA;
        case BUILDING_COLOSSEUM:
            return &MODEL_COLOSSEUM;
        case BUILDING_HIPPODROME:
            return &MODEL_HIPPODROME;
        case BUILDING_LARARIUM:
        case BUILDING_SHRINE_CERES:
        case BUILDING_SHRINE_MARS:
        case BUILDING_SHRINE_MERCURY:
        case BUILDING_SHRINE_NEPTUNE:
        case BUILDING_SHRINE_VENUS:
            return &MODEL_LARARIUM;
        case BUILDING_NYMPHAEUM:
            return &MODEL_NYMPHAEUM;
        case BUILDING_WATCHTOWER:
            return &MODEL_WATCHTOWER;
        case BUILDING_SMALL_MAUSOLEUM:
            return &MODEL_SMALL_MAUSOLEUM;
        case BUILDING_LARGE_MAUSOLEUM:
            return &MODEL_LARGE_MAUSOLEUM;
        case BUILDING_CARAVANSERAI:
            return &MODEL_CARAVANSERAI;
        case BUILDING_PALISADE:
        case BUILDING_PALISADE_GATE:
            return &MODEL_PALISADE;
        case BUILDING_HIGHWAY:
            return &MODEL_HIGHWAY;
        case BUILDING_GOLD_MINE:
            return &MODEL_GOLD_MINE;
        case BUILDING_STONE_QUARRY:
            return &MODEL_STONE_QUARRY;
        case BUILDING_SAND_PIT:
            return &MODEL_SAND_PIT;
        case BUILDING_BRICKWORKS:
            return &MODEL_BRICKWORKS;
        case BUILDING_CONCRETE_MAKER:
            return &MODEL_CONCRETE_MAKER;
        case BUILDING_CITY_MINT:
            return &MODEL_CITY_MINT;
        case BUILDING_DEPOT:
            return &MODEL_DEPOT;
        case BUILDING_OVERGROWN_GARDENS:
            return &buildings[BUILDING_GARDENS];
        case BUILDING_ARMOURY:
            return &MODEL_ARMOURY;
        case BUILDING_LATRINES:
            return &MODEL_LATRINE;
        case BUILDING_NATIVE_DECORATION:
            return &MODEL_NATIVE_DECORATION;
        case BUILDING_NATIVE_MONUMENT:
            return &MODEL_NATIVE_MONUMENT;
        case BUILDING_NATIVE_WATCHTOWER:
            return &MODEL_NATIVE_WATCHTOWER;
        default:
            break;
    }

    if ((type >= BUILDING_PINE_TREE && type <= BUILDING_SENATOR_STATUE) ||
        type == BUILDING_HEDGE_DARK || type == BUILDING_HEDGE_LIGHT ||
        type == BUILDING_DECORATIVE_COLUMN || type == BUILDING_LOOPED_GARDEN_WALL ||
        type == BUILDING_COLONNADE || type == BUILDING_LOOPED_GARDEN_WALL ||
        type == BUILDING_ROOFED_GARDEN_WALL || type == BUILDING_GARDEN_PATH ||
        type == BUILDING_PANELLED_GARDEN_WALL || type == BUILDING_GLADIATOR_STATUE) {
        return &buildings[BUILDING_SMALL_STATUE];
    }

    if (type == BUILDING_SMALL_POND || type == BUILDING_OBELISK ||
        type == BUILDING_LEGION_STATUE || type == BUILDING_DOLPHIN_FOUNTAIN) {
        return &buildings[BUILDING_MEDIUM_STATUE];
    }

    if (type == BUILDING_LARGE_POND || type == BUILDING_HORSE_STATUE) {
        return &buildings[BUILDING_LARGE_STATUE];
    }

    if (type == BUILDING_FORT_AUXILIA_INFANTRY || type == BUILDING_FORT_ARCHERS) {
        return &buildings[BUILDING_FORT_LEGIONARIES];
    }

    if (type > 129) {
        return &MODEL_NULL;
    } else {
        return &buildings[type];
    }
}

const model_house *model_get_house(house_level level)
{
    return &houses[level];
}

int model_house_uses_inventory(house_level level, resource_type inventory)
{
    const model_house *house = model_get_house(level);
    switch (inventory) {
        case RESOURCE_WINE:
            return house->wine;
        case RESOURCE_OIL:
            return house->oil;
        case RESOURCE_FURNITURE:
            return house->furniture;
        case RESOURCE_POTTERY:
            return house->pottery;
        default:
            return 0;
    }
}
