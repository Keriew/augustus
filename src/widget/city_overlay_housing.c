#include "city_overlay_housing.h"

#include "assets/assets.h"
#include "building/animation.h"
#include "building/building.h"
#include "building/industry.h"
#include "building/model.h"
#include "building/monument.h"
#include "building/roadblock.h"
#include "building/rotation.h"
#include "city/constants.h"
#include "city/finance.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/lang.h"
#include "core/string.h"
#include "game/resource.h"
#include "game/state.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/text.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/desirability.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"
#include "scenario/property.h"
#include "translation/translation.h"
#include "widget/city_draw_highway.h"

#include <stdio.h>

static int show_figure_none(const figure *f)
{
    return 0;
}

static int get_column_height_none(const building *b)
{
    return NO_COLUMN;
}

static int show_building_small_tent(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_TENT;
}

const city_overlay *city_overlay_for_small_tent(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_SMALL_TENT, COLUMN_COLOR_GREEN, show_building_small_tent,
    show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_large_tent(const building *b)
{
    return b->type == BUILDING_HOUSE_LARGE_TENT;
}

const city_overlay *city_overlay_for_large_tent(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_LARGE_TENT, COLUMN_COLOR_GREEN, show_building_large_tent,
    show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_small_shack(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_SHACK;
}

const city_overlay *city_overlay_for_small_shack(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_SMALL_SHACK, COLUMN_COLOR_GREEN, show_building_small_shack,
    show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_large_shack(const building *b)
{
    return b->type == BUILDING_HOUSE_LARGE_SHACK;
}

const city_overlay *city_overlay_for_large_shack(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_LARGE_SHACK, COLUMN_COLOR_GREEN, show_building_large_shack,
    show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_small_hovel(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_HOVEL;
}

const city_overlay *city_overlay_for_small_hovel(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_SMALL_HOVEL, COLUMN_COLOR_GREEN, show_building_small_hovel,
    show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_large_hovel(const building *b)
{
    return b->type == BUILDING_HOUSE_LARGE_HOVEL;
}

const city_overlay *city_overlay_for_large_hovel(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_LARGE_HOVEL, COLUMN_COLOR_GREEN, show_building_large_hovel,
    show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_small_casa(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_CASA;
}

const city_overlay *city_overlay_for_small_casa(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_SMALL_CASA, COLUMN_COLOR_GREEN, show_building_small_casa,
    show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_large_casa(const building *b)
{
    return b->type == BUILDING_HOUSE_LARGE_CASA;
}

const city_overlay *city_overlay_for_large_casa(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_LARGE_CASA, COLUMN_COLOR_GREEN, show_building_large_casa,
    show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_small_insula(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_INSULA;
}

const city_overlay *city_overlay_for_small_insula(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_SMALL_INSULA, COLUMN_COLOR_GREEN, show_building_small_insula,
    show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_medium_insula(const building *b)
{
    return b->type == BUILDING_HOUSE_MEDIUM_INSULA;
}

const city_overlay *city_overlay_for_medium_insula(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_MEDIUM_INSULA, COLUMN_COLOR_GREEN, show_building_medium_insula,
        show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_large_insula(const building *b)
{
    return b->type == BUILDING_HOUSE_LARGE_INSULA;
}

const city_overlay *city_overlay_for_large_insula(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_LARGE_INSULA, COLUMN_COLOR_GREEN, show_building_large_insula,
        show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_grand_insula(const building *b)
{
    return b->type == BUILDING_HOUSE_GRAND_INSULA;
}

const city_overlay *city_overlay_for_grand_insula(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_GRAND_INSULA, COLUMN_COLOR_GREEN, show_building_grand_insula,
        show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_small_villa(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_VILLA;
}

const city_overlay *city_overlay_for_small_villa(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_SMALL_VILLA, COLUMN_COLOR_GREEN, show_building_small_villa,
        show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_medium_villa(const building *b)
{
    return b->type == BUILDING_HOUSE_MEDIUM_VILLA;
}

const city_overlay *city_overlay_for_medium_villa(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_MEDIUM_VILLA, COLUMN_COLOR_GREEN, show_building_medium_villa,
        show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_large_villa(const building *b)
{
    return b->type == BUILDING_HOUSE_LARGE_VILLA;
}

const city_overlay *city_overlay_for_large_villa(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_LARGE_VILLA, COLUMN_COLOR_GREEN, show_building_large_villa,
        show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_grand_villa(const building *b)
{
    return b->type == BUILDING_HOUSE_GRAND_VILLA;
}

const city_overlay *city_overlay_for_grand_villa(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_GRAND_VILLA, COLUMN_COLOR_GREEN, show_building_grand_villa,
        show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_small_palace(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_PALACE;
}

const city_overlay *city_overlay_for_small_palace(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_SMALL_PALACE, COLUMN_COLOR_GREEN, show_building_small_palace,
        show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_medium_palace(const building *b)
{
    return b->type == BUILDING_HOUSE_MEDIUM_PALACE;
}

const city_overlay *city_overlay_for_medium_palace(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_MEDIUM_PALACE, COLUMN_COLOR_GREEN, show_building_medium_palace,
        show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_large_palace(const building *b)
{
    return b->type == BUILDING_HOUSE_LARGE_PALACE;
}

const city_overlay *city_overlay_for_large_palace(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_LARGE_PALACE, COLUMN_COLOR_GREEN, show_building_large_palace,
        show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_luxury_palace(const building *b)
{
    return b->type == BUILDING_HOUSE_LUXURY_PALACE;
}

const city_overlay *city_overlay_for_luxury_palace(void)
{
    static city_overlay overlay = { OVERLAY_HOUSE_LUXURY_PALACE, COLUMN_COLOR_GREEN, show_building_luxury_palace,
        show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_housing_groups_tents(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_TENT || b->type == BUILDING_HOUSE_LARGE_TENT;
}

const city_overlay *city_overlay_for_housing_groups_tents(void)
{
    static city_overlay overlay = { OVERLAY_HOUSING_GROUPS_TENTS, COLUMN_COLOR_GREEN,
        show_building_housing_groups_tents, show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_housing_groups_shacks(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_SHACK || b->type == BUILDING_HOUSE_LARGE_SHACK;
}

const city_overlay *city_overlay_for_housing_groups_shacks(void)
{
    static city_overlay overlay = { OVERLAY_HOUSING_GROUPS_SHACKS, COLUMN_COLOR_GREEN,
        show_building_housing_groups_shacks, show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_housing_groups_hovels(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_HOVEL || b->type == BUILDING_HOUSE_LARGE_HOVEL;
}

const city_overlay *city_overlay_for_housing_groups_hovels(void)
{
    static city_overlay overlay = { OVERLAY_HOUSING_GROUPS_HOVELS, COLUMN_COLOR_GREEN,
        show_building_housing_groups_hovels, show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_housing_groups_casae(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_CASA || b->type == BUILDING_HOUSE_LARGE_CASA;
}

const city_overlay *city_overlay_for_housing_groups_casae(void)
{
    static city_overlay overlay = { OVERLAY_HOUSING_GROUPS_CASAE, COLUMN_COLOR_GREEN,
        show_building_housing_groups_casae, show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_housing_groups_insulae(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_INSULA || b->type == BUILDING_HOUSE_MEDIUM_INSULA ||
        b->type == BUILDING_HOUSE_LARGE_INSULA || b->type == BUILDING_HOUSE_GRAND_INSULA;
}

const city_overlay *city_overlay_for_housing_groups_insulae(void)
{
    static city_overlay overlay = { OVERLAY_HOUSING_GROUPS_INSULAE, COLUMN_COLOR_GREEN,
        show_building_housing_groups_insulae, show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_housing_groups_villas(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_VILLA || b->type == BUILDING_HOUSE_MEDIUM_VILLA ||
        b->type == BUILDING_HOUSE_LARGE_VILLA || b->type == BUILDING_HOUSE_GRAND_VILLA;
}

const city_overlay *city_overlay_for_housing_groups_villas(void)
{
    static city_overlay overlay = { OVERLAY_HOUSING_GROUPS_VILLAS, COLUMN_COLOR_GREEN,
        show_building_housing_groups_villas, show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}

static int show_building_housing_groups_palaces(const building *b)
{
    return b->type == BUILDING_HOUSE_SMALL_PALACE || b->type == BUILDING_HOUSE_MEDIUM_PALACE ||
        b->type == BUILDING_HOUSE_LARGE_PALACE || b->type == BUILDING_HOUSE_LUXURY_PALACE;
}

const city_overlay *city_overlay_for_housing_groups_palaces(void)
{
    static city_overlay overlay = { OVERLAY_HOUSING_GROUPS_PALACES, COLUMN_COLOR_GREEN,
        show_building_housing_groups_palaces, show_figure_none, get_column_height_none, 0, 0, 0, 0, 0 };
    return &overlay;
}