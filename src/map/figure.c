#include "figure.h"

#include "figure/figure.h"
#include "map/grid.h"

static grid_u16 figures;

static struct {
    figure_category category;
    int count;
} data;

int map_has_figure_at(int grid_offset)
{
    return map_grid_is_valid_offset(grid_offset) && figures.items[grid_offset] > 0;
}

unsigned int map_figure_at(int grid_offset)
{
    return map_grid_is_valid_offset(grid_offset) ? figures.items[grid_offset] : 0;
}

static int has_category(figure *f)
{
    return figure_is_category(f, data.category);
}

static void count_category(figure *f)
{
    if (figure_is_category(f, data.category)) {
        data.count++;
    }
}

static void kill_category(figure *f)
{
    if (figure_is_category(f, data.category)) {
        figure_delete(f);
    }
}

int map_has_figure_category_at(int grid_offset, figure_category category)
{
    data.category = category;
    int result = map_figure_foreach_until(grid_offset, has_category);
    return result;
}

int map_has_figure_category_in_area(int minx, int miny, int maxx, int maxy, figure_category category)
{
    int grid_offset;
    for (int yy = miny; yy <= maxy; yy++) {
        for (int xx = minx; xx <= maxx; xx++) {
            grid_offset = map_grid_offset(minx, miny);
            if (map_has_figure_category_at(grid_offset, category)) {
                return 1;
            }
        }
    }
    return 0;
}

int map_count_figures_category_at(int grid_offset, figure_category category)
{
    data.count = 0;
    data.category = category;
    map_figure_foreach(grid_offset, count_category);
    return data.count;
}

int map_count_figures_category_in_area(int minx, int miny, int maxx, int maxy, figure_category category)
{
    int count = 0;
    int grid_offset;
    for (int yy = miny; yy <= maxy; yy++) {
        for (int xx = minx; xx <= maxx; xx++) {
            grid_offset = map_grid_offset(minx, miny);
            count += map_count_figures_category_at(grid_offset, category);
        }
    }
    return count;
}

void map_kill_figures_category_at(int grid_offset, figure_category category)
{
    data.category = category;
    map_figure_foreach(grid_offset, kill_category);
}

void map_kill_figures_category_in_area(int minx, int miny, int maxx, int maxy, figure_category category)
{
    int grid_offset;
    for (int yy = miny; yy <= maxy; yy++) {
        for (int xx = minx; xx <= maxx; xx++) {
            grid_offset = map_grid_offset(minx, miny);
            map_kill_figures_category_at(grid_offset, category);
        }
    }
}

static void cap_figures_on_same_tile_index(figure *f)
{
    if (f->figures_on_same_tile_index > 20) {
        f->figures_on_same_tile_index = 20;
    }
}

void map_figure_add(figure *f)
{
    if (!map_grid_is_valid_offset(f->grid_offset)) {
        return;
    }
    f->figures_on_same_tile_index = 0;
    f->next_figure_id_on_same_tile = 0;

    if (figures.items[f->grid_offset]) {
        figure *next = figure_get(figures.items[f->grid_offset]);
        f->figures_on_same_tile_index++;
        while (next->next_figure_id_on_same_tile) {
            next = figure_get(next->next_figure_id_on_same_tile);
            f->figures_on_same_tile_index++;
        }
        cap_figures_on_same_tile_index(f);
        next->next_figure_id_on_same_tile = f->id;
    } else {
        figures.items[f->grid_offset] = f->id;
    }
}

void map_figure_update(figure *f)
{
    if (!map_grid_is_valid_offset(f->grid_offset)) {
        return;
    }
    f->figures_on_same_tile_index = 0;

    figure *next = figure_get(figures.items[f->grid_offset]);
    while (next->id) {
        if (next->id == f->id) {
            cap_figures_on_same_tile_index(f);
            return;
        }
        f->figures_on_same_tile_index++;
        next = figure_get(next->next_figure_id_on_same_tile);
    }
    cap_figures_on_same_tile_index(f);
}

void map_figure_delete(figure *f)
{
    if (!map_grid_is_valid_offset(f->grid_offset) || !figures.items[f->grid_offset]) {
        f->next_figure_id_on_same_tile = 0;
        return;
    }
    if (figures.items[f->grid_offset] == f->id) {
        figures.items[f->grid_offset] = f->next_figure_id_on_same_tile;
    } else {
        figure *prev = figure_get(figures.items[f->grid_offset]);
        while (prev->id && (unsigned int) prev->next_figure_id_on_same_tile != f->id) {
            prev = figure_get(prev->next_figure_id_on_same_tile);
        }
        prev->next_figure_id_on_same_tile = f->next_figure_id_on_same_tile;
    }
    f->next_figure_id_on_same_tile = 0;
}

int map_figure_foreach_until(int grid_offset, int (*callback)(figure *f))
{
    if (figures.items[grid_offset] > 0) {
        int figure_id = figures.items[grid_offset];
        while (figure_id) {
            figure *f = figure_get(figure_id);
            int result = callback(f);
            if (result) {
                return result;
            }
            figure_id = f->next_figure_id_on_same_tile;
        }
    }
    return 0;
}

void map_figure_foreach(int grid_offset, void (*callback)(figure *f))
{
    if (figures.items[grid_offset] > 0) {
        int figure_id = figures.items[grid_offset];
        while (figure_id) {
            figure *f = figure_get(figure_id);
            callback(f);
            figure_id = f->next_figure_id_on_same_tile;
        }
    }
}

void map_figure_clear(void)
{
    map_grid_clear_u16(figures.items);
}

void map_figure_save_state(buffer *buf)
{
    map_grid_save_state_u16(figures.items, buf);
}

void map_figure_load_state(buffer *buf)
{
    map_grid_load_state_u16(figures.items, buf);
}
