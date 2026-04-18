#include "overlay.h"

#include "game/state.h"
#include "map/building.h"
#include "widget/city/overlay/education.h"
#include "widget/city/overlay/entertainment.h"
#include "widget/city/overlay/health.h"
#include "widget/city/overlay/housing.h"
#include "widget/city/overlay/other.h"
#include "widget/city/overlay/risks.h"

static const city_overlay no_overlay = { .type = OVERLAY_NONE };

const city_overlay *city_overlay_get(void)
{
    switch (game_state_overlay()) {
        case OVERLAY_FIRE:
            return city_overlay_for_fire();
        case OVERLAY_CRIME:
            return city_overlay_for_crime();
        case OVERLAY_DAMAGE:
            return city_overlay_for_damage();
        case OVERLAY_PROBLEMS:
            return city_overlay_for_problems();
        case OVERLAY_NATIVE:
            return city_overlay_for_native();
        case OVERLAY_ENTERTAINMENT:
            return city_overlay_for_entertainment();
        case OVERLAY_THEATER:
            return city_overlay_for_theater();
        case OVERLAY_AMPHITHEATER:
            return city_overlay_for_amphitheater();
        case OVERLAY_ARENA:
            return city_overlay_for_arena();
        case OVERLAY_COLOSSEUM:
            return city_overlay_for_colosseum();
        case OVERLAY_HIPPODROME:
            return city_overlay_for_hippodrome();
        case OVERLAY_TAVERN:
            return city_overlay_for_tavern();
        case OVERLAY_EDUCATION:
            return city_overlay_for_education();
        case OVERLAY_SCHOOL:
            return city_overlay_for_school();
        case OVERLAY_LIBRARY:
            return city_overlay_for_library();
        case OVERLAY_ACADEMY:
            return city_overlay_for_academy();
        case OVERLAY_HEALTH:
            return city_overlay_for_health();
        case OVERLAY_BARBER:
            return city_overlay_for_barber();
        case OVERLAY_BATHHOUSE:
            return city_overlay_for_bathhouse();
        case OVERLAY_CLINIC:
            return city_overlay_for_clinic();
        case OVERLAY_HOSPITAL:
            return city_overlay_for_hospital();
        case OVERLAY_SICKNESS:
            return city_overlay_for_sickness();
        case OVERLAY_RELIGION:
            return city_overlay_for_religion();
        case OVERLAY_TAX_INCOME:
            return city_overlay_for_tax_income();
        case OVERLAY_EFFICIENCY:
            return city_overlay_for_efficiency();
        case OVERLAY_FOOD_STOCKS:
            return city_overlay_for_food_stocks();
        case OVERLAY_WATER:
            return city_overlay_for_water();
        case OVERLAY_SENTIMENT:
            return city_overlay_for_sentiment();
        case OVERLAY_DESIRABILITY:
            return city_overlay_for_desirability();
        case OVERLAY_ROADS:
            return city_overlay_for_roads();
        case OVERLAY_LEVY:
            return city_overlay_for_levy();
        case OVERLAY_EMPLOYMENT:
            return city_overlay_for_employment();
        case OVERLAY_MOTHBALL:
            return city_overlay_for_mothball();
        case OVERLAY_ENEMY:
            return city_overlay_for_enemy();
        case OVERLAY_LOGISTICS:
            return city_overlay_for_logistics();
        case OVERLAY_STORAGES:
            return city_overlay_for_storages();
        case OVERLAY_HOUSE_SMALL_TENT:
            return city_overlay_for_small_tent();
        case OVERLAY_HOUSE_LARGE_TENT:
            return city_overlay_for_large_tent();
        case OVERLAY_HOUSE_SMALL_SHACK:
            return city_overlay_for_small_shack();
        case OVERLAY_HOUSE_LARGE_SHACK:
            return city_overlay_for_large_shack();
        case OVERLAY_HOUSE_SMALL_HOVEL:
            return city_overlay_for_small_hovel();
        case OVERLAY_HOUSE_LARGE_HOVEL:
            return city_overlay_for_large_hovel();
        case OVERLAY_HOUSE_SMALL_CASA:
            return city_overlay_for_small_casa();
        case OVERLAY_HOUSE_LARGE_CASA:
            return city_overlay_for_large_casa();
        case OVERLAY_HOUSE_SMALL_INSULA:
            return city_overlay_for_small_insula();
        case OVERLAY_HOUSE_MEDIUM_INSULA:
            return city_overlay_for_medium_insula();
        case OVERLAY_HOUSE_LARGE_INSULA:
            return city_overlay_for_large_insula();
        case OVERLAY_HOUSE_GRAND_INSULA:
            return city_overlay_for_grand_insula();
        case OVERLAY_HOUSE_SMALL_VILLA:
            return city_overlay_for_small_villa();
        case OVERLAY_HOUSE_MEDIUM_VILLA:
            return city_overlay_for_medium_villa();
        case OVERLAY_HOUSE_LARGE_VILLA:
            return city_overlay_for_large_villa();
        case OVERLAY_HOUSE_GRAND_VILLA:
            return city_overlay_for_grand_villa();
        case OVERLAY_HOUSE_SMALL_PALACE:
            return city_overlay_for_small_palace();
        case OVERLAY_HOUSE_MEDIUM_PALACE:
            return city_overlay_for_medium_palace();
        case OVERLAY_HOUSE_LARGE_PALACE:
            return city_overlay_for_large_palace();
        case OVERLAY_HOUSE_LUXURY_PALACE:
            return city_overlay_for_luxury_palace();
        case OVERLAY_HOUSING_GROUPS_TENTS:
            return city_overlay_for_housing_groups_tents();
        case OVERLAY_HOUSING_GROUPS_SHACKS:
            return city_overlay_for_housing_groups_shacks();
        case OVERLAY_HOUSING_GROUPS_HOVELS:
            return city_overlay_for_housing_groups_hovels();
        case OVERLAY_HOUSING_GROUPS_CASAE:
            return city_overlay_for_housing_groups_casae();
        case OVERLAY_HOUSING_GROUPS_INSULAE:
            return city_overlay_for_housing_groups_insulae();
        case OVERLAY_HOUSING_GROUPS_VILLAS:
            return city_overlay_for_housing_groups_villas();
        case OVERLAY_HOUSING_GROUPS_PALACES:
            return city_overlay_for_housing_groups_palaces();
        default:
            return &no_overlay;
    }
}

int city_overlay_get_tooltip_text(tooltip_context *c, int grid_offset)
{
    const city_overlay *overlay = city_overlay_get();
    if (overlay->get_tooltip) {
        return overlay->get_tooltip(c, grid_offset);
    }
    return 0;
}
