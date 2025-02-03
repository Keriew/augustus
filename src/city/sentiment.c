#include "sentiment.h"

#include "building/building.h"
#include "building/model.h"
#include "city/constants.h"
#include "city/data_private.h"
#include "city/figures.h"
#include "city/games.h"
#include "city/gods.h"
#include "city/message.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/random.h"
#include "game/difficulty.h"
#include "game/settings.h"
#include "game/time.h"
#include "game/tutorial.h"

#include <math.h>
#include <stdlib.h>

#define MAX_SENTIMENT_FROM_EXTRA_ENTERTAINMENT 24
#define MAX_SENTIMENT_FROM_EXTRA_FOOD 24
#define FESTIVAL_BOOST_DECREMENT_RATE 0.84
#define UNEMPLOYMENT_THRESHHOLD 5
#define MAX_TAX_MULTIPLIER 12
#define WAGE_NEGATIVE_MODIFIER 3
#define WAGE_POSITIVE_MODIFIER 2
#define SQUALOR_MULTIPLIER 3
#define SENTIMENT_PER_ENTERTAINMENT 1
#define SENTIMENT_PER_EXTRA_FOOD 12
#define MAX_SENTIMENT_CHANGE 2
#define DESIRABILITY_TO_SENTIMENT_RATIO 2
#define COOLDOWN_AFTER_CRIME_DAYS 10
#define EXECUTIONS_GAMES_SENTIMENT_BONUS 20
#define IMPERIAL_GAMES_SENTIMENT_BONUS 15
#define POP_STEP_FOR_BASE_AVERAGE_HOUSE_LEVEL 625

#define min(a,b)             \
({                           \
    __typeof__ (a) _a = (a); \
    __typeof__ (b) _b = (b); \
    _a < _b ? _a : _b;       \
})

// New advanced sentiment change calculation applies only after city reaches
// population of 1000. This is required to prevent very fast sentiment drop 
// by high unemployment at very early game
#define ADVANCED_SENTIMENT_CHANGE_APPLY_AFTER_POPULATION 1000

int city_sentiment(void)
{
    return city_data.sentiment.value;
}

int city_sentiment_low_mood_cause(void)
{
    return city_data.sentiment.low_mood_cause;
}

void city_sentiment_change_happiness(int amount)
{
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_IN_USE && b->house_size) {
                b->sentiment.house_happiness = calc_bound(b->sentiment.house_happiness + amount, 0, 100);
            }
        }
    }
}

void city_sentiment_set_max_happiness(int max)
{
    max = calc_bound(max, 0, 100);
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_IN_USE && b->house_size) {
                if (b->sentiment.house_happiness > max) {
                    b->sentiment.house_happiness = max;
                }
            }
        }
    }
}

void city_sentiment_set_min_happiness(int min)
{
    min = calc_bound(min, 0, 100);
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_IN_USE && b->house_size) {
                if (b->sentiment.house_happiness < min) {
                    b->sentiment.house_happiness = min;
                }
            }
        }
    }
}

int city_sentiment_get_population_below_happiness(int happiness)
{
    int population = 0;
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state == BUILDING_STATE_IN_USE && b->house_size) {
                if (b->sentiment.house_happiness < happiness) {
                    population += b->house_population;
                }
            }
        }
    }
    return population;
}

void city_sentiment_reset_protesters_criminals(void)
{
    city_data.sentiment.protesters = 0;
    city_data.sentiment.criminals = 0;
}

void city_sentiment_add_protester(void)
{
    city_data.sentiment.protesters++;
}

void city_sentiment_add_criminal(void)
{
    city_data.sentiment.criminals++;
}

int city_sentiment_protesters(void)
{
    return city_data.sentiment.protesters;
}

int city_sentiment_criminals(void)
{
    return city_data.sentiment.criminals;
}

int city_sentiment_crime_cooldown(void)
{
    return city_data.sentiment.crime_cooldown;
}

void city_sentiment_set_crime_cooldown(void)
{
    city_data.sentiment.crime_cooldown = COOLDOWN_AFTER_CRIME_DAYS;
}

void city_sentiment_reduce_crime_cooldown(void)
{
    if (city_sentiment_crime_cooldown() > 0) {
        city_data.sentiment.crime_cooldown -= 1;
    }
}


int city_sentiment_get_blessing_festival_boost(void)
{
    return city_data.sentiment.blessing_festival_boost;
}

