#include "phrase.h"

#include "building/building.h"
#include "building/market.h"
#include "city/constants.h"
#include "city/culture.h"
#include "city/figures.h"
#include "city/gods.h"
#include "city/labor.h"
#include "city/population.h"
#include "city/resource.h"
#include "city/sentiment.h"
#include "core/calc.h"
#include "core/file.h"
#include "figure/properties.h"
#include "figure/trader.h"
#include "figuretype/trader.h"
#include "sound/speech.h"
#include "sound/effect.h"

#include <stdio.h>

#define MAX_CITY_STATES 7
#define EXACT_STATE_OFFSET MAX_CITY_STATES
#define NO_PHRASE -1

static const char *CITY_STATE[MAX_CITY_STATES] = {
    "starv1", "nojob1", "needjob1", "nofun1", "relig1", "great1", "great2"
};

typedef struct {
    const char *name;
    unsigned int total_phrases;
    const char **phrases;
} figure_phrase_data;

static const figure_phrase_data FIGURE_PHRASE_DATA[FIGURE_SOUND_MAX] = {
    [FIGURE_SOUND_PREFECT] = { "vigils", 10 },
    [FIGURE_SOUND_TROOP] = { "wallguard", 5 },
    [FIGURE_SOUND_ENGINEER] = { "engine", 2 },
    [FIGURE_SOUND_TAX_COLLECTOR] = { "taxman", 3 },
    [FIGURE_SOUND_MARKET_LADY] = { "market", 3 },
    [FIGURE_SOUND_CARTPUSHER] = { "crtpsh", 3 },
    [FIGURE_SOUND_DONKEY] = { "donkey", 5 },
    [FIGURE_SOUND_BOATS] = { "boats", 5 },
    [FIGURE_SOUND_PRIEST] = { "priest" },
    [FIGURE_SOUND_TEACHER] = { "teach" },
    [FIGURE_SOUND_PUPILS] = { "pupils" },
    [FIGURE_SOUND_BATHOUSE_LADY] = { "bather" },
    [FIGURE_SOUND_DOCTOR] = { "doctor" },
    [FIGURE_SOUND_BARBER] = { "barber" },
    [FIGURE_SOUND_ACTOR] = { "actors" },
    [FIGURE_SOUND_GLADIATOR] = { "gladtr", 1 },
    [FIGURE_SOUND_LION_TAMER] = { "liontr", 3 },
    [FIGURE_SOUND_CHARIOTEER] = { "charot" },
    [FIGURE_SOUND_PATRICIAN] = { "patric" },
    [FIGURE_SOUND_PLEBIAN] = { "pleb" },
    [FIGURE_SOUND_RIOTER] = { "rioter", 3 },
    [FIGURE_SOUND_HOMELESS] = { "homeless", 2 },
    [FIGURE_SOUND_UNEMPLOYED] = { "unemploy", 2 },
    [FIGURE_SOUND_EMIGRANT] = { "emigrate", 4 },
    [FIGURE_SOUND_IMMIGRANT] = { "immigrant", 3 },
    [FIGURE_SOUND_ENEMY] = { "enemy" },
    [FIGURE_SOUND_MISSIONARY] = { "missionary", 4 },
    [FIGURE_SOUND_GRANARY_BOY] = { "granboy", 3 },
    [FIGURE_SOUND_OX_CART_PUSHER] = { .total_phrases = 1, .phrases = (const char *[]) { "Ox" } },
    [FIGURE_SOUND_DOG] = { .total_phrases = 1, .phrases = (const char *[]) { "Dog_Bark" } }
};

enum {
    GOD_STATE_NONE = 0,
    GOD_STATE_VERY_ANGRY = 1,
    GOD_STATE_ANGRY = 2
};

