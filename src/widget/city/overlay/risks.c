#include "risks.h"

#include "assets/assets.h"
#include "building/industry.h"
#include "core/config.h"
#include "figure/properties.h"
#include "game/state.h"
#include "graphics/image.h"
#include "map/building.h"
#include "map/bridge.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"
#include "translation/translation.h"
#include "widget/city/draw.h"
#include "widget/city/highway.h"

enum crime_level {
    NO_CRIME = 0,
    MINOR_CRIME = 1,
    LOW_CRIME = 2,
    SOME_CRIME = 3,
    MEDIUM_CRIME = 4,
    LARGE_CRIME = 5,
    RAMPANT_CRIME = 6,
};

static int is_problem_cartpusher(int figure_id)
{
    if (figure_id) {
        figure *fig = figure_get(figure_id);
        return fig->action_state == FIGURE_ACTION_20_CARTPUSHER_INITIAL && fig->min_max_seen;
    } else {
        return 0;
    }
}

static int show_building_fire_crime(const building *b)
{
    return b->type == BUILDING_PREFECTURE || b->type == BUILDING_BURNING_RUIN;
}

static int show_building_damage(const building *b)
{
    return b->type == BUILDING_ENGINEERS_POST || b->type == BUILDING_ARCHITECT_GUILD;
}

static int show_building_problems(const building *b)
{
    b = building_main(b);

    if (b->has_problem) {
        return 1;
    }

    if (b->strike_duration_days > 0) {
        return 1;
    }

    if (b->has_plague || b->state == BUILDING_STATE_MOTHBALLED) {
        return 1;
    }
    
    if (!b->num_workers && building_get_laborers(b->type)) {
        return 1;
    }
    
    if (b->type == BUILDING_FOUNTAIN || b->type == BUILDING_BATHHOUSE ||
         b->type == BUILDING_LARGE_POND || b->type == BUILDING_SMALL_POND) {
        if (!b->has_water_access) {
            return 1;
        }
    } else if (b->type >= BUILDING_WHEAT_FARM && b->type <= BUILDING_CLAY_PIT) {
        if (is_problem_cartpusher(b->figure_id)) {
            return 1;
        }
    } else if (building_is_workshop(b->type)) {
        if (b->type == BUILDING_CONCRETE_MAKER && !b->has_water_access) {
            return 1;
        }
        if (is_problem_cartpusher(b->figure_id) || !building_industry_has_raw_materials_for_production(b)) {
            return 1;
        }
    } else if ((b->type == BUILDING_THEATER || b->type == BUILDING_AMPHITHEATER || b->type == BUILDING_ARENA ||
        b->type == BUILDING_COLOSSEUM || b->type == BUILDING_HIPPODROME)) {
        if (!b->data.entertainment.days1) {
            return 1;
        }
    } else if (b->type == BUILDING_ARENA || b->type == BUILDING_COLOSSEUM) {
        if (!b->data.entertainment.days2) {
            return 1;
        }

    } else if (b->type == BUILDING_DEPOT &&
        (!b->data.depot.current_order.src_storage_id ||
         !b->data.depot.current_order.dst_storage_id)) {
        return 1;

    } else if (b->has_road_access == 0 &&
        building_get_laborers(b->type) && b->type != BUILDING_LATRINES && b->type != BUILDING_FOUNTAIN) {
        return 1;
    }

    return 0;
}

static int show_building_native(const building *b)
{
    return b->type == BUILDING_NATIVE_HUT || b->type == BUILDING_NATIVE_HUT_ALT ||
        b->type == BUILDING_NATIVE_MEETING || b->type == BUILDING_MISSION_POST || b->type == BUILDING_NATIVE_CROPS ||
        b->type == BUILDING_NATIVE_DECORATION || b->type == BUILDING_NATIVE_MONUMENT ||
        b->type == BUILDING_NATIVE_WATCHTOWER;
}

static int show_building_enemy(const building *b)
{
    return b->type == BUILDING_PREFECTURE
        || b->type == BUILDING_WATCHTOWER || b->type == BUILDING_TOWER || b->type == BUILDING_WALL
        || (building_is_fort(b->type)) || b->type == BUILDING_FORT_GROUND
        || b->type == BUILDING_BARRACKS || b->type == BUILDING_MILITARY_ACADEMY
        || b->type == BUILDING_GATEHOUSE || b->type == BUILDING_PALISADE_GATE || b->type == BUILDING_PALISADE;
}

static int show_figure_fire(const figure *f)
{
    return f->type == FIGURE_PREFECT;
}

static int show_figure_damage(const figure *f)
{
    return f->type == FIGURE_ENGINEER || f->type == FIGURE_WORK_CAMP_ARCHITECT;
}

static int show_figure_crime(const figure *f)
{
    const figure_properties *props = figure_properties_for_type(f->type);
    return props->category & FIGURE_CATEGORY_ARMED || props->category & FIGURE_CATEGORY_CRIMINAL
        || props->category & FIGURE_CATEGORY_PROJECTILE
        || f->type == FIGURE_FORT_STANDARD;
}

