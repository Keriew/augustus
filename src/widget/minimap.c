#include "minimap.h"

#include "assets/assets.h"
#include "building/building.h"
#include "building/monument.h"
#include "city/view.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/renderer.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"
#include "scenario/property.h"

#include <stdlib.h>
#include <string.h>

enum {
    FIGURE_COLOR_NONE = 0,
    FIGURE_COLOR_SOLDIER = 1,
    FIGURE_COLOR_SELECTED_SOLDIER = 2,
    FIGURE_COLOR_ENEMY = 3,
    FIGURE_COLOR_WOLF = 4
};

enum {
    REFRESH_NOT_NEEDED = 0,
    REFRESH_FULL = 1,
    REFRESH_CAMERA_MOVED = 2
};

typedef struct {
    color_t left;
    color_t right;
} tile_color;

typedef struct {
    scenario_climate(*climate)(void);
    building *(*building)(int id);
    struct {
        int (*width)(void);
        int (*height)(void);
    } map;
    struct {
        int (*figure)(int grid_offset, int (*callback)(figure *f));
        int (*terrain)(int grid_offset);
        int (*building_id)(int grid_offset);
        int (*is_draw_tile)(int grid_offset);
        int (*tile_size)(int grid_offset);
        int (*random)(int grid_offset);
    } offset;
} minimap_functions;

static minimap_functions functions = {
    .climate = scenario_property_climate,
    .map.width = map_grid_width,
    .map.height = map_grid_height,
    .offset.figure = map_figure_foreach_until,
    .offset.terrain = map_terrain_get,
    .offset.building_id = map_building_at,
    .offset.is_draw_tile = map_property_is_draw_tile,
    .offset.tile_size = map_property_multi_tile_size,
    .offset.random = map_random_get,
    .building = building_get
};

typedef struct {
    color_t soldier;
    color_t selected_soldier;
    color_t enemy;
    color_t wolf;
    tile_color water[4];
    tile_color tree[4];
    tile_color rock[4];
    tile_color meadow[4];
    tile_color grass[8];
    tile_color road;
    tile_color wall;
    tile_color aqueduct;
    tile_color reservoir[2];
    tile_color house[2];
    tile_color building[2];
    tile_color monument[2];
} tile_color_set;