void city_sentiment_decrement_blessing_boost(void)
{
    short new_sentiment = (short) (city_data.sentiment.blessing_festival_boost * FESTIVAL_BOOST_DECREMENT_RATE);
    city_data.sentiment.blessing_festival_boost = new_sentiment;
}

static int get_games_bonus(void)
{
    int bonus = 0;
    if (city_games_executions_active()) {
        bonus += EXECUTIONS_GAMES_SENTIMENT_BONUS;
    }
    if (city_games_imperial_festival_active()) {
        bonus += IMPERIAL_GAMES_SENTIMENT_BONUS;
    }
    return bonus;
}

static int get_wage_sentiment_modifier(void)
{
    const int wage_interval = 8;
    int use_advanced_sentiment_contribution = config_get(CONFIG_GP_CH_ADVANCED_TAX_WAGE_SENTIMENT_CONTRIBUTION);
    int wage_differential = city_data.labor.wages - city_data.labor.wages_rome;
    if (use_advanced_sentiment_contribution && wage_differential > wage_interval) {
        // Extra sentiment bonus modifier applies when player sets higher wages than Rome. Wage range is 
        // divided by 8dn intervals. For every next wage interval the modifier is decreased by 1/2. 
        // Example:
        // Rome pays 30dn, player sets current wage level to 55dn. Wage difference will be 25dn and
        // the interval length is 8dn. Base bonus modifier is 2. Modifier will be set for every 
        // wage interval to values [2, 1, 0.5, 0.25].
        // The final modifier: 8 * 2 + 8 * 1 + 8 * 0.5 + 1 * 0.25) = 28 (original value was 50).
        int remaining = abs(wage_differential);
        wage_differential = 0;

        int modifier = 100 * WAGE_POSITIVE_MODIFIER;
        while (remaining > 0) {
            int diff = calc_bound(remaining, 1, wage_interval);
            wage_differential += diff * modifier;
            remaining -= diff;
            modifier >>= 1; // reduces sentiment bonus modifier by 50%
        }
        wage_differential /= 100;
    } else if (use_advanced_sentiment_contribution && wage_differential < -wage_interval) {
        // Extra sentiment punishment modifier applies when player sets lower wages than Rome. Wage range is 
        // divided by 8dn intervals. For every next wage interval the modifier is increased by 1/2. 
        // Example:
        // Rome pays 40dn, player sets current wage level to 15dn. Wage difference will be -25dn and
        // the interval length is 8dn. Base punishment modifier is 3. Modifier will be set for every 
        // wage interval to values [3, 4.5, 6.75, 10.125].
        // The final modifier: -(8 * 3 + 8 * 4.5 + 8 * 6.75 + 1 * 10.125) = -124 (original value was -75).
        int remaining = abs(wage_differential);
        wage_differential = 0;

        int modifier = 100 * WAGE_NEGATIVE_MODIFIER;
        while (remaining > 0) {
            int diff = calc_bound(remaining, 1, wage_interval);
            wage_differential -= diff * modifier;
            remaining -= diff;
            modifier += modifier / 2; // adds 50% sentiment points reduction
        }
        wage_differential /= 100;
    } else {
        wage_differential *= (wage_differential > 0 ? WAGE_POSITIVE_MODIFIER : WAGE_NEGATIVE_MODIFIER);
    }
    return wage_differential;
}

static int get_unemployment_sentiment_modifier(void)
{
    int unhappiness_treshhold = UNEMPLOYMENT_THRESHHOLD + city_god_venus_bonus_employment();
    if (city_data.labor.unemployment_percentage > unhappiness_treshhold) {
        return (city_data.labor.unemployment_percentage - unhappiness_treshhold);
    }
    return 0;
}

static int get_sentiment_modifier_for_tax_rate(int tax)
{
    int base_tax = difficulty_base_tax_rate();
    int interval = calc_bound(base_tax / 2, 1, 8); // Calculate the tax interval length based on difficulty
    int sentiment_modifier = base_tax - tax; // The original base sentiment modifier if advanced logic isn't enabled
    if (sentiment_modifier < -interval && config_get(CONFIG_GP_CH_ADVANCED_TAX_WAGE_SENTIMENT_CONTRIBUTION)) {
        // Extra sentiment punishment modifier applies when player sets higher taxes. Tax range is 
        // divided by intervals which length is half of base tax rate based on difficulty settings.
        // For every next tax interval the modifier is increased by 1/3. 
        // Example:
        // In very hard mode the base tax rate is 6%. Player sets current tax level to 16%.
        // Tax difference will be 10% and the interval length is 3% (half of base tax rate 6%).
        // Base punishment modifier is a diff between 12% and base tax rate (6%).
        // Modifier will be set for every tax interval to values [6, 8, 10.66, 14.2].
        // The final modifier: -(3% * 6 + 3% * 8 + 3% * 10.66 + 1% * 14.2) = -88 (original value was -60).
        int remaining = abs(sentiment_modifier);
        sentiment_modifier = 0;

        int modifier = 100 * (MAX_TAX_MULTIPLIER - base_tax);
        while (remaining > 0) {
            int diff = calc_bound(remaining, 1, interval);
            sentiment_modifier -= diff * modifier;
            remaining -= diff;
            modifier += modifier / 3; // adds 33% sentiment points reduction
        }
        sentiment_modifier /= 100;
    } else {
        sentiment_modifier *= sentiment_modifier < 0 ? (MAX_TAX_MULTIPLIER - base_tax) : (base_tax / 2);
    }

    return sentiment_modifier;
}

static int get_average_housing_level(void)
{
    int avg = 0;
    int population = 0;
    int multiplier = 1;

    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        if (type == BUILDING_HOUSE_LARGE_CASA) {
            multiplier = 10;
        } else if (type == BUILDING_HOUSE_SMALL_VILLA) {
            multiplier = 20;
        } else if (type == BUILDING_HOUSE_SMALL_PALACE) {
            multiplier = 30;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE || !b->house_size) {
                continue;
            }
            avg += b->subtype.house_level * b->house_population * multiplier;
            population += b->house_population * multiplier;
        }
    }
    if (population) {
        avg = avg / population;
    }
    int base_average_level = population / POP_STEP_FOR_BASE_AVERAGE_HOUSE_LEVEL;
    base_average_level = calc_bound(base_average_level, HOUSE_SMALL_TENT, HOUSE_SMALL_INSULA);
    return avg < base_average_level ? base_average_level : avg;
}

static int house_level_sentiment_modifier(int level, int average)
{
    int diff_from_average = level - average;
    if (diff_from_average < 0) {
        diff_from_average *= SQUALOR_MULTIPLIER;
    }
    return diff_from_average;
}

static int extra_desirability_bonus(int desirability, int max)
{
    int extra = (desirability - 50) / DESIRABILITY_TO_SENTIMENT_RATIO;
    return calc_bound(extra, 0, max);
}

static int extra_entertainment_bonus(int entertainment, int required)
{
    int extra = (entertainment - required) * SENTIMENT_PER_ENTERTAINMENT;
    return calc_bound(extra, 0, MAX_SENTIMENT_FROM_EXTRA_ENTERTAINMENT);
}

static int extra_food_bonus(int types, int required)
{
    // Tents have no food bonus because they always scavenge for food
    if (required == 0) {
        return 0;
    }
    int extra = ((types - required) * SENTIMENT_PER_EXTRA_FOOD);
    return calc_bound(extra, 0, MAX_SENTIMENT_FROM_EXTRA_FOOD);
}

const int advanced_sentiment_gain_modifier[5] = {
    30, // Very Easy
    25, // Easy
    20, // Normal
    17, // Hard
    15  // Very Hard
};

const int advanced_sentiment_drop_modifier[5] = {
    30, // Very Easy
    35, // Easy
    40, // Normal
    45, // Hard
    50  // Very Hard
};

// Updates house building sentiment cooldown by delta value.
// Returns 1 if advanced sentiment logic should be applied to house and 0 if not
int update_house_advanced_sentiment_cooldown(building *b, int sentiment_cooldown_delta) {
    if (!building_is_house(b->type)) {
        return 0;
    }

    if (b->type >= BUILDING_HOUSE_SMALL_VILLA) {
        // Reset cooldown for villas
        b->cooldown_advanced_sentiment = 0;
    } else if (b->type == BUILDING_HOUSE_VACANT_LOT) {
        // Wait for new citizens to arrive
        return 0;
    }

    if (sentiment_cooldown_delta > 0 && b->cooldown_advanced_sentiment > 0) {
        b->cooldown_advanced_sentiment -= min(sentiment_cooldown_delta, b->cooldown_advanced_sentiment);
    }

    if (!b->cooldown_advanced_sentiment) {
        // Cooldown has ended
        return 1;
    }

    return 0;
}