static int show_figure_problems(const figure *f)
{
    if (f->type == FIGURE_LABOR_SEEKER) {
        building *b = building_get(f->building_id);
        return (!b->num_workers && building_get_laborers(b->type)) || b->has_problem;
    } else if (f->type == FIGURE_CART_PUSHER) {
        return f->action_state == FIGURE_ACTION_20_CARTPUSHER_INITIAL || f->min_max_seen;
    } else if (f->type == FIGURE_PROTESTER || f->type == FIGURE_BEGGAR) {
        return 1;
    } else {
        return 0;
    }
}

static int show_figure_native(const figure *f)
{
    return f->type == FIGURE_INDIGENOUS_NATIVE || f->type == FIGURE_MISSIONARY ||
        f->type == FIGURE_NATIVE_TRADER;
}

static int show_figure_enemy(const figure *f)
{
    const figure_properties *props = figure_properties_for_type(f->type);
    return props->category & FIGURE_CATEGORY_HOSTILE || props->category & FIGURE_CATEGORY_AGGRESSIVE_ANIMAL
        || props->category & FIGURE_CATEGORY_ARMED || props->category & FIGURE_CATEGORY_PROJECTILE
        || f->type == FIGURE_FORT_STANDARD;
}

static int get_column_height_fire(const building *b)
{
    return b->fire_risk > 0 ? b->fire_risk / 10 : NO_COLUMN;
}

static int get_column_height_damage(const building *b)
{
    return b->damage_risk > 0 ? b->damage_risk / 20 : NO_COLUMN;
}

static int get_crime_level(const building *b)
{
    if (b->house_size) {
        int happiness = b->sentiment.house_happiness;
        if (happiness <= 0) {
            return RAMPANT_CRIME;
        } else if (happiness <= 10 || b->house_criminal_active) {
            return LARGE_CRIME;
        } else if (happiness <= 20) {
            return MEDIUM_CRIME;
        } else if (happiness <= 30) {
            return SOME_CRIME;
        } else if (happiness <= 40) {
            return LOW_CRIME;
        } else if (happiness < 50) {
            return MINOR_CRIME;
        }
    }
    return 0;

}

static int get_column_height_crime(const building *b)
{
    if (b->house_size) {
        int crime = get_crime_level(b);
        if (crime == RAMPANT_CRIME) {
            return 10;
        } else if (crime == LARGE_CRIME) {
            return 8;
        } else if (crime == MEDIUM_CRIME) {
            return 6;
        } else if (crime == SOME_CRIME) {
            return 4;
        } else if (crime == LOW_CRIME) {
            return 2;
        } else if (crime == MINOR_CRIME) {
            return 1;
        }
    }
    return NO_COLUMN;
}

static int get_tooltip_fire(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    if (b->fire_risk <= 0) {
        return 46;
    } else if (b->fire_risk <= 20) {
        return 47;
    } else if (b->fire_risk <= 40) {
        return 48;
    } else if (b->fire_risk <= 60) {
        return 49;
    } else if (b->fire_risk <= 80) {
        return 50;
    } else {
        return 51;
    }
}

static int get_tooltip_damage(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    if (b->damage_risk <= 0) {
        return 52;
    } else if (b->damage_risk <= 40) {
        return 53;
    } else if (b->damage_risk <= 80) {
        return 54;
    } else if (b->damage_risk <= 120) {
        return 55;
    } else if (b->damage_risk <= 160) {
        return 56;
    } else {
        return 57;
    }
}

static int get_tooltip_crime(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    int crime = get_crime_level(b);
    if (crime == RAMPANT_CRIME) {
        return 63;
    } else if (crime == LARGE_CRIME) {
        return 62;
    } else if (crime == MEDIUM_CRIME) {
        return 61;
    } else if (crime == SOME_CRIME) {
        return 60;
    } else if (crime == LOW_CRIME || crime == MINOR_CRIME) {
        return 59;
    } else {
        return 58;
    }
}