// Since the minimap tiles are only 25 color sets per climate, we just hardcode them.
// This "hack" is necessary to avoid reloading the climate graphics when selecting
// a scenario with another climate in the CCK selection screen, which is expensive.
static const tile_color_set MINIMAP_COLOR_SETS[3] = {
    // central
    {
        .soldier = COLOR_MINIMAP_SOLDIER,
        .selected_soldier = COLOR_MINIMAP_SELECTED_SOLDIER,
        .enemy = COLOR_MINIMAP_ENEMY_CENTRAL,
        .wolf = COLOR_MINIMAP_WOLF,
        .water = {{0xff394a7b, 0xff31427b}, {0xff394a7b, 0xff314273}, {0xff313973, 0xff314273}, {0xff31427b, 0xff394a7b}},
        .tree = {{0xff6b8431, 0xff102108}, {0xff103908, 0xff737b29}, {0xff103108, 0xff526b21}, {0xff737b31, 0xff084a10}},
        .rock = {{0xff948484, 0xff635a4a}, {0xffa59c94, 0xffb5ada5}, {0xffb5ada5, 0xff8c8484}, {0xff635a4a, 0xffa59c94}},
        .meadow = {{0xffd6bd63, 0xff9c8c39}, {0xff948c39, 0xffd6bd63}, {0xffd6bd63, 0xff9c9439}, {0xff848431, 0xffada54a}},
        .grass = {
            {0xff6b8c31, 0xff6b7b29}, {0xff738431, 0xff6b7b29}, {0xff6b7329, 0xff7b8c39}, {0xff527b29, 0xff6b7321},
            {0xff6b8431, 0xff737b31}, {0xff6b7b31, 0xff737b29}, {0xff636b18, 0xff526b21}, {0xff737b31, 0xff737b29}
        },
        .road = {0xff736b63, 0xff4a3121},
        .wall = {0xffd6d3c6, 0xfff7f3de},
        .aqueduct = {0xff5282bd, 0xff84baff},
        .reservoir = {{0xff5282bd, 0xff5282bd}, {0xff84baff, 0xff84baff}}, // Edges, center
        .house = {{0xffffb28c, 0xffd65110}, {0xffef824a, 0xffffa273}}, // Edges, center
        .building = {{0xfffffbde, 0xffefd34a}, {0xfffff3c6, 0xffffebb5}}, // Edges, center
        .monument = {{0xfff5deff, 0xffb84aef}, {0xffe9c6ff, 0xffdfb5ff}} // Edges, center
    },
    // northern
    {
        .soldier = COLOR_MINIMAP_SOLDIER,
        .selected_soldier = COLOR_MINIMAP_SELECTED_SOLDIER,
        .enemy = COLOR_MINIMAP_ENEMY_NORTHERN,
        .wolf = COLOR_MINIMAP_WOLF,
        .water = {{0xff394a7b, 0xff31427b}, {0xff394a7b, 0xff314273}, {0xff313973, 0xff314273}, {0xff31427b, 0xff394a7b}},
        .tree = {{0xff527b31, 0xff082108}, {0xff083908, 0xff5a7329}, {0xff082908, 0xff316b21}, {0xff527b29, 0xff084a21}},
        .rock = {{0xff8c8484, 0xff5a5252}, {0xff9c9c94, 0xffa5a5a5}, {0xffa5a5a5, 0xff848484}, {0xff5a5252, 0xff9c9c94}},
        .meadow = {{0xff427318, 0xff8c9442}, {0xffb5ad4a, 0xff738c39}, {0xff8c8c39, 0xff6b7b29}, {0xff527331, 0xff5a8442}},
        .grass = {
            {0xff4a8431, 0xff4a7329}, { 0xff527b29, 0xff4a7329 }, { 0xff526b29, 0xff5a8439 }, { 0xff397321, 0xff4a6b21 },
            { 0xff527b31, 0xff5a7331 }, { 0xff4a7329, 0xff5a7329 }, { 0xff4a6b18, 0xff316b21 }, { 0xff527b29, 0xff527329 }
        },
        .road = { 0xff736b63, 0xff4a3121 },
                .wall = { 0xffd6d3c6, 0xfff7f3de },
                .aqueduct = { 0xff5282bd, 0xff84baff },
                .reservoir = { {0xff5282bd, 0xff5282bd}, {0xff84baff, 0xff84baff} }, // Edges, center
                .house = { {0xffffb28c, 0xffd65110}, {0xffef824a, 0xffffa273} }, // Edges, center
                .building = { {0xfffffbde, 0xffefd34a}, {0xfffff3c6, 0xffffebb5} }, // Edges, center
                .monument = { {0xfff5deff, 0xffb84aef}, {0xffe9c6ff, 0xffdfb5ff} } // Edges, center
    },
    // desert
    {
        .soldier = COLOR_MINIMAP_SOLDIER,
        .selected_soldier = COLOR_MINIMAP_SELECTED_SOLDIER,
        .enemy = COLOR_MINIMAP_ENEMY_DESERT,
        .wolf = COLOR_MINIMAP_WOLF,
        .water = {{0xff4a84c6, 0xff4a7bc6}, {0xff4a84c6, 0xff4a7bc6}, {0xff4a84c6, 0xff5284c6}, {0xff4a7bbd, 0xff4a7bc6}},
        .tree = {{0xffa59c7b, 0xff6b7b18}, {0xff214210, 0xffada573}, {0xff526b21, 0xffcec6a5}, {0xffa59c7b, 0xff316321}},
        .rock = {{0xffa59494, 0xff736352}, {0xffa59c94, 0xffb5ada5}, {0xffb5ada5, 0xff8c847b}, {0xff736352, 0xffbdada5}},
        .meadow = {{0xff739c31, 0xff9cbd52}, {0xff7bb529, 0xff63ad21}, {0xff9cbd52, 0xff8c944a}, {0xff7ba539, 0xff739c31}},
        .grass = {
            {0xffbdbd9c, 0xffb5b594}, {0xffc6bda5, 0xffbdbda5}, {0xffbdbd9c, 0xffc6c6ad}, {0xffd6cead, 0xffc6bd9c},
            {0xffa59c7b, 0xffbdb594}, {0xffcecead, 0xffb5ad94}, {0xffc6c6a5, 0xffdedebd}, {0xffcecead, 0xffd6d6b5}
        },
        .road = {0xff6b5a52, 0xff4a4239},
        .wall = {0xffd6d3c6, 0xfff7f3de},
        .aqueduct = {0xff5282bd, 0xff84baff},
        .reservoir = {{0xff5282bd, 0xff5282bd}, {0xff84baff, 0xff84baff}}, // Edges, center
        .house = {{0xffffb28c, 0xffd65110}, {0xffef824a, 0xffffa273}}, // Edges, center
        .building = {{0xfffffbde, 0xffefd34a}, {0xfffff3c6, 0xffffebb5}}, // Edges, center
        .monument = {{0xfff5deff, 0xffb84aef}, {0xffe9c6ff, 0xffdfb5ff}} // Edges, center
    }
};

static struct {
    struct {
        int x;
        int y;
        int width;
        int height;
        int width_tiles;
        int height_tiles;
    } screen;
    struct {
        int x;
        int y;
        int width;
        int height;
    } minimap;
    struct {
        int width;
        int height;
        int stride;
        color_t *buffer;
    } cache;
    int offset_x;
    int offset_y;
    float scale;
    const minimap_functions *functions;
    struct {
        int x;
        int y;
        int grid_offset;
    } mouse;
    int refresh_requested;
    int camera_x;
    int camera_y;
} data = {
    .functions = &functions
};

void widget_minimap_invalidate(void)
{
    data.refresh_requested = 1;
}

static void foreach_map_tile(map_callback *callback)
{
    city_view_foreach_minimap_tile(0, 0, data.minimap.x, data.minimap.y, data.minimap.width, data.minimap.height, callback);
}

static void set_bounds(int x_offset, int y_offset, int width, int height)
{
    data.screen.x = x_offset;
    data.screen.y = y_offset;
    data.screen.width = width;
    data.screen.height = height;
    data.screen.width_tiles = width / 2;
    data.screen.height_tiles = height;

    city_view_get_camera(&data.camera_x, &data.camera_y);
    int view_width_tiles, view_height_tiles;
    city_view_get_viewport_size_tiles(&view_width_tiles, &view_height_tiles);

    data.minimap.width = data.functions->map.width();
    data.minimap.height = data.functions->map.height() * 2;
    data.minimap.x = (VIEW_X_MAX - data.minimap.width) / 2;
    data.minimap.y = (VIEW_Y_MAX - data.minimap.height) / 2;

    // ensure even height
    data.minimap.y &= ~1;

    if (data.minimap.width * 2 < data.screen.width) {
        data.screen.x += (data.screen.width - data.minimap.width * 2) / 2;
        data.screen.width = data.minimap.width * 2;
    } else if (view_width_tiles <= data.screen.width) {
        if (data.camera_x > data.screen.width_tiles)
           // scale_width = 1.0f;
            if ((data.minimap.width - data.width_tiles) / 2 > 0) {
                if (data.camera_x < data.absolute_x) {
                    data.absolute_x = data.camera_x;
                } else if (data.camera_x > data.width_tiles + data.absolute_x - view_width_tiles) {
                    data.absolute_x = view_width_tiles + data.camera_x - data.width_tiles;
                }
            }
        /*  /*else {
            data.absolute_x = data.camera_x;
            if (data.absolute_x < (VIEW_X_MAX - data.minimap_width) / 2) {
                data.absolute_x = (VIEW_X_MAX - data.minimap_width) / 2;
            }
            scale_width = data.width_tiles / (float) view_width_tiles;
        }*/
    }
    if (data.minimap.height < data.screen.height) {
        data.screen.y += (data.screen.height - data.minimap.height) / 2;
        data.screen.height = data.minimap.height;
    }




    /*
    data.absolute_x = (VIEW_X_MAX - data.minimap_width) / 2;
    data.absolute_y = (VIEW_Y_MAX - 2 * data.minimap_height) / 2;

    float scale_width;
    float scale_height;

    if (view_width_tiles <= data.width_tiles) {
        scale_width = 1.0f;
        if ((data.minimap_width - data.width_tiles) / 2 > 0) {
            if (data.camera_x < data.absolute_x) {
                data.absolute_x = data.camera_x;
            } else if (data.camera_x > data.width_tiles + data.absolute_x - view_width_tiles) {
                data.absolute_x = view_width_tiles + data.camera_x - data.width_tiles;
            }
        }
    } else {
        data.absolute_x = data.camera_x;
        if (data.absolute_x < (VIEW_X_MAX - data.minimap_width) / 2) {
            data.absolute_x = (VIEW_X_MAX - data.minimap_width) / 2;
        }
        scale_width = data.width_tiles / (float) view_width_tiles;
    }
    if (view_height_tiles <= data.height_tiles) {
        scale_height = 1.0f;
        if ((2 * data.minimap_height - data.height_tiles) / 2 > 0) {
            if (data.camera_y < data.absolute_y) {
                data.absolute_y = data.camera_y;
            } else if (data.camera_y > data.height_tiles + data.absolute_y - view_height_tiles) {
                data.absolute_y = view_height_tiles + data.camera_y - data.height_tiles;
            }
        }
    } else {
        data.absolute_y = data.camera_y;
        if (data.absolute_y < (VIEW_Y_MAX - 2 * data.minimap_height) / 2) {
            data.absolute_y = (VIEW_Y_MAX - 2 * data.minimap_height) / 2;
        }
        scale_height = data.height_tiles / (float) view_height_tiles;
    }
    if (scale_width < scale_height) {
        data.scale = scale_width;
    } else {
        data.scale = scale_height;
    }
    */
}

static int has_figure_color(figure *f)
{
    int type = f->type;
    if (figure_is_legion(f)) {
        return formation_get_selected() == f->formation_id ?
            FIGURE_COLOR_SELECTED_SOLDIER : FIGURE_COLOR_SOLDIER;
    }
    if (figure_is_enemy(f)) {
        return FIGURE_COLOR_ENEMY;
    }
    if (f->type == FIGURE_INDIGENOUS_NATIVE &&
        f->action_state == FIGURE_ACTION_159_NATIVE_ATTACKING) {
        return FIGURE_COLOR_ENEMY;
    }
    if (type == FIGURE_WOLF) {
        return FIGURE_COLOR_WOLF;
    }
    return FIGURE_COLOR_NONE;
}

static inline void draw_pixel(int x, int y, color_t color)
{
    if (x < 0 || x >= data.minimap.width * 2 || y < 0 || y >= data.minimap.height) {
        return;
    }
    data.cache.buffer[y * data.cache.stride + x] = color;
}

static inline void draw_tile(int x_offset, int y_offset, const tile_color *colors)
{
    draw_pixel(x_offset, y_offset, colors->left);
    draw_pixel(x_offset + 1, y_offset, colors->right);
}

static int draw_figure(int x_view, int y_view, int grid_offset, const tile_color_set *set)
{
    if (!data.functions->offset.figure) {
        return 0;
    }
    int color_type = data.functions->offset.figure(grid_offset, has_figure_color);
    if (color_type == FIGURE_COLOR_NONE) {
        return 0;
    }
    color_t color = set->wolf;
    if (color_type == FIGURE_COLOR_SOLDIER) {
        color = set->soldier;
    } else if (color_type == FIGURE_COLOR_SELECTED_SOLDIER) {
        color = set->selected_soldier;
    } else if (color_type == FIGURE_COLOR_ENEMY) {
        color = set->enemy;
    }
    draw_pixel(x_view, y_view, color);
    draw_pixel(x_view + 1, y_view, color);
    return 1;
}

static void draw_building(int size, int x_offset, int y_offset, const tile_color *colors)
{
    if (size == 1) {
        draw_tile(x_offset, y_offset, &colors[1]);
        return;
    }
    int width = size * 2;
    int height = width - 1;
    y_offset -= size - 1;
    int start_y = y_offset < 0 ? -y_offset : 0;
    int end_y = height / 2 + 1;
    if (end_y + y_offset > data.screen.height) {
        end_y = data.screen.height - y_offset;
    }
    for (int y = start_y; y < end_y; y++) {
        int x_start = height / 2 - y;
        int x_end = width - x_start - 1;
        draw_pixel(x_start + x_offset, y + y_offset, colors[1].left);
        draw_pixel(x_end + x_offset, y + y_offset, colors[1].right);
        if (x_start + x_offset < 0) {
            x_start = -x_offset - 1;
        }
        if (x_end + x_offset >= data.screen.width) {
            x_end = data.screen.width - x_offset;
        }
        color_t *value = &data.cache.buffer[(y_offset + y) * data.cache.stride + x_start + x_offset + 1];
        for (int x = x_start; x < x_end - 1; x++) {
            *value++ = ((x + y) & 1) ? colors[0].left : colors[0].right;
        }
    }
    y_offset += height / 2 + 1;
    start_y = y_offset < 0 ? -y_offset : 0;
    end_y = height / 2;
    if (end_y + y_offset > data.screen.height) {
        end_y = data.screen.height - y_offset;
    }

    for (int y = start_y; y < end_y; y++) {
        int x_start = y + 1;
        int x_end = width - x_start - 1;
        draw_pixel(x_start + x_offset, y + y_offset, colors[1].left);
        draw_pixel(x_end + x_offset, y + y_offset, colors[1].right);
        if (x_start + x_offset < 0) {
            x_start = -x_offset - 1;
        }
        if (x_end + x_offset >= data.screen.width) {
            x_end = data.screen.width - x_offset;
        }
        color_t *value = &data.cache.buffer[(y_offset + y) * data.cache.stride + x_start + x_offset + 1];
        for (int x = x_start; x < x_end - 1; x++) {
            *value++ = ((x + y) & 1) ? colors[0].right : colors[0].left;
        }
    }
}

