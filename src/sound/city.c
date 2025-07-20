#include "city.h"

#include "building/properties.h"
#include "city/figures.h"
#include "core/file.h"
#include "core/random.h"
#include "core/time.h"
#include "game/settings.h"
#include "sound/device.h"

#include <string.h>

#define SOUND_VIEWS_THRESHOLD 200
#define SOUND_DELAY_MILLIS 30000
#define SOUND_PLAY_INTERVAL_MILLIS 2000

typedef enum {
    SOUND_AMBIENT_NONE = 0,
    SOUND_AMBIENT_FIRST = 1,
    SOUND_AMBIENT_EMPTY_LAND1 = 1,
    SOUND_AMBIENT_EMPTY_LAND2,
    SOUND_AMBIENT_EMPTY_LAND3, // SOUND_AMBIENT_RIVER,
    SOUND_AMBIENT_EMPTY_TERRAIN01,
    SOUND_AMBIENT_EMPTY_TERRAIN02,
    SOUND_AMBIENT_MAX
} sound_ambient_type;

typedef const char *sound_filenames;

typedef struct {
    int available;
    int total_views;
    int direction_views[5];
    time_millis last_played_time;
    struct {
        unsigned int total;
        unsigned int current;
        sound_filenames *list;
    } filenames;
} background_sound;