static const char *generate_sound_file(figure_sound_types sound_id, int phrase_id)
{
    static char filename[FILE_NAME_MAX];
    const figure_phrase_data *phrase_data = &FIGURE_PHRASE_DATA[sound_id];

    if (phrase_data->phrases) {
        if ((unsigned int) phrase_id >= phrase_data->total_phrases) {
            return 0;
        }
        snprintf(filename, FILE_NAME_MAX, ASSETS_DIRECTORY "/Sounds/%s.ogg", phrase_data->phrases[phrase_id]);
        return filename;
    }
    if (!phrase_data->name) {
        return 0;
    }
    if (phrase_id < MAX_CITY_STATES) {
        snprintf(filename, FILE_NAME_MAX, "wavs/%s_%s.wav", phrase_data->name, CITY_STATE[phrase_id]);
        return filename;
    }
    phrase_id -= EXACT_STATE_OFFSET;
    if ((unsigned int) phrase_id >= phrase_data->total_phrases) {
        return 0;
    }
    // We need to account for the wrongly named "mission" file for the missionary figure,
    // which contains the correct phrase and is used in the original game, instead of the expected "missionary_exact4.wav"
    const char *name = (sound_id == FIGURE_SOUND_MISSIONARY && phrase_id == 3) ? "mission" : phrase_data->name;
    snprintf(filename, FILE_NAME_MAX, "wavs/%s_exact%u.wav", name, phrase_id + 1);
    return filename;
}

static void play_sound_file(figure_sound_types sound_id, int phrase_id)
{
    if (sound_id > FIGURE_SOUND_NONE && phrase_id > NO_PHRASE) {
        sound_speech_play_file(generate_sound_file(sound_id, phrase_id));
    }
}

int figure_phrase_play(figure *f)
{
    if (f->id <= 0) {
        return FIGURE_SOUND_NONE;
    }
    if (f->type == FIGURE_ZEBRA) {
        sound_effect_play(SOUND_EFFECT_ZEBRA_DIE);
        return FIGURE_SOUND_NONE; //default behaviour
    } else if (f->type == FIGURE_SHEEP) {
        sound_effect_play(SOUND_EFFECT_SHEEP_BAA);
        return FIGURE_SOUND_NONE; //default behaviour
    } else if (f->type == FIGURE_WOLF) {
        sound_effect_play(SOUND_EFFECT_WOLF_HOWL);
        return FIGURE_SOUND_NONE; //default behaviour
    }
    figure_sound_types sound_id = figure_properties_for_type(f->type)->sound_type;
    play_sound_file(sound_id, f->phrase_id);
    return sound_id;
}

static int lion_tamer_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        if (++f->phrase_sequence_exact >= 3) {
            f->phrase_sequence_exact = 0;
        }
        return EXACT_STATE_OFFSET + f->phrase_sequence_exact;
    }
    return NO_PHRASE;
}

static int gladiator_phrase(figure *f)
{
    return f->action_state == FIGURE_ACTION_150_ATTACK ? EXACT_STATE_OFFSET : NO_PHRASE;
}

static int tax_collector_phrase(figure *f)
{
    if (f->min_max_seen >= HOUSE_LARGE_CASA) {
        return EXACT_STATE_OFFSET;
    } else if (f->min_max_seen >= HOUSE_SMALL_HOVEL) {
        return EXACT_STATE_OFFSET + 1;
    } else if (f->min_max_seen >= HOUSE_LARGE_TENT) {
        return EXACT_STATE_OFFSET + 2;
    } else {
        return NO_PHRASE;
    }
}

static int market_trader_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_126_ROAMER_RETURNING) {
        if (building_market_get_max_food_stock(building_get(f->building_id)) <= 0) {
            return EXACT_STATE_OFFSET + 2; // run out of goods
        }
    }
    return NO_PHRASE;
}

static int market_supplier_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE) {
        return EXACT_STATE_OFFSET;
    } else if (f->action_state == FIGURE_ACTION_146_SUPPLIER_RETURNING) {
        resource_type resource = f->collecting_item_id;
        if (resource != RESOURCE_NONE) {
            return EXACT_STATE_OFFSET + 1;
        } else {
            return NO_PHRASE;
        }
    } else {
        return NO_PHRASE;
    }
}

static int cart_pusher_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_20_CARTPUSHER_INITIAL) {
        if (f->min_max_seen == 2) {
            return EXACT_STATE_OFFSET;
        } else if (f->min_max_seen == 1) {
            return EXACT_STATE_OFFSET + 1;
        }
    } else if (f->action_state == FIGURE_ACTION_21_CARTPUSHER_DELIVERING_TO_WAREHOUSE ||
            f->action_state == FIGURE_ACTION_22_CARTPUSHER_DELIVERING_TO_GRANARY ||
            f->action_state == FIGURE_ACTION_23_CARTPUSHER_DELIVERING_TO_WORKSHOP) {
        if (calc_maximum_distance(
            f->destination_x, f->destination_y, f->source_x, f->source_y) >= 25) {
            return EXACT_STATE_OFFSET + 2; // too far
        }
    }
    return NO_PHRASE;
}

