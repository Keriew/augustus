#include "building/model.h"

#include "city/resource.h"
#include "core/buffer.h"
#include "core/io.h"
#include "core/log.h"
#include "core/string.h"

#include <stdlib.h>
#include <string.h>

#define TMP_BUFFER_SIZE 100000

#define NUM_BUILDINGS 130
#define NUM_NEW_BUILDINGS 95
#define NUM_HOUSES 20

static const uint8_t ALL_BUILDINGS[] = { 'A', 'L', 'L', ' ', 'B', 'U', 'I', 'L', 'D', 'I', 'N', 'G', 'S', 0 };
static const uint8_t ALL_HOUSES[] = { 'A', 'L', 'L', ' ', 'H', 'O', 'U', 'S', 'E', 'S', 0 };

static model_building buildings[NUM_BUILDINGS + NUM_NEW_BUILDINGS];
static model_house houses[NUM_HOUSES];
static const model_building new_buildings[NUM_NEW_BUILDINGS];

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
    
    buildings[BUILDING_COLOSSEUM].cost = 1500;
    buildings[BUILDING_COLOSSEUM].laborers = 100;
    buildings[BUILDING_HIPPODROME].cost = 3500;

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
    
    for (int i = 0; i < NUM_NEW_BUILDINGS; i++) {
        buildings[i + 130] = new_buildings[i];
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

void model_save_model_data(buffer *buf) {
    int buf_size = sizeof(model_building) * (NUM_BUILDINGS + NUM_NEW_BUILDINGS);
    uint8_t *buf_data = malloc(buf_size);
    
    buffer_init(buf, buf_data, buf_size);
    
    buffer_write_raw(buf, buildings, buf_size);
}
void model_load_model_data(buffer *buf) {
    int buf_size = sizeof(model_building) * (NUM_BUILDINGS + NUM_NEW_BUILDINGS);
    
    buffer_read_raw(buf, buildings, buf_size);
}

static const model_building NOTHING = { .cost = 0, .desirability_value = 0, .desirability_step = 0,
 .desirability_step_size = 0, .desirability_range = 0, .laborers = 0 };

static const model_building GRAND_TEMPLE = { .cost = 2500, .desirability_value = 20, .desirability_step = 2,                .desirability_step_size = -4, .desirability_range = 8, .laborers = 50 };

static const model_building SMALL_STATUE = { .cost = 12, .desirability_value = 3, .desirability_step = 1,      .desirability_step_size = -1, .desirability_range = 3, .laborers = 0 };


static const model_building MEDIUM_STATUE = { .cost = 60, .desirability_value = 10, .desirability_step = 1,      .desirability_step_size = -2, .desirability_range = 4, .laborers = 0 };


static const model_building LARGE_STATUE = { .cost = 150, .desirability_value = 14, .desirability_step = 2,      .desirability_step_size = -2, .desirability_range = 5, .laborers = 0 };

static const model_building LARARIUM = { .cost = 45, .desirability_value = 4, .desirability_step = 1,
 .desirability_step_size = -1, .desirability_range = 3, .laborers = 0 };

static const model_building ROADBLOCK = { .cost = 12, .desirability_value = 0, .desirability_step = 0, .desirability_step_size = 0, .desirability_range = 0, .laborers = 0 };

static const model_building FORT = { .cost = 1000, .desirability_value = -20, .desirability_step = 2, .desirability_step_size = 2, .desirability_range = 8, .laborers = 0 };

static const model_building new_buildings[NUM_NEW_BUILDINGS] = {
    ROADBLOCK, // Roadblock
    { .cost = 150, .desirability_value = -10, .desirability_step = 2, .desirability_step_size = 3, .desirability_range = 4, .laborers = 20 }, // Work camp
    GRAND_TEMPLE, // Ceres
    GRAND_TEMPLE, // Neptune
    GRAND_TEMPLE, // Mercury
    GRAND_TEMPLE, // Mars
    GRAND_TEMPLE, // Venus
    NOTHING, // Menu GTs
    NOTHING, // Menu Trees
    NOTHING, // Menu Paths
    NOTHING, // Menu Parks
    MEDIUM_STATUE, // Small pond
    LARGE_STATUE, // Large pond
    SMALL_STATUE, // Pine tree
    SMALL_STATUE, // Fir
    SMALL_STATUE, // Oak
    SMALL_STATUE, // Elm
    SMALL_STATUE, // Fig
    SMALL_STATUE, // Plum
    SMALL_STATUE, // Palm
    SMALL_STATUE, // Date
    SMALL_STATUE, // Pine path
    SMALL_STATUE, // Fir
    SMALL_STATUE, // Oak
    SMALL_STATUE, // Elm
    SMALL_STATUE, // Fig
    SMALL_STATUE, // Plum
    SMALL_STATUE, // Palm
    SMALL_STATUE, // Date
    SMALL_STATUE, // Pavillon blue
    SMALL_STATUE, // Pavillon red
    SMALL_STATUE, // Pavillon orange
    SMALL_STATUE, // Pavillon yellow
    SMALL_STATUE, // Pavillon green
    SMALL_STATUE, // Goddess statue
    SMALL_STATUE, // Senator statue
    MEDIUM_STATUE, // Obelisk
    { .cost = 3500, .desirability_value = 20, .desirability_step = 2,
    .desirability_step_size = -4, .desirability_range = 8, .laborers = 50 }, // Pantheon
    { .cost = 200, .desirability_value = -8, .desirability_step = 1,
    .desirability_step_size = 2, .desirability_range = 4, .laborers = 12 }, // Architect guild
    { .cost = 100, .desirability_value = -8, .desirability_step = 1,
    .desirability_step_size = 2, .desirability_range = 4, .laborers = 10 }, // Mess hall
    { .cost = 1000, .desirability_value = 6, .desirability_step = 1,
    .desirability_step_size = -1, .desirability_range = 4, .laborers = 20 }, // Lighthouse
    NOTHING, // Menu Statues
    NOTHING, // Menu Gov Res
    { .cost = 40, .desirability_value = -2, .desirability_step = 1,
    .desirability_step_size = 1, .desirability_range = 6, .laborers = 8 }, // Tavern
    { .cost = 400, .desirability_value = 0, .desirability_step = 0,
    .desirability_step_size = 0, .desirability_range = 0, .laborers = 0 }, // Grand garden
    { .cost = 500, .desirability_value = -3, .desirability_step = 1,
    .desirability_step_size = 1, .desirability_range = 3, .laborers = 25 }, // Arena
    LARGE_STATUE, // Horse statue
    MEDIUM_STATUE, // Dolphin fountain
    SMALL_STATUE, // Hedge dark
    SMALL_STATUE, // Hedge light
    SMALL_STATUE, // Looped garden wall
    MEDIUM_STATUE, // Legionairy statue
    SMALL_STATUE, // Marble Column
    SMALL_STATUE, // Colonade
    LARARIUM, // Lararium
    { .cost = 400, .desirability_value = 12, .desirability_step = 2,
    .desirability_step_size = -1, .desirability_range = 6, .laborers = 0 }, // Nymphaeum
    { .cost = 250, .desirability_value = -8, .desirability_step = 1,
    .desirability_step_size = 3, .desirability_range = 5, .laborers = 0 }, // Small mausoleum
    { .cost = 500, .desirability_value = -10, .desirability_step = 1,
    .desirability_step_size = 3, .desirability_range = 6, .laborers = 0 }, // Large mausoleum
    { .cost = 100, .desirability_value = -6, .desirability_step = 1,
    .desirability_step_size = 2, .desirability_range = 3, .laborers = 8 }, // Watchtower
    { .cost = 6, .desirability_value = 0, .desirability_step = 0,
    .desirability_step_size = 0, .desirability_range = 0, .laborers = 0 }, // Palisade
    SMALL_STATUE, // Garden path
    { .cost = 500, .desirability_value = -10, .desirability_step = 2,
    .desirability_step_size = 3, .desirability_range = 4, .laborers = 20 }, // Caravanserai
    SMALL_STATUE, // Roofed garden wall
    ROADBLOCK, // Roofed garden wall gate
    ROADBLOCK, // Hedge gate dark
    ROADBLOCK, // Hedge gate light
    { .cost = 6, .desirability_value = 0, .desirability_step = 0,
    .desirability_step_size = 0, .desirability_range = 0, .laborers = 0 }, // Palisade gate
    SMALL_STATUE, // Gladiator statue
    { .cost = 100, .desirability_value = -4, .desirability_step = 1,
    .desirability_step_size = 2, .desirability_range = 2, .laborers = 0 }, // Highway
    { .cost = 100, .desirability_value = -6, .desirability_step = 1,
    .desirability_step_size = 1, .desirability_range = 4, .laborers = 30 }, // Goldmine
    { .cost = 250, .desirability_value = -3, .desirability_step = 1,
    .desirability_step_size = 1, .desirability_range = 3, .laborers = 40 }, // Mint
    { .cost = 100, .desirability_value = -3, .desirability_step = 1,
    .desirability_step_size = 1, .desirability_range = 2, .laborers = 15 }, // Depot
    { .cost = 40, .desirability_value = -6, .desirability_step = 1,
    .desirability_step_size = 1, .desirability_range = 4, .laborers = 10 }, // Sand pit
    { .cost = 60, .desirability_value = -6, .desirability_step = 1,
    .desirability_step_size = 1, .desirability_range = 4, .laborers = 10 }, // Stone quarry
    { .cost = 60, .desirability_value = -3, .desirability_step = 1,
    .desirability_step_size = 1, .desirability_range = 3, .laborers = 10 }, // Concrete maker
    { .cost = 80, .desirability_value = -3, .desirability_step = 1,
    .desirability_step_size = 1, .desirability_range = 3, .laborers = 10 }, // Brickworks
    SMALL_STATUE, // Paneled garden wall
    ROADBLOCK, // Paneled garden wall gate
    ROADBLOCK, // Looped garden wall gate
    LARARIUM, // Shrine Ceres
    LARARIUM, // Neptune
    LARARIUM, // Mercury
    LARARIUM, // Mars
    LARARIUM, // Venus
    NOTHING, // Menu Shrines
    NOTHING, // Menu Gardens
    { .cost = 12, .desirability_value = 3, .desirability_step = 1,
    .desirability_step_size = -1, .desirability_range = 3, .laborers = 0 }, // Overgrown gardens
    FORT, // Infantry
    { .cost = 50, .desirability_value = -5, .desirability_step = 1,
    .desirability_step_size = 1, .desirability_range = 4, .laborers = 6 }, // Armoury
    FORT, // Archers
    { .cost = 15, .desirability_value = 0, .desirability_step = 0,
    .desirability_step_size = 0, .desirability_range = 0, .laborers = 2 }, // Latrines
    { .cost = 50, .desirability_value = 0, .desirability_step = 0,
    .desirability_step_size = 0, .desirability_range = 0, .laborers = 0 }, // Native hut alt
    { .cost = 0, .desirability_value = -10, .desirability_step = 1,
    .desirability_step_size = 2, .desirability_range = 4, .laborers = 0 }, // Native Watchtower
    { .cost = 0, .desirability_value = 15, .desirability_step = 2,
    .desirability_step_size = -2, .desirability_range = 8, .laborers = 0 }, // Native Monument
    { .cost = 0, .desirability_value = 6, .desirability_step = 1,
    .desirability_step_size = -1, .desirability_range = 4, .laborers = 0 }, // Native Decoration
};


const model_building *model_get_building(building_type type)
{
    if (type > NUM_BUILDINGS + NUM_NEW_BUILDINGS) {
        return &NOTHING;
    }
    if (type > 114) {
        return &buildings[type + 15];
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