static int get_tooltip_problems(tooltip_context *c, int grid_offset)
{
    const building *b = building_get(map_building_at(grid_offset));
    const building *main_building = b;

    int guard = 0;
    while (guard < 9) {
        if (main_building->prev_part_building_id <= 0) {
            break;
        }
        main_building = building_get(main_building->prev_part_building_id);
        guard++;
    }
    if (guard < 9) {
        b = main_building;
    }
    if (b->has_plague) {
        c->translation_key = TR_TOOLTIP_OVERLAY_PROBLEMS_PLAGUE;
    }
    if (b->strike_duration_days > 0) {
        c->translation_key = TR_TOOLTIP_OVERLAY_PROBLEMS_STRIKE;
    } else if (b->state == BUILDING_STATE_MOTHBALLED) {
        c->translation_key = TR_TOOLTIP_OVERLAY_PROBLEMS_MOTHBALLED;
    } else if (!b->num_workers && building_get_laborers(b->type)) {
        c->translation_key = TR_TOOLTIP_OVERLAY_PROBLEMS_NO_LABOR;
    } else if (b->type == BUILDING_FOUNTAIN || b->type == BUILDING_BATHHOUSE
        || b->type == BUILDING_LARGE_POND || b->type == BUILDING_SMALL_POND) {
        if (!b->has_water_access) {
            c->translation_key = TR_TOOLTIP_OVERLAY_PROBLEMS_NO_WATER_ACCESS;
        }
    } else if (b->type >= BUILDING_WHEAT_FARM && b->type <= BUILDING_CLAY_PIT) {
        if (is_problem_cartpusher(b->figure_id)) {
            c->translation_key = TR_TOOLTIP_OVERLAY_PROBLEMS_CARTPUSHER;
        }
    } else if (building_is_workshop(b->type)) {
        if (b->type == BUILDING_CONCRETE_MAKER && !b->has_water_access) {
            c->translation_key = TR_TOOLTIP_OVERLAY_PROBLEMS_NO_WATER_ACCESS;
        }
        if (is_problem_cartpusher(b->figure_id)) {
            c->translation_key = TR_TOOLTIP_OVERLAY_PROBLEMS_CARTPUSHER;
        } else if (!building_industry_has_raw_materials_for_production(b)) {
            c->translation_key = TR_TOOLTIP_OVERLAY_PROBLEMS_NO_RESOURCES;
        }
    } else if (b->type == BUILDING_THEATER && !b->data.entertainment.days1) {
        c->text_group = 72;
        return 5;
    } else if (b->type == BUILDING_AMPHITHEATER) {
        if (!b->data.entertainment.days1) {
            c->text_group = 71;
            return 7;
        } else if (!b->data.entertainment.days2) {
            c->text_group = 71;
            return 9;
        }
    } else if (b->type == BUILDING_ARENA || b->type == BUILDING_COLOSSEUM) {
        if (!b->data.entertainment.days1) {
            c->text_group = 74;
            return 7;
        } else if (!b->data.entertainment.days2) {
            c->text_group = 74;
            return 9;
        }
    } else if (b->type == BUILDING_HIPPODROME && !b->data.entertainment.days1) {
        c->text_group = 73;
        return 5;

    } else if (b->type == BUILDING_DEPOT &&
        (!b->data.depot.current_order.src_storage_id ||
            !b->data.depot.current_order.dst_storage_id)) {
        c->translation_key = TR_TOOLTIP_OVERLAY_PROBLEMS_DEPOT_NO_INSTRUCTIONS;

    } else if (b->has_road_access == 0 &&
        building_get_laborers(b->type) && b->type != BUILDING_LATRINES && b->type != BUILDING_FOUNTAIN) {
        c->translation_key = TR_TOOLTIP_OVERLAY_PROBLEMS_NO_ROAD_ACCESS;
    }
    if (c->translation_key) {
        return 1;
    }
    return 0;
}

const city_overlay *city_overlay_for_fire(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_FIRE,
        .column_type = COLUMN_COLOR_RED_TO_GREEN,
        .show_building = show_building_fire_crime,
        .show_figure = show_figure_fire,
        .get_column_height = get_column_height_fire,
        .get_tooltip = get_tooltip_fire
    };
    return &overlay;
}

const city_overlay *city_overlay_for_damage(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_DAMAGE,
        .column_type = COLUMN_COLOR_RED_TO_GREEN,
        .show_building = show_building_damage,
        .show_figure = show_figure_damage,
        .get_column_height = get_column_height_damage,
        .get_tooltip = get_tooltip_damage
    };
    return &overlay;
}

const city_overlay *city_overlay_for_crime(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_CRIME,
        .column_type = COLUMN_COLOR_RED_TO_GREEN,
        .show_building = show_building_fire_crime,
        .show_figure = show_figure_crime,
        .get_column_height = get_column_height_crime,
        .get_tooltip = get_tooltip_crime
    };
    return &overlay;
}

const city_overlay *city_overlay_for_problems(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_PROBLEMS,
        .show_building = show_building_problems,
        .show_figure = show_figure_problems,
        .get_tooltip = get_tooltip_problems
    };
    return &overlay;
}

static void draw_graph_native(int x, int y, float scale, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));

    if (map_property_is_native_land(grid_offset) && !show_building_native(b)) {
        image_draw_isometric_footprint_from_draw_tile(image_group(GROUP_TERRAIN_DESIRABILITY) + 1, x, y,
            ALPHA_FONT_SEMI_TRANSPARENT, scale);
    }

    if (show_building_native(b) && map_property_is_draw_tile(grid_offset)) {
        int image_id = map_image_at(grid_offset);
        image_draw_isometric_top_from_draw_tile(image_id, x, y, city_draw_get_color_mask(grid_offset, 1), scale);
    }
}

const city_overlay *city_overlay_for_native(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_NATIVE,
        .show_building = show_building_native,
        .show_figure = show_figure_native,
        .draw_layer = draw_graph_native
    };
    return &overlay;
}

const city_overlay *city_overlay_for_enemy(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_ENEMY,
        .show_building = show_building_enemy,
        .show_figure = show_figure_enemy
    };
    return &overlay;
}