static int mess_hall_supplier_phrase(figure *f)
{
    return 0;
}

static int warehouseman_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE) {
        if (calc_maximum_distance(
            f->destination_x, f->destination_y, f->source_x, f->source_y) >= 25) {
            return EXACT_STATE_OFFSET + 2; // too far
        }
    }
    return NO_PHRASE;
}

static int prefect_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 4) {
        f->phrase_sequence_exact = 0;
    }
    if (f->action_state == FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE) {
        return EXACT_STATE_OFFSET + 3;;
    } else if (f->action_state == FIGURE_ACTION_75_PREFECT_AT_FIRE) {
        return EXACT_STATE_OFFSET + 4 + (f->phrase_sequence_exact % 2);
    } else if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        return EXACT_STATE_OFFSET + 6 + f->phrase_sequence_exact;
    } else if (f->min_max_seen >= 50) {
        // alternate between "no sign of crime around here" and the regular city phrases
        if (f->phrase_sequence_exact % 2) {
            return EXACT_STATE_OFFSET;
        } else {
            return NO_PHRASE;
        }
    } else if (f->min_max_seen >= 10) {
        return EXACT_STATE_OFFSET + 1;
    } else {
        return EXACT_STATE_OFFSET + 2;
    }
}

static int engineer_phrase(figure *f)
{
    if (f->min_max_seen >= 60) {
        return EXACT_STATE_OFFSET;
    } else if (f->min_max_seen >= 10) {
        return EXACT_STATE_OFFSET + 1;
    } else {
        return NO_PHRASE;
    }
}

static int citizen_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 3) {
        f->phrase_sequence_exact = 0;
    }
    return EXACT_STATE_OFFSET + f->phrase_sequence_exact;
}

static int missionary_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 4) {
        f->phrase_sequence_exact = 0;
    }
    return EXACT_STATE_OFFSET + f->phrase_sequence_exact;
}

static int ox_phrase(figure *f)
{
    return 0;
}

static int dog_phrase(figure *f)
{
    return 0;
}

static int homeless_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 2) {
        f->phrase_sequence_exact = 0;
    }
    return EXACT_STATE_OFFSET + f->phrase_sequence_exact;
}

static int house_seeker_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 3) {
        f->phrase_sequence_exact = 0;
    }
    return EXACT_STATE_OFFSET + f->phrase_sequence_exact;
}

static int emigrant_phrase(void)
{
    switch (city_sentiment_low_mood_cause()) {
        case LOW_MOOD_CAUSE_NO_JOBS:
            return EXACT_STATE_OFFSET;
        case LOW_MOOD_CAUSE_NO_FOOD:
            return EXACT_STATE_OFFSET + 1;
        case LOW_MOOD_CAUSE_HIGH_TAXES:
            return EXACT_STATE_OFFSET + 2;
        case LOW_MOOD_CAUSE_LOW_WAGES:
            return EXACT_STATE_OFFSET + 3;
        default:
            return EXACT_STATE_OFFSET + 4;
    }
}

static int tower_sentry_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 2) {
        f->phrase_sequence_exact = 0;
    }
    int enemies = city_figures_enemies();
    if (!enemies) {
        return EXACT_STATE_OFFSET + f->phrase_sequence_exact;
    } else if (enemies <= 10) {
        return EXACT_STATE_OFFSET + 2;
    } else if (enemies <= 30) {
        return EXACT_STATE_OFFSET + 3;
    } else {
        return EXACT_STATE_OFFSET + 4;
    }
}

static int soldier_phrase(void)
{
    int enemies = city_figures_enemies();
    if (enemies >= 40) {
        return EXACT_STATE_OFFSET + 4;
    } else if (enemies > 20) {
        return EXACT_STATE_OFFSET + 3;
    } else if (enemies) {
        return EXACT_STATE_OFFSET + 2;
    }
    return NO_PHRASE;
}

static int docker_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_135_DOCKER_IMPORT_GOING_TO_STORAGE ||
        f->action_state == FIGURE_ACTION_136_DOCKER_EXPORT_GOING_TO_STORAGE) {
        if (calc_maximum_distance(
            f->destination_x, f->destination_y, f->source_x, f->source_y) >= 25) {
            return EXACT_STATE_OFFSET + 2; // too far
        }
    }
    return NO_PHRASE;
}