void city_sentiment_update(int sentiment_cooldown_delta)
{
    city_population_check_consistency();

    int default_sentiment = difficulty_sentiment();
    int houses_calculated = 0;
    int sentiment_contribution_taxes = get_sentiment_modifier_for_tax_rate(city_data.finance.tax_percentage);
    int sentiment_contribution_no_tax = get_sentiment_modifier_for_tax_rate(0) / 2;
    int sentiment_contribution_wages = get_wage_sentiment_modifier();
    int sentiment_contribution_unemployment = get_unemployment_sentiment_modifier();
    int average_housing_level = get_average_housing_level();
    int blessing_festival_boost = city_data.sentiment.blessing_festival_boost;
    int average_squalor_penalty = 0;
    int games_bonus = get_games_bonus();

    int total_sentiment = 0;
    int total_pop = 0;
    int total_houses = 0;
    int house_level_sentiment_multiplier = 3;
    int apply_advanced_sentiment_change = config_get(CONFIG_GP_CH_ADVANCED_TAX_WAGE_SENTIMENT_CONTRIBUTION) &&
        city_data.population.population >= ADVANCED_SENTIMENT_CHANGE_APPLY_AFTER_POPULATION;

    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type++) {
        if (type == BUILDING_HOUSE_SMALL_SHACK) {
            house_level_sentiment_multiplier = 2;
        } else if (type == BUILDING_HOUSE_LARGE_CASA) {
            house_level_sentiment_multiplier = 1;
        } else if (type == BUILDING_HOUSE_SMALL_VILLA) {
            house_level_sentiment_multiplier = 0;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE || !b->house_size) {
                continue;
            }
            if (!b->house_population) {
                b->sentiment.house_happiness = calc_bound(city_data.sentiment.value + 10, 0, 100);
                continue;
            }

            int sentiment = default_sentiment;
            if (b->subtype.house_level > HOUSE_GRAND_INSULA) {
                // Reduce sentiment contribution from taxes for villas by 20%
                sentiment += (b->house_tax_coverage ? sentiment_contribution_taxes : sentiment_contribution_no_tax) * 8 / 10;
            } else {
                sentiment += b->house_tax_coverage ? sentiment_contribution_taxes : sentiment_contribution_no_tax;
            }

            if (b->subtype.house_level <= HOUSE_GRAND_INSULA) {
                sentiment += sentiment_contribution_wages;
                sentiment -= sentiment_contribution_unemployment;
            }

            int house_level_sentiment = house_level_sentiment_modifier(b->subtype.house_level, average_housing_level);
            if (house_level_sentiment < 0) {
                house_level_sentiment *= house_level_sentiment_multiplier;
                average_squalor_penalty += house_level_sentiment;
            }
            sentiment += house_level_sentiment;

            int max_desirability = b->subtype.house_level + 1;
            int desirability_bonus = extra_desirability_bonus(b->desirability, max_desirability);
            int entertainment_bonus = extra_entertainment_bonus(b->data.house.entertainment,
                model_get_house(b->subtype.house_level)->entertainment);
            int food_bonus = extra_food_bonus(b->data.house.num_foods,
                model_get_house(b->subtype.house_level)->food_types);

            sentiment += desirability_bonus;
            sentiment += entertainment_bonus;
            sentiment += food_bonus;
            sentiment += games_bonus;

            sentiment += blessing_festival_boost;

            sentiment = calc_bound(sentiment, 0, 100); // new sentiment value should be in range of 0..100

            // Change sentiment gradually to the new value
            int sentiment_delta = sentiment - b->sentiment.house_happiness;
            if (sentiment_delta != 0 &&
                update_house_advanced_sentiment_cooldown(b, sentiment_cooldown_delta) &&
                apply_advanced_sentiment_change
            ) {
                // With new advanced logic we introduce faster sentiment change when the target value is
                // far away from current happiness level. The final change value depends on difficulty settings.
                // Example #1:
                // Current house happiness level is 82, the new sentiment value is 10 and the delta is -72.
                // The final happiness change for VeryHard mode will be -36 (50% of -72).
                // Example #2:
                // Current house happiness level is 20, the new sentiment value is 77 and the delta is 57.
                // The final happiness change for Hard mode will be 9 (17% of 57).
                if (sentiment_delta > 0) {
                    int gain_modifier = advanced_sentiment_gain_modifier[setting_difficulty()];
                    sentiment_delta = calc_bound(sentiment_delta * gain_modifier / 100, 1, 100);
                } else {
                    int drop_modifier = advanced_sentiment_drop_modifier[setting_difficulty()];
                    sentiment_delta = calc_bound(sentiment_delta * drop_modifier / 100, -100, -1);
                }
            } else {
                sentiment_delta = calc_bound(sentiment_delta, -MAX_SENTIMENT_CHANGE, MAX_SENTIMENT_CHANGE);
            }
            b->sentiment.house_happiness = calc_bound(b->sentiment.house_happiness + sentiment_delta, 0, 100);
            houses_calculated++;

            total_pop += b->house_population;
            total_houses++;
            total_sentiment += b->sentiment.house_happiness * b->house_population;

            int worst_sentiment = 0;
            b->house_sentiment_message = LOW_MOOD_CAUSE_NONE;

            if (b->sentiment.house_happiness < 80) {
                if (b->house_tax_coverage) {
                    worst_sentiment = sentiment_contribution_taxes;
                    b->house_sentiment_message = LOW_MOOD_CAUSE_HIGH_TAXES;
                }
                if (b->subtype.house_level <= HOUSE_GRAND_INSULA) {
                    if (-sentiment_contribution_unemployment < worst_sentiment) {
                        worst_sentiment = -sentiment_contribution_unemployment;
                        b->house_sentiment_message = LOW_MOOD_CAUSE_NO_JOBS;
                    }
                    if (sentiment_contribution_wages < worst_sentiment) {
                        worst_sentiment = sentiment_contribution_wages;
                        b->house_sentiment_message = LOW_MOOD_CAUSE_LOW_WAGES;
                    }
                    if (house_level_sentiment < worst_sentiment) {
                        worst_sentiment = house_level_sentiment;
                        b->house_sentiment_message = LOW_MOOD_CAUSE_SQUALOR;
                    }
                }

                if (worst_sentiment > -15) {
                    if (entertainment_bonus < SENTIMENT_PER_EXTRA_FOOD ||
                        (entertainment_bonus < food_bonus && entertainment_bonus < desirability_bonus)) {
                        b->house_sentiment_message = SUGGEST_MORE_ENT;
                    } else if (desirability_bonus < max_desirability && desirability_bonus < food_bonus) {
                        b->house_sentiment_message = SUGGEST_MORE_DESIRABILITY;
                    } else if (model_get_house(b->subtype.house_level)->food_types > 0 &&
                        food_bonus < MAX_SENTIMENT_FROM_EXTRA_FOOD && b->data.house.num_foods < 3) {
                        b->house_sentiment_message = SUGGEST_MORE_FOOD;
                    } else {
                        b->house_sentiment_message = LOW_MOOD_CAUSE_NONE;
                    }
                }
            }
        }
    }

    if (total_pop) {
        city_data.sentiment.value = calc_bound(total_sentiment / total_pop, 0, 100);
        average_squalor_penalty = average_squalor_penalty / total_houses;
    } else {
        city_data.sentiment.value = default_sentiment;
    }

    int worst_sentiment = 0;
    city_data.sentiment.low_mood_cause = LOW_MOOD_CAUSE_NONE;

    if (-sentiment_contribution_unemployment < worst_sentiment) {
        worst_sentiment = -sentiment_contribution_unemployment;
        city_data.sentiment.low_mood_cause = LOW_MOOD_CAUSE_NO_JOBS;
    }
    if (sentiment_contribution_taxes < worst_sentiment) {
        worst_sentiment = sentiment_contribution_taxes;
        city_data.sentiment.low_mood_cause = LOW_MOOD_CAUSE_HIGH_TAXES;
    }
    if (sentiment_contribution_wages < worst_sentiment) {
        worst_sentiment = sentiment_contribution_wages;
        city_data.sentiment.low_mood_cause = LOW_MOOD_CAUSE_LOW_WAGES;
    }
    if (average_squalor_penalty < worst_sentiment) {
        city_data.sentiment.low_mood_cause = LOW_MOOD_CAUSE_SQUALOR;
    }

    if (city_data.sentiment.message_delay) {
        city_data.sentiment.message_delay--;
    }

    if (city_data.sentiment.value < 48 && city_data.sentiment.value < city_data.sentiment.previous_value) {
        if (city_data.sentiment.message_delay <= 0) {
            city_data.sentiment.message_delay = 3;
            int cause = city_data.sentiment.low_mood_cause;
            if (!cause) {
                cause = -1;
            }
            if (city_data.sentiment.value < 35) {
                city_message_post(0, MESSAGE_PEOPLE_ANGRY, cause, 0);
            } else if (city_data.sentiment.value < 40) {
                city_message_post(0, MESSAGE_PEOPLE_UNHAPPY, cause, 0);
            } else {
                city_message_post(0, MESSAGE_PEOPLE_DISGRUNTLED, cause, 0);
            }
        }
    }

    city_data.sentiment.previous_value = city_data.sentiment.value;
}
