#include "education.h"

#include "game/state.h"
#include "map/building.h"

static int show_building_education(const building *b)
{
    return b->type == BUILDING_SCHOOL || b->type == BUILDING_LIBRARY || b->type == BUILDING_ACADEMY;
}

static int show_building_school(const building *b)
{
    return b->type == BUILDING_SCHOOL;
}

static int show_building_library(const building *b)
{
    return b->type == BUILDING_LIBRARY;
}

static int show_building_academy(const building *b)
{
    return b->type == BUILDING_ACADEMY;
}

static int show_figure_education(const figure *f)
{
    return f->type == FIGURE_SCHOOL_CHILD || f->type == FIGURE_LIBRARIAN || f->type == FIGURE_TEACHER;
}

static int show_figure_school(const figure *f)
{
    return f->type == FIGURE_SCHOOL_CHILD;
}

static int show_figure_library(const figure *f)
{
    return f->type == FIGURE_LIBRARIAN;
}

static int show_figure_academy(const figure *f)
{
    return f->type == FIGURE_TEACHER;
}

static int get_column_height_education(const building *b)
{
    return b->house_size && b->data.house.education ? b->data.house.education * 3 - 1 : NO_COLUMN;
}

static int get_column_height_school(const building *b)
{
    return b->house_size && b->data.house.school ? b->data.house.school / 10 : NO_COLUMN;
}

static int get_column_height_library(const building *b)
{
    return b->house_size && b->data.house.library ? b->data.house.library / 10 : NO_COLUMN;
}

static int get_column_height_academy(const building *b)
{
    return b->house_size && b->data.house.academy ? b->data.house.academy / 10 : NO_COLUMN;
}

static int get_tooltip_education(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    if (!b->house_size) {
        return 0;
    }
    switch (b->data.house.education) {
        case 0: return 100;
        case 1: return 101;
        case 2: return 102;
        case 3: return 103;
        default: return 0;
    }
}

static int get_tooltip_school(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    if (!b->house_size) {
        return 0;
    }
    if (b->data.house.school <= 0) {
        return 19;
    } else if (b->data.house.school >= 80) {
        return 20;
    } else if (b->data.house.school >= 20) {
        return 21;
    } else {
        return 22;
    }
}

static int get_tooltip_library(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    if (!b->house_size) {
        return 0;
    }
    if (b->data.house.library <= 0) {
        return 23;
    } else if (b->data.house.library >= 80) {
        return 24;
    } else if (b->data.house.library >= 20) {
        return 25;
    } else {
        return 26;
    }
}

static int get_tooltip_academy(tooltip_context *c, int grid_offset)
{
    building *b = building_get(map_building_at(grid_offset));
    if (!b->house_size) {
        return 0;
    }
    if (b->data.house.academy <= 0) {
        return 27;
    } else if (b->data.house.academy >= 80) {
        return 28;
    } else if (b->data.house.academy >= 20) {
        return 29;
    } else {
        return 30;
    }
}

const city_overlay *city_overlay_for_education(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_EDUCATION,
        .column_type = COLUMN_COLOR_GREEN,
        .show_building = show_building_education,
        .show_figure = show_figure_education,
        .get_column_height = get_column_height_education,
        .get_tooltip = get_tooltip_education
    };
    return &overlay;
}

const city_overlay *city_overlay_for_school(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_SCHOOL,
        .column_type = COLUMN_COLOR_GREEN_TO_RED,
        .show_building = show_building_school,
        .show_figure = show_figure_school,
        .get_column_height = get_column_height_school,
        .get_tooltip = get_tooltip_school
    };
    return &overlay;
}

const city_overlay *city_overlay_for_library(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_LIBRARY,
        .column_type = COLUMN_COLOR_GREEN_TO_RED,
        .show_building = show_building_library,
        .show_figure = show_figure_library,
        .get_column_height = get_column_height_library,
        .get_tooltip = get_tooltip_library
    };
    return &overlay;
}

const city_overlay *city_overlay_for_academy(void)
{
    static city_overlay overlay = {
        .type = OVERLAY_ACADEMY,
        .column_type = COLUMN_COLOR_GREEN_TO_RED,
        .show_building = show_building_academy,
        .show_figure = show_figure_academy,
        .get_column_height = get_column_height_academy,
        .get_tooltip = get_tooltip_academy
    };
    return &overlay;
}