static int trade_caravan_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 2) {
        f->phrase_sequence_exact = 0;
    }
    if (f->action_state == FIGURE_ACTION_103_TRADE_CARAVAN_LEAVING) {
        if (!trader_has_traded(f->trader_id)) {
            return EXACT_STATE_OFFSET; // no trade
        }
    } else if (f->action_state == FIGURE_ACTION_102_TRADE_CARAVAN_TRADING) {
        if (figure_trade_caravan_can_buy(f, f->destination_building_id, f->empire_city_id)) {
            return EXACT_STATE_OFFSET + 4; // buying goods
        } else if (figure_trade_caravan_can_sell(f, f->destination_building_id, f->empire_city_id)) {
            return EXACT_STATE_OFFSET + 3; // selling goods
        }
    }
    return EXACT_STATE_OFFSET + 1 + f->phrase_sequence_exact;
}

static int trade_ship_phrase(figure *f)
{
    if (f->action_state == FIGURE_ACTION_115_TRADE_SHIP_LEAVING) {
        if (!trader_has_traded(f->trader_id)) {
            return EXACT_STATE_OFFSET + 2; // no trade
        } else {
            return EXACT_STATE_OFFSET + 4; // good trade
        }
    } else if (f->action_state == FIGURE_ACTION_112_TRADE_SHIP_MOORED) {
        int state = figure_trade_ship_is_trading(f);
        if (state == TRADE_SHIP_BUYING) {
            return EXACT_STATE_OFFSET + 1; // buying goods
        } else if (state == TRADE_SHIP_SELLING) {
            return EXACT_STATE_OFFSET; // selling goods
        } else {
            if (!trader_has_traded(f->trader_id)) {
                return EXACT_STATE_OFFSET + 2; // no trade
            } else {
                return EXACT_STATE_OFFSET + 4; // good trade
            }
        }
    } else {
        if (!trader_has_traded(f->trader_id)) {
            return EXACT_STATE_OFFSET + 3; // can't wait to trade
        } else {
            return EXACT_STATE_OFFSET + 4; // good trade
        }
    }
}

static int city_god_state(void)
{
    int least_god_happiness = 100;
    for (int i = 0; i < MAX_GODS; i++) {
        int happiness = city_god_happiness(i);
        if (happiness < least_god_happiness) {
            least_god_happiness = happiness;
        }
    }
    if (least_god_happiness < 20) {
        return GOD_STATE_VERY_ANGRY;
    } else if (least_god_happiness < 40) {
        return GOD_STATE_ANGRY;
    } else {
        return GOD_STATE_NONE;
    }
}

static int barkeep_phrase(figure *f)
{
    f->phrase_sequence_city = 0;
    int god_state = city_god_state();
    int unemployment_pct = city_labor_unemployment_percentage();

    if (unemployment_pct >= 17) {
        return 1;
    } else if (city_labor_workers_needed() >= 10) {
        return 2;
    } else if (city_culture_average_entertainment() == 0) {
        return 3;
    } else if (god_state == GOD_STATE_VERY_ANGRY) {
        return 4;
    } else if (city_culture_average_entertainment() <= 10) {
        return 3;
    } else if (god_state == GOD_STATE_ANGRY) {
        return 4;
    } else if (city_culture_average_entertainment() <= 20) {
        return 3;
    } else if (city_resource_food_supply_months() >= 4 &&
        unemployment_pct <= 5 &&
        city_culture_average_health() > 0 &&
        city_culture_average_education() > 0) {
        if (city_population() < 500) {
            return 5;
        } else {
            return 6;
        }
    } else if (unemployment_pct >= 10) {
        return 1;
    } else {
        return 5;
    }
}

static int beggar_phrase(figure *f)
{
    if (++f->phrase_sequence_exact >= 2) {
        f->phrase_sequence_exact = 0;
    }
    return EXACT_STATE_OFFSET + f->phrase_sequence_exact;
}

static int phrase_based_on_figure_state(figure *f)
{
    switch (f->type) {
        case FIGURE_LION_TAMER:
            return lion_tamer_phrase(f);
        case FIGURE_GLADIATOR:
            return gladiator_phrase(f);
        case FIGURE_TAX_COLLECTOR:
            return tax_collector_phrase(f);
        case FIGURE_MARKET_TRADER:
            return market_trader_phrase(f);
        case FIGURE_MARKET_SUPPLIER:
            return market_supplier_phrase(f);
        case FIGURE_CART_PUSHER:
            return cart_pusher_phrase(f);
        case FIGURE_WAREHOUSEMAN:
            return warehouseman_phrase(f);
        case FIGURE_PREFECT:
            return prefect_phrase(f);
        case FIGURE_ENGINEER:
        case FIGURE_WORK_CAMP_ARCHITECT:
            return engineer_phrase(f);
        case FIGURE_PROTESTER:
        case FIGURE_CRIMINAL:
        case FIGURE_RIOTER:
        case FIGURE_CRIMINAL_ROBBER:
        case FIGURE_CRIMINAL_LOOTER:
        case FIGURE_DELIVERY_BOY:
            return citizen_phrase(f);
        case FIGURE_MISSIONARY:
            return missionary_phrase(f);
        case FIGURE_DEPOT_CART_PUSHER:
            return ox_phrase(f);
        case FIGURE_DOG:
            return dog_phrase(f);
        case FIGURE_HOMELESS:
            return homeless_phrase(f);
        case FIGURE_IMMIGRANT:
            return house_seeker_phrase(f);
        case FIGURE_EMIGRANT:
            return emigrant_phrase();
        case FIGURE_TOWER_SENTRY:
        case FIGURE_WATCHMAN:
            return tower_sentry_phrase(f);
        case FIGURE_MESS_HALL_SUPPLIER:
            return mess_hall_supplier_phrase(f);
        case FIGURE_FORT_JAVELIN:
        case FIGURE_FORT_MOUNTED:
        case FIGURE_FORT_LEGIONARY:
        case FIGURE_FORT_INFANTRY:
        case FIGURE_FORT_ARCHER:
            return soldier_phrase();
        case FIGURE_DOCKER:
            return docker_phrase(f);
        case FIGURE_TRADE_CARAVAN:
            return trade_caravan_phrase(f);
        case FIGURE_BARKEEP:
        case FIGURE_BARKEEP_SUPPLIER:
            return barkeep_phrase(f);
        case FIGURE_TRADE_CARAVAN_DONKEY:
            while (f->type == FIGURE_TRADE_CARAVAN_DONKEY && f->leading_figure_id) {
                f = figure_get(f->leading_figure_id);
            }
            return f->type == FIGURE_TRADE_CARAVAN ? trade_caravan_phrase(f) : NO_PHRASE;
        case FIGURE_TRADE_SHIP:
            return trade_ship_phrase(f);
        case FIGURE_BEGGAR:
            return beggar_phrase(f);
    }
    return NO_PHRASE;
}

static int phrase_based_on_city_state(figure *f)
{
    f->phrase_sequence_city = 0;
    int god_state = city_god_state();
    int unemployment_pct = city_labor_unemployment_percentage();

    if (city_resource_food_supply_months() <= 0) {
        return 0;
    } else if (unemployment_pct >= 17) {
        return 1;
    } else if (city_labor_workers_needed() >= 10) {
        return 2;
    } else if (city_culture_average_entertainment() == 0) {
        return 3;
    } else if (god_state == GOD_STATE_VERY_ANGRY) {
        return 4;
    } else if (city_culture_average_entertainment() <= 10) {
        return 3;
    } else if (god_state == GOD_STATE_ANGRY) {
        return 4;
    } else if (city_culture_average_entertainment() <= 20) {
        return 3;
    } else if (city_resource_food_supply_months() >= 4 &&
            unemployment_pct <= 5 &&
            city_culture_average_health() > 0 &&
            city_culture_average_education() > 0) {
        if (city_population() < 500) {
            return 5;
        } else {
            return 6;
        }
    } else if (unemployment_pct >= 10) {
        return 1;
    } else {
        return 5;
    }
}

void figure_phrase_determine(figure *f)
{
    if (f->id <= 0) {
        return;
    }
    f->phrase_id = 0;

    if (figure_is_enemy(f) || f->type == FIGURE_INDIGENOUS_NATIVE || f->type == FIGURE_NATIVE_TRADER) {
        f->phrase_id = NO_PHRASE;
        return;
    }

    int phrase_id = phrase_based_on_figure_state(f);
    if (phrase_id != NO_PHRASE) {
        f->phrase_id = phrase_id;
    } else {
        f->phrase_id = phrase_based_on_city_state(f);
    }
}