static void draw_minimap_tile(int x_view, int y_view, int grid_offset)
{
    const tile_color_set *set = &MINIMAP_COLOR_SETS[data.functions->climate()];

    if (grid_offset < 0) {
        return;
    }

    if (draw_figure(x_view, y_view, grid_offset, set)) {
        return;
    }
    int terrain = data.functions->offset.terrain(grid_offset);
    // exception for fort ground: display as empty land
    if (terrain & TERRAIN_BUILDING && data.functions->building) {
        if (data.functions->building(data.functions->offset.building_id(grid_offset))->type == BUILDING_FORT_GROUND) {
            terrain = 0;
        }
    }

    if (terrain & TERRAIN_BUILDING) {
        if (data.functions->offset.is_draw_tile(grid_offset)) {
            const tile_color *colors;
            if (data.functions->building) {
                building *b = data.functions->building(data.functions->offset.building_id(grid_offset));
                if (b->house_size) {
                    colors = set->house;
                } else if (b->type == BUILDING_RESERVOIR) {
                    colors = set->reservoir;
                } else if (building_monument_is_monument(b)) {
                    colors = set->monument;
                } else {
                    colors = set->building;
                }
            } else {
                colors = set->building;
            }
            int size = data.functions->offset.tile_size(grid_offset);
            draw_building(size, x_view, y_view, colors);
        }
        return;
    }
    int rand = data.functions->offset.random(grid_offset);
    const tile_color *colors;
    if (terrain & TERRAIN_ROAD) {
        colors = &set->road;
    } else if (terrain & TERRAIN_WATER) {
        colors = &set->water[rand & 3];
    } else if (terrain & (TERRAIN_SHRUB | TERRAIN_TREE)) {
        colors = &set->tree[rand & 3];
    } else if (terrain & (TERRAIN_ROCK | TERRAIN_ELEVATION)) {
        colors = &set->rock[rand & 3];
    } else if (terrain & TERRAIN_AQUEDUCT) {
        colors = &set->aqueduct;
    } else if (terrain & TERRAIN_WALL) {
        colors = &set->wall;
    } else if (terrain & TERRAIN_MEADOW) {
        colors = &set->meadow[rand & 3];
    } else {
        colors = &set->grass[rand & 7];
    }
    draw_tile(x_view, y_view, colors);
}

static void draw_viewport_rectangle(void)
{
    int camera_x, camera_y;
    int camera_pixels_x, camera_pixels_y;
    city_view_get_camera(&camera_x, &camera_y);
    city_view_get_pixel_offset(&camera_pixels_x, &camera_pixels_y);
    int view_width_tiles, view_height_tiles;
    city_view_get_viewport_size_tiles(&view_width_tiles, &view_height_tiles);

    int x_offset = data.screen.x + 2 * (camera_x - data.minimap.x) - 2 + camera_pixels_x / 30;
    if (x_offset < data.screen.x) {
        x_offset = data.screen.x;
    }
    if (x_offset + 2 * view_width_tiles + 4 > data.screen.x + data.screen.width) {
        x_offset -= 2;
    }
    int y_offset = data.screen.y + camera_y - data.minimap.y + 2;
    graphics_draw_rect(x_offset, y_offset,
        view_width_tiles * 2 + 4,
        view_height_tiles - 4,
        COLOR_MINIMAP_VIEWPORT);
}

static void prepare_minimap_cache(void)
{
    if (data.functions->map.width() != data.minimap.width || data.functions->map.height() * 2 != data.minimap.height ||
        !graphics_renderer()->has_custom_image(CUSTOM_IMAGE_MINIMAP)) {
        graphics_renderer()->create_custom_image(CUSTOM_IMAGE_MINIMAP, data.functions->map.width() * 2,
            data.functions->map.height() * 2, 0);
    }
    data.cache.buffer = graphics_renderer()->get_custom_image_buffer(CUSTOM_IMAGE_MINIMAP, &data.cache.stride);
}