static struct {
    background_sound city_sounds[SOUND_CITY_MAX];
    background_sound ambient_sounds[SOUND_AMBIENT_MAX];
    time_millis last_update_time;
} data = {
    .city_sounds = {
        [SOUND_CITY_HOUSE_SLUM]         = { .filenames.total = 8, .filenames.list = (sound_filenames[]) { "wavs/house_slum1.wav", "wavs/house_slum2.wav", "wavs/house_slum3.wav", "wavs/house_slum4.wav", "wavs/house_poor1.wav", "wavs/house_poor2.wav", "wavs/house_poor3.wav", "wavs/house_poor4.wav" } },
        [SOUND_CITY_HOUSE_POOR]         = { .filenames.total = 8, .filenames.list = (sound_filenames[]) { "wavs/house_poor1.wav", "wavs/house_poor2.wav", "wavs/house_poor3.wav", "wavs/house_poor4.wav", "wavs/house_mid1.wav", "wavs/house_mid2.wav", "wavs/house_mid3.wav", "wavs/house_mid4.wav" } },
        [SOUND_CITY_HOUSE_MEDIUM]       = { .filenames.total = 8, .filenames.list = (sound_filenames[]) { "wavs/house_mid1.wav", "wavs/house_mid2.wav", "wavs/house_mid3.wav", "wavs/house_mid4.wav", "wavs/house_good2.wav", "wavs/house_good4.wav", "wavs/house_posh1.wav", "wavs/house_posh2.wav" } },
        [SOUND_CITY_HOUSE_GOOD]         = { .filenames.total = 8, .filenames.list = (sound_filenames[]) { "wavs/house_good1.wav", "wavs/house_good2.wav", "wavs/house_good3.wav", "wavs/house_good4.wav", "wavs/house_posh1.wav", "wavs/house_posh2.wav", "wavs/house_posh3.wav", "wavs/house_posh4.wav" } },
        [SOUND_CITY_HOUSE_POSH]         = { .filenames.total = 8, .filenames.list = (sound_filenames[]) { "wavs/house_posh1.wav", "wavs/house_posh2.wav", "wavs/house_posh3.wav", "wavs/house_posh4.wav", "wavs/house_good1.wav", "wavs/house_good2.wav", "wavs/house_good3.wav", "wavs/house_good4.wav" } },
        [SOUND_CITY_AMPHITHEATER]       = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/ampitheatre.wav" } },
        [SOUND_CITY_THEATER]            = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/theatre.wav" } },
        [SOUND_CITY_HIPPODROME]         = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/hippodrome.wav" } },
        [SOUND_CITY_COLOSSEUM]          = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/colloseum.wav" } },
        [SOUND_CITY_GLADIATOR_SCHOOL]   = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/glad_pit.wav" } },
        [SOUND_CITY_LION_PIT]           = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/lion_pit.wav" } },
        [SOUND_CITY_ACTOR_COLONY]       = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/art_pit.wav" } },
        [SOUND_CITY_CHARIOT_MAKER]      = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/char_pit.wav" } },
        [SOUND_CITY_GARDEN]             = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/gardens1.wav" } },
        [SOUND_CITY_CLINIC]             = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/clinic.wav" } },
        [SOUND_CITY_HOSPITAL]           = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/hospital.wav" } },
        [SOUND_CITY_BATHHOUSE]          = { .filenames.total = 2, .filenames.list = (sound_filenames[]) { "wavs/baths.wav", "wavs/aquaduct.wav" } },
        [SOUND_CITY_BARBER]             = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/barber.wav" } },
        [SOUND_CITY_SCHOOL]             = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/school.wav" } },
        [SOUND_CITY_ACADEMY]            = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/academy.wav" } },
        [SOUND_CITY_LIBRARY]            = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/library.wav" } },
        [SOUND_CITY_PREFECTURE]         = { .filenames.total = 2, .filenames.list = (sound_filenames[]) { ASSETS_DIRECTORY "/Sounds/Prefect.ogg", "wavs/prefecture.wav" } },
        [SOUND_CITY_FORT]               = { .filenames.total = 4, .filenames.list = (sound_filenames[]) { "wavs/fort1.wav", "wavs/fort2.wav", "wavs/fort3.wav", "wavs/fort4.wav" } },
        [SOUND_CITY_TOWER]              = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/tower1.wav" } },
        [SOUND_CITY_WATCHTOWER]         = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/tower2.wav" } },
        [SOUND_CITY_ARMOURY]            = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/tower3.wav" } },
        [SOUND_CITY_WORKCAMP]           = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/tower4.wav" } },
        [SOUND_CITY_TEMPLE_CERES]       = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/temp_farm.wav" } },
        [SOUND_CITY_TEMPLE_NEPTUNE]     = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/temp_ship.wav" } },
        [SOUND_CITY_TEMPLE_MERCURY]     = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/temp_comm.wav" } },
        [SOUND_CITY_TEMPLE_MARS]        = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/temp_war.wav" } },
        [SOUND_CITY_TEMPLE_VENUS]       = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/temp_love.wav" } },
        [SOUND_CITY_MARKET]             = { .filenames.total = 2, .filenames.list = (sound_filenames[]) { "wavs/market1.wav", "wavs/market4.wav" } },
        [SOUND_CITY_CARAVANSERAI]       = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/market2.wav" } },
        [SOUND_CITY_TAVERN]             = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/market3.wav" } },
        [SOUND_CITY_GRANARY]            = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/granary1.wav" } },
        [SOUND_CITY_WAREHOUSE]          = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/warehouse1.wav" } },
        [SOUND_CITY_MESS_HALL]          = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/warehouse2.wav" } },
        [SOUND_CITY_SHIPYARD]           = { .filenames.total = 2, .filenames.list = (sound_filenames[]) { "wavs/shipyard1.wav", "wavs/shipyard2.wav" } },
        [SOUND_CITY_DOCK]               = { .filenames.total = 2, .filenames.list = (sound_filenames[]) { "wavs/dock1.wav", "wavs/dock2.wav" } },
        [SOUND_CITY_WHARF]              = { .filenames.total = 2, .filenames.list = (sound_filenames[]) { "wavs/wharf1.wav", "wavs/wharf2.wav" } },
        [SOUND_CITY_PALACE]             = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/palace.wav" } },
        [SOUND_CITY_ENGINEERS_POST]     = { .filenames.total = 2, .filenames.list = (sound_filenames[]) { ASSETS_DIRECTORY "/Sounds/Engineer.ogg", "wavs/eng_post.wav" } },
        [SOUND_CITY_SENATE]             = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/senate.wav" } },
        [SOUND_CITY_FORUM]              = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/forum.wav" } },
        [SOUND_CITY_RESERVOIR]          = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/resevoir.wav" } },
        [SOUND_CITY_FOUNTAIN]           = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/fountain.wav" } },
        [SOUND_CITY_WELL]               = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/well1.wav" } },
        [SOUND_CITY_MILITARY_ACADEMY]   = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/mil_acad.wav" } },
        [SOUND_CITY_BARRACKS]           = { .filenames.total = 2, .filenames.list = (sound_filenames[]) { "wavs/barracks.wav", "wavs/marching.wav" } },
        [SOUND_CITY_ORACLE]             = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/oracle.wav" } },
        [SOUND_CITY_BURNING_RUIN]       = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/burning_ruin.wav" } },
        [SOUND_CITY_WHEAT_FARM]         = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/wheat_farm.wav" } },
        [SOUND_CITY_VEGETABLE_FARM]     = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/veg_farm.wav" } },
        [SOUND_CITY_FRUIT_FARM]         = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/figs_farm.wav" } },
        [SOUND_CITY_OLIVE_FARM]         = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/olives_farm.wav" } },
        [SOUND_CITY_VINE_FARM]          = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/vines_farm.wav" } },
        [SOUND_CITY_PIG_FARM]           = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/meat_farm.wav" } },
        [SOUND_CITY_QUARRY]             = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/quarry.wav" } },
        [SOUND_CITY_IRON_MINE]          = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/mine.wav" } },
        [SOUND_CITY_TIMBER_YARD]        = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/lumber_mill.wav" } },
        [SOUND_CITY_CLAY_PIT]           = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/clay_pit.wav" } },
        [SOUND_CITY_WINE_WORKSHOP]      = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/wine_workshop.wav" } },
        [SOUND_CITY_OIL_WORKSHOP]       = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/oil_workshop.wav" } },
        [SOUND_CITY_WEAPONS_WORKSHOP]   = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/weap_workshop.wav" } },
        [SOUND_CITY_FURNITURE_WORKSHOP] = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/furn_workshop.wav" } },
        [SOUND_CITY_POTTERY_WORKSHOP]   = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/pott_workshop.wav" } },
        [SOUND_CITY_CITY_MINT]          = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/coin.wav" } },
        [SOUND_CITY_MISSION_POST]       = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { ASSETS_DIRECTORY "/Sounds/MissionPost.ogg" } },
        [SOUND_CITY_BRICKWORKS]         = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { ASSETS_DIRECTORY "/Sounds/Brickworks.ogg" } },
        [SOUND_CITY_LIGHTHOUSE]         = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { ASSETS_DIRECTORY "/Sounds/Lighthouse.ogg" } },
        [SOUND_CITY_DEPOT]              = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { ASSETS_DIRECTORY "/Sounds/Ox.ogg" } },
        [SOUND_CITY_CONCRETE_MAKER]     = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { ASSETS_DIRECTORY "/Sounds/ConcreteMaker.ogg" } },
        [SOUND_CITY_CONSTRUCTION_SITE]  = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { ASSETS_DIRECTORY "/Sounds/Engineer.ogg" } },
        [SOUND_CITY_NATIVE_HUT]         = { .filenames.total = 1, .filenames.list = (sound_filenames[]) { ASSETS_DIRECTORY "/Sounds/NativeHut.ogg" } }
    },
    .ambient_sounds = {
        [SOUND_AMBIENT_EMPTY_LAND1] = {.filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/empty_land1.wav" } },
        [SOUND_AMBIENT_EMPTY_LAND2] = {.filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/empty_land2.wav" } },
        [SOUND_AMBIENT_EMPTY_LAND3] = {.filenames.total = 1, .filenames.list = (sound_filenames[]) { "wavs/empty_land3.wav" } },
        [SOUND_AMBIENT_EMPTY_TERRAIN01] = {.filenames.total = 1, .filenames.list = (sound_filenames[]) { ASSETS_DIRECTORY "/Sounds/Terrain01.ogg" } },
        [SOUND_AMBIENT_EMPTY_TERRAIN02] = {.filenames.total = 1, .filenames.list = (sound_filenames[]) { ASSETS_DIRECTORY "/Sounds/Terrain02.ogg" } }
    }
};

void sound_city_init(void)
{
    data.last_update_time = time_get_millis();
    for (sound_city_type sound = SOUND_CITY_FIRST; sound < SOUND_CITY_MAX; sound++) {
        background_sound *current_sound = &data.city_sounds[sound];
        current_sound->last_played_time = data.last_update_time;
        current_sound->available = 0;
        current_sound->total_views = 0;
        current_sound->filenames.current = 0;
        memset(current_sound->direction_views, 0, sizeof(current_sound->direction_views));
    }
    for (sound_ambient_type sound = SOUND_AMBIENT_FIRST; sound < SOUND_AMBIENT_MAX; sound++) {
        background_sound *current_sound = &data.ambient_sounds[sound];
        current_sound->last_played_time = data.last_update_time;
        current_sound->available = 0;
        current_sound->total_views = 0;
        current_sound->filenames.current = 0;
        memset(current_sound->direction_views, 0, sizeof(current_sound->direction_views));
    }
}

void sound_city_set_volume(int percentage)
{
    sound_device_set_volume_for_type(SOUND_TYPE_CITY, percentage);
}

void sound_city_mark_building_view(building_type type, int num_workers, int direction)
{
    sound_city_type sound = building_properties_for_type(type)->sound_id;

    if (sound == SOUND_CITY_NONE) {
        return;
    }
    if (type == BUILDING_THEATER || type == BUILDING_AMPHITHEATER ||
        type == BUILDING_GLADIATOR_SCHOOL || type == BUILDING_HIPPODROME) {
        // entertainment is shut off when caesar invades
        if (num_workers <= 0 || city_figures_imperial_soldiers() > 0) {
            return;
        }
    }

    data.city_sounds[sound].available = 1;
    ++data.city_sounds[sound].total_views;
    ++data.city_sounds[sound].direction_views[direction];
}

void sound_city_mark_construction_site_view(int direction)
{
    data.city_sounds[SOUND_CITY_CONSTRUCTION_SITE].available = 1;
    ++data.city_sounds[SOUND_CITY_CONSTRUCTION_SITE].total_views;
    ++data.city_sounds[SOUND_CITY_CONSTRUCTION_SITE].direction_views[direction];
}

void sound_city_decay_views(void)
{
    for (sound_city_type sound = SOUND_CITY_FIRST; sound < SOUND_CITY_MAX; sound++) {
        for (int d = 0; d < 5; d++) {
            data.city_sounds[sound].direction_views[d] = 0;
        }
        data.city_sounds[sound].total_views /= 2;
    }

    for (sound_ambient_type sound = SOUND_AMBIENT_FIRST; sound < SOUND_AMBIENT_MAX; sound++) {
        for (int d = 0; d < 5; d++) {
            data.ambient_sounds[sound].direction_views[d] = 0;
        }
        data.ambient_sounds[sound].total_views /= 2;
    }
}

void sound_city_progress_ambient(void)
{
    for (sound_ambient_type sound = SOUND_AMBIENT_FIRST; sound < SOUND_AMBIENT_MAX; sound++) {
        data.ambient_sounds[sound].available = 1;
        ++data.ambient_sounds[sound].total_views;
        ++data.ambient_sounds[sound].direction_views[SOUND_DIRECTION_CENTER];
    }
}

static const char *get_filename_for_sound(background_sound *sound)
{
    const char *filename = sound->filenames.list[sound->filenames.current];
    sound->filenames.current = (sound->filenames.current + 1) % sound->filenames.total;
    return filename;
}

static void play_sound(background_sound *sound, int direction)
{
    if (!setting_sound(SOUND_TYPE_CITY)->enabled || !sound->filenames.total) {
        return;
    }
    const char *filename = get_filename_for_sound(sound);
    if (sound_device_is_file_playing_on_channel(filename, SOUND_TYPE_CITY)) {
        return;
    }
    int left_pan;
    int right_pan;
    switch (direction) {
        case SOUND_DIRECTION_CENTER:
            left_pan = right_pan = 100;
            break;
        case SOUND_DIRECTION_LEFT:
            left_pan = 100;
            right_pan = 0;
            break;
        case SOUND_DIRECTION_RIGHT:
            left_pan = 0;
            right_pan = 100;
            break;
        default:
            left_pan = right_pan = 0;
            break;
    }
    sound_device_play_file_on_channel_panned(filename, SOUND_TYPE_CITY,
        setting_sound(SOUND_TYPE_CITY)->volume, left_pan, right_pan, 0);
}

void sound_city_play(void)
{
    time_millis now = time_get_millis();
    time_millis max_delay = 0;
    background_sound *sound_to_play = 0;
    for (sound_city_type sound = SOUND_CITY_FIRST; sound < SOUND_CITY_MAX; sound++) {
        background_sound *current_sound = &data.city_sounds[sound];
        if (current_sound->available) {
            current_sound->available = 0;
            if (current_sound->total_views >= SOUND_VIEWS_THRESHOLD) {
                if (now - current_sound->last_played_time >= SOUND_DELAY_MILLIS) {
                    if (now - current_sound->last_played_time > max_delay) {
                        max_delay = now - current_sound->last_played_time;
                        sound_to_play = current_sound;
                    }
                }
            }
        } else {
            current_sound->total_views = 0;
            for (int d = 0; d < 5; d++) {
                current_sound->direction_views[d] = 0;
            }
        }
    }

    if (now - data.last_update_time < SOUND_PLAY_INTERVAL_MILLIS) {
        // Only play 1 sound every 2 seconds
        return;
    }

    if (!sound_to_play) {
        // progress_ambient();
        return;
    }

    // always only one channel available... use it
    int direction;
    if (sound_to_play->direction_views[SOUND_DIRECTION_CENTER] > 10) {
        direction = SOUND_DIRECTION_CENTER;
    } else if (sound_to_play->direction_views[SOUND_DIRECTION_LEFT] > 10) {
        direction = SOUND_DIRECTION_LEFT;
    } else if (sound_to_play->direction_views[SOUND_DIRECTION_RIGHT] > 10) {
        direction = SOUND_DIRECTION_RIGHT;
    } else {
        direction = SOUND_DIRECTION_CENTER;
    }

    play_sound(sound_to_play, direction);
    data.last_update_time = now;
    sound_to_play->last_played_time = now;
    sound_to_play->total_views = 0;
    for (int d = 0; d < 5; d++) {
        sound_to_play->direction_views[d] = 0;
    }
}