static void clear_minimap(void)
{
    memset(data.cache.buffer, 0, data.functions->map.height() * 2 * data.cache.stride * sizeof(color_t));
}

static void draw_minimap(void)
{
    if (!data.cache.buffer) {
        return;
    }
    graphics_set_clip_rectangle(data.screen.x, data.screen.y, data.screen.width, data.screen.height);
    clear_minimap();
    foreach_map_tile(draw_minimap_tile);
    graphics_renderer()->update_custom_image(CUSTOM_IMAGE_MINIMAP);
    graphics_renderer()->draw_custom_image(CUSTOM_IMAGE_MINIMAP, data.screen.x, data.screen.y, SCALE_NONE);
    draw_viewport_rectangle();
    graphics_reset_clip_rectangle();
}

static void draw_uncached(int x_offset, int y_offset, int width, int height)
{
    prepare_minimap_cache();
    set_bounds(x_offset, y_offset, width, height);
    draw_minimap();
}

static void draw_using_cache(int x_offset, int y_offset, int width, int height)
{
    /*
    if (width != data.screenwidth || height != data.height) {
        draw_uncache.bufferd(x_offset, y_offset, width, height);
        return;
    }

    /*int old_absolute_x = data.absolute_x;
    int old_absolute_y = data.absolute_y;
    set_bounds(x_offset, y_offset, width, height);
    if (data.absolute_x != old_absolute_x || data.absolute_y != old_absolute_y) {
        draw_minimap();
        return;
    }*/

    graphics_set_clip_rectangle(x_offset, y_offset, width, height);
    graphics_renderer()->draw_custom_image(CUSTOM_IMAGE_MINIMAP, data.screen.x, data.screen.y, SCALE_NONE);
    draw_viewport_rectangle();
    graphics_reset_clip_rectangle();
}

static int should_refresh(int force)
{
    if (data.refresh_requested || force) {
        data.refresh_requested = 0;
        return REFRESH_FULL;
    }
    int new_x, new_y;
    city_view_get_camera(&new_x, &new_y);
    if (data.camera_x != new_x || data.camera_y != new_y) {
        return REFRESH_CAMERA_MOVED;
    }
    return REFRESH_NOT_NEEDED;
}

void widget_minimap_draw(int x_offset, int y_offset, int width, int height, int force)
{
    int refresh_type = should_refresh(force);
    if (refresh_type != REFRESH_NOT_NEEDED) {
        graphics_fill_rect(x_offset, y_offset, width, height, COLOR_BLACK);
        draw_uncached(x_offset, y_offset, width, height);
/*
        if (refresh_type == REFRESH_FULL) {
            draw_uncached(x_offset, y_offset, width, height);
        } else {
            draw_using_cache(x_offset, y_offset, width, height);
        }
        */
        graphics_draw_line(x_offset - 1, x_offset - 1 + width, y_offset - 1, y_offset - 1, COLOR_MINIMAP_DARK);
        graphics_draw_line(x_offset - 1, x_offset - 1, y_offset, y_offset + height, COLOR_MINIMAP_DARK);
        graphics_draw_line(x_offset - 1 + width, x_offset - 1 + width, y_offset,
            y_offset + height, COLOR_MINIMAP_LIGHT);
    }
}

static void update_mouse_grid_offset(int x_view, int y_view, int grid_offset)
{
    if (data.mouse.y == y_view && (data.mouse.x == x_view || data.mouse.x == x_view + 1)) {
        data.mouse.grid_offset = grid_offset < 0 ? 0 : grid_offset;
    }
}

static int get_mouse_grid_offset(const mouse *m)
{
    data.mouse.x = m->x;
    data.mouse.y = m->y;
    data.mouse.grid_offset = 0;
    foreach_map_tile(update_mouse_grid_offset);
    return data.mouse.grid_offset;
}

static int is_in_minimap(const mouse *m)
{
    if (m->x >= data.screen.x && m->x < data.screen.x + data.screen.width &&
        m->y >= data.screen.y && m->y < data.screen.y + data.screen.height) {
        return 1;
    }
    return 0;
}

int widget_minimap_handle_mouse(const mouse *m)
{
    if ((m->left.went_down || m->right.went_down) && is_in_minimap(m)) {
        int grid_offset = get_mouse_grid_offset(m);
        if (grid_offset > 0) {
            city_view_go_to_grid_offset(grid_offset);
            return 1;
        }
    }
    return 0;
}
