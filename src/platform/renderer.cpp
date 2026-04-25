#include "renderer.h"

#include "core/calc.h"
#include "core/config.h"
#include "core/time.h"
#include "graphics/renderer.h"
#include "graphics/screen.h"
#include "input/mouse.h"
#include "platform/cursor.h"
#include "platform/emscripten/emscripten.h"
#include "platform/platform.h"
#include "platform/screen.h"
#include "platform/switch/switch.h"
#include "platform/vita/vita.h"

#include "platform/render_2d_pipeline.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#if SDL_VERSION_ATLEAST(2, 0, 1)
#define USE_YUV_TEXTURES
#define HAS_YUV_TEXTURES (platform_sdl_version_at_least(2, 0, 1))
#else
#define HAS_YUV_TEXTURES 0
#endif

#if SDL_VERSION_ATLEAST(2, 0, 10)
#define USE_RENDERCOPYF
#define HAS_RENDERCOPYF (platform_sdl_version_at_least(2, 0, 10))
#endif

#if SDL_VERSION_ATLEAST(2, 0, 12)
#define USE_TEXTURE_SCALE_MODE
#define HAS_TEXTURE_SCALE_MODE (platform_sdl_version_at_least(2, 0, 12))
#else
#define HAS_TEXTURE_SCALE_MODE 0
#endif

#define MAX_UNPACKED_IMAGES 20

#define MAX_PACKED_IMAGE_SIZE 64000

#if (defined(__ANDROID__) || defined(__EMSCRIPTEN__)) && !SDL_VERSION_ATLEAST(2, 24, 0)
// On the arm versions of android, on SDL < 2.24.0, atlas textures that are too large will make the renderer fetch
// some images from the atlas with an off-by-one pixel, making things look terrible. Defining a smaller atlas texture
// prevents the problem, at the cost of performance due to the extra texture context switching.
// This also happens on emscripten for android, hence the emscripten inclusion.
#define MAX_TEXTURE_SIZE 1024
#endif

#ifdef __vita__
// On Vita, due to the small amount of VRAM, having textures that are too large will cause the game to eventually crash
// when changing climates, due to lack of contiguous memory space. Creating smaller atlases mitigates the issue
#define MAX_TEXTURE_SIZE 2048
#endif

typedef struct buffer_texture {
    SDL_Texture *texture;
    int id;
    int width;
    int height;
    int raw_width;
    int raw_height;
    int tex_width;
    int tex_height;
    struct buffer_texture *next;
} buffer_texture;

typedef struct silhouette_texture {
    const image *img;
    SDL_Texture *texture;
    struct silhouette_texture *next;
} silhouette_texture;

typedef struct managed_image_resource {
    SDL_Texture *texture;
    int width;
    int height;
} managed_image_resource;

static void draw_image_request(const render_2d_request *request);

typedef struct render_state {
    SDL_Texture *target;
    SDL_Rect viewport;
    SDL_Rect clip_rect;
    SDL_BlendMode blend_mode;
    float scale_x;
    float scale_y;
    render_domain domain;
    int clip_enabled;
} render_state;

static Render2DPipeline g_render_2d_pipeline;

static struct {
    SDL_Renderer *renderer;
    SDL_Texture *render_texture;
    int is_software_renderer;
    int paused;
    struct {
        SDL_Texture *texture;
        int size;
        struct {
            int x, y;
        } hotspot;
    } cursors[CURSOR_MAX];
    struct {
        SDL_Texture *texture;
        int texture_width;
        int texture_height;
        int x;
        int y;
        int width;
        int height;
        int opacity;
    } tooltip;
    SDL_Texture **texture_lists[ATLAS_MAX];
    image_atlas_data atlas_data[ATLAS_MAX];
    struct {
        SDL_Texture *texture;
        color_t *buffer;
        image img;
    } custom_textures[CUSTOM_IMAGE_MAX];
    struct {
        int width;
        int height;
    } max_texture_size;
    struct {
        buffer_texture *first;
        buffer_texture *last;
        int current_id;
    } texture_buffers;
    silhouette_texture *silhouettes;
    struct {
        int id;
        time_millis last_used;
        SDL_Texture *texture;
    } unpacked_images[MAX_UNPACKED_IMAGES];
    std::vector<managed_image_resource> image_resources;
    graphics_renderer_interface renderer_interface;
    int supports_yuv_textures;
    float city_scale;
    render_domain active_render_domain;
    int should_correct_texture_offset;
    int disable_linear_filter;
} data = {};

static render_state tooltip_render_state;
static int tooltip_render_state_valid;
static render_state render_state_stack[16];
static int render_state_stack_depth;

static void save_render_state(render_state *state)
{
    if (!state) {
        return;
    }
    state->target = SDL_GetRenderTarget(data.renderer);
    SDL_RenderGetViewport(data.renderer, &state->viewport);
    state->clip_enabled = SDL_RenderIsClipEnabled(data.renderer);
    if (state->clip_enabled) {
        SDL_RenderGetClipRect(data.renderer, &state->clip_rect);
    } else {
        SDL_memset(&state->clip_rect, 0, sizeof(state->clip_rect));
    }
    SDL_GetRenderDrawBlendMode(data.renderer, &state->blend_mode);
    SDL_RenderGetScale(data.renderer, &state->scale_x, &state->scale_y);
    state->domain = data.active_render_domain;
}

static void restore_render_state(const render_state *state)
{
    if (!state) {
        return;
    }
    SDL_SetRenderTarget(data.renderer, state->target);
    data.active_render_domain = state->domain;
    SDL_RenderSetScale(data.renderer, state->scale_x, state->scale_y);
    SDL_RenderSetViewport(data.renderer, &state->viewport);
    if (state->clip_enabled) {
        SDL_RenderSetClipRect(data.renderer, &state->clip_rect);
    } else {
        SDL_RenderSetClipRect(data.renderer, NULL);
    }
    SDL_SetRenderDrawBlendMode(data.renderer, state->blend_mode);
}

static void push_render_state(void)
{
    if (data.paused || render_state_stack_depth >= 16) {
        return;
    }
    save_render_state(&render_state_stack[render_state_stack_depth]);
    ++render_state_stack_depth;
}

static void pop_render_state(void)
{
    if (data.paused || render_state_stack_depth <= 0) {
        return;
    }
    --render_state_stack_depth;
    restore_render_state(&render_state_stack[render_state_stack_depth]);
}

static int save_screen_buffer(color_t *pixels, int x, int y, int width, int height, int row_width)
{
    if (data.paused) {
        return 0;
    }
    SDL_Rect rect = { x, y, width, height };
    return SDL_RenderReadPixels(data.renderer, &rect, SDL_PIXELFORMAT_ARGB8888, pixels,
        row_width * sizeof(color_t)) == 0;
}

static void draw_line(int x_start, int x_end, int y_start, int y_end, color_t color)
{
    if (data.paused) {
        return;
    }
    SDL_SetRenderDrawColor(data.renderer,
        (color & COLOR_CHANNEL_RED) >> COLOR_BITSHIFT_RED,
        (color & COLOR_CHANNEL_GREEN) >> COLOR_BITSHIFT_GREEN,
        (color & COLOR_CHANNEL_BLUE) >> COLOR_BITSHIFT_BLUE,
        (color & COLOR_CHANNEL_ALPHA) >> COLOR_BITSHIFT_ALPHA);
    SDL_RenderDrawLine(data.renderer, x_start, y_start, x_end, y_end);
}

static void draw_rect(int x_start, int x_end, int y_start, int y_end, color_t color)
{
    if (data.paused) {
        return;
    }
    SDL_SetRenderDrawColor(data.renderer,
        (color & COLOR_CHANNEL_RED) >> COLOR_BITSHIFT_RED,
        (color & COLOR_CHANNEL_GREEN) >> COLOR_BITSHIFT_GREEN,
        (color & COLOR_CHANNEL_BLUE) >> COLOR_BITSHIFT_BLUE,
        (color & COLOR_CHANNEL_ALPHA) >> COLOR_BITSHIFT_ALPHA);
    SDL_Rect rect = { x_start, y_start, x_end, y_end };
    SDL_RenderDrawRect(data.renderer, &rect);
}

static void fill_rect(int x_start, int x_end, int y_start, int y_end, color_t color)
{
    if (data.paused) {
        return;
    }
    SDL_SetRenderDrawColor(data.renderer,
        (color & COLOR_CHANNEL_RED) >> COLOR_BITSHIFT_RED,
        (color & COLOR_CHANNEL_GREEN) >> COLOR_BITSHIFT_GREEN,
        (color & COLOR_CHANNEL_BLUE) >> COLOR_BITSHIFT_BLUE,
        (color & COLOR_CHANNEL_ALPHA) >> COLOR_BITSHIFT_ALPHA);
    SDL_Rect rect = { x_start, y_start, x_end, y_end };
    SDL_RenderFillRect(data.renderer, &rect);
}

static float scale_for_domain(render_domain domain)
{
    return g_render_2d_pipeline.scale_for_domain(domain, platform_screen_get_scale());
}

static int is_pixel_domain(render_domain domain)
{
    return domain == RENDER_DOMAIN_PIXEL
        || domain == RENDER_DOMAIN_TOOLTIP_PIXEL
        || domain == RENDER_DOMAIN_SNAPSHOT_PIXEL;
}

static int should_use_linear_scale_filter(void)
{
    switch (config_get(CONFIG_UI_SCALE_FILTER)) {
        case CONFIG_UI_SCALE_FILTER_NEAREST:
            return 0;
        case CONFIG_UI_SCALE_FILTER_LINEAR:
            return 1;
        case CONFIG_UI_SCALE_FILTER_AUTO:
        default:
            break;
    }
#ifndef __APPLE__
    return (platform_screen_get_scale() % 100) != 0;
#else
    return 1;
#endif
}

static const char *configured_scale_quality_hint(void)
{
    return should_use_linear_scale_filter() ? "linear" : "nearest";
}

#ifdef USE_TEXTURE_SCALE_MODE
static SDL_ScaleMode configured_scale_mode(void)
{
    return should_use_linear_scale_filter() ? SDL_ScaleModeLinear : SDL_ScaleModeNearest;
}
#endif

static void set_output_scale(float scale)
{
    if (data.paused || !data.renderer) {
        return;
    }
    if (scale <= 0.0f) {
        scale = 1.0f;
    }
    SDL_RenderSetScale(data.renderer, scale, scale);
}

static void set_render_domain(render_domain domain)
{
    data.active_render_domain = domain;
    set_output_scale(scale_for_domain(domain));
}

static render_domain get_render_domain(void)
{
    return data.active_render_domain;
}

static void set_clip_rectangle(int x, int y, int width, int height)
{
    if (data.paused) {
        return;
    }
    SDL_Rect clip = { x, y, width, height };
    SDL_RenderSetClipRect(data.renderer, &clip);
}

static void reset_clip_rectangle(void)
{
    if (data.paused) {
        return;
    }
    SDL_RenderSetClipRect(data.renderer, NULL);
}

static void set_viewport(int x, int y, int width, int height)
{
    if (data.paused) {
        return;
    }
    SDL_Rect viewport = { x, y, width, height };
    SDL_RenderSetViewport(data.renderer, &viewport);
}

static void reset_viewport(void)
{
    if (data.paused) {
        return;
    }
    SDL_RenderSetViewport(data.renderer, NULL);
    SDL_RenderSetClipRect(data.renderer, NULL);
}

static void clear_screen(void)
{
    if (data.paused) {
        return;
    }
    SDL_SetRenderDrawColor(data.renderer, 0, 0, 0, 255);
    SDL_RenderClear(data.renderer);
}

static void get_max_image_size(int *width, int *height)
{
    *width = data.max_texture_size.width;
    *height = data.max_texture_size.height;
}

static void free_silhouettes(void)
{
    silhouette_texture *silhouette = data.silhouettes;
    while (silhouette) {
        silhouette_texture *current = silhouette;
        silhouette = silhouette->next;
        SDL_DestroyTexture(current->texture);
        free(current);
    }
    data.silhouettes = 0;
}

static void free_managed_image_resources(void)
{
    for (managed_image_resource &resource : data.image_resources) {
        if (resource.texture) {
            SDL_DestroyTexture(resource.texture);
            resource.texture = 0;
        }
        resource.width = 0;
        resource.height = 0;
    }
    data.image_resources.clear();
}

static image_handle reserve_managed_image_resource_slot(void)
{
    if (data.image_resources.empty()) {
        data.image_resources.resize(1);
    }
    for (size_t i = 1; i < data.image_resources.size(); ++i) {
        if (!data.image_resources[i].texture) {
            return static_cast<image_handle>(i);
        }
    }
    data.image_resources.push_back({});
    return static_cast<image_handle>(data.image_resources.size() - 1);
}

static SDL_Texture *create_texture_from_pixels(const color_t *pixels, int width, int height)
{
    if (!pixels || width <= 0 || height <= 0) {
        return 0;
    }
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom((void *) pixels,
        width,
        height,
        32,
        width * sizeof(color_t),
        COLOR_CHANNEL_RED,
        COLOR_CHANNEL_GREEN,
        COLOR_CHANNEL_BLUE,
        COLOR_CHANNEL_ALPHA);
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create image resource surface. Reason: %s", SDL_GetError());
        return 0;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(data.renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create image resource texture. Reason: %s", SDL_GetError());
        return 0;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

static SDL_Texture *get_managed_texture(image_handle handle)
{
    if (handle <= 0 || handle >= static_cast<image_handle>(data.image_resources.size())) {
        return 0;
    }
    return data.image_resources[handle].texture;
}

static image_handle request_image_handle(const render_2d_request *request)
{
    if (!request) {
        return 0;
    }
    if (request->handle > 0) {
        return request->handle;
    }
    if (request->img && request->img->resource_handle > 0) {
        return request->img->resource_handle;
    }
    return 0;
}

static void draw_managed_image_request(const managed_image_request *request)
{
    if (!request || request->handle <= 0 || request->width <= 0 || request->height <= 0) {
        return;
    }

    image runtime_image = {};
    runtime_image.width = request->width;
    runtime_image.height = request->height;
    runtime_image.x_offset = request->x_offset;
    runtime_image.y_offset = request->y_offset;
    runtime_image.is_isometric = request->is_isometric;
    runtime_image.resource_handle = request->handle;

    render_2d_request bridged_request = {};
    bridged_request.img = &runtime_image;
    bridged_request.handle = request->handle;
    bridged_request.x = request->x;
    bridged_request.y = request->y;
    bridged_request.logical_width = request->logical_width;
    bridged_request.logical_height = request->logical_height;
    bridged_request.color = request->color;
    bridged_request.domain = request->domain;
    bridged_request.scaling_policy = request->scaling_policy;
    bridged_request.angle = request->angle;
    bridged_request.disable_coord_scaling = request->disable_coord_scaling;
    draw_image_request(&bridged_request);
}

static void free_unpacked_assets(void)
{
    for (int i = 0; i < MAX_UNPACKED_IMAGES; i++) {
        if (data.unpacked_images[i].texture) {
            SDL_DestroyTexture(data.unpacked_images[i].texture);
        }
    }
    memset(data.unpacked_images, 0, sizeof(data.unpacked_images));
}

static void free_texture_atlas(atlas_type type)
{
    if (!data.texture_lists[type]) {
        return;
    }
    SDL_Texture **list = data.texture_lists[type];
    data.texture_lists[type] = 0;
    for (int i = 0; i < data.atlas_data[type].num_images; i++) {
        if (list[i]) {
            SDL_DestroyTexture(list[i]);
        }
    }
    free(list);

    if (type == ATLAS_EXTRA_ASSET) {
        free_unpacked_assets();
    }
}

static void free_atlas_data_buffers(atlas_type type)
{
    image_atlas_data *atlas_data = &data.atlas_data[type];
    if (atlas_data->buffers) {
#ifndef __VITA__
        for (int i = 0; i < atlas_data->num_images; i++) {
            free(atlas_data->buffers[i]);
        }
#endif
        free(atlas_data->buffers);
        atlas_data->buffers = 0;
    }
    if (atlas_data->image_widths) {
        free(atlas_data->image_widths);
        atlas_data->image_widths = 0;
    }
    if (atlas_data->image_heights) {
        free(atlas_data->image_heights);
        atlas_data->image_heights = 0;
    }
}

static void reset_atlas_data(atlas_type type)
{
    free_atlas_data_buffers(type);
    data.atlas_data[type].num_images = 0;
    data.atlas_data[type].type = type;
}

static void free_texture_atlas_and_data(atlas_type type)
{
    free_texture_atlas(type);
    reset_atlas_data(type);
}

static const image_atlas_data *prepare_texture_atlas(atlas_type type, int num_images, int last_width, int last_height)
{
    free_texture_atlas_and_data(type);
    image_atlas_data *atlas_data = &data.atlas_data[type];
    atlas_data->num_images = num_images;
    atlas_data->image_widths = static_cast<int *>(malloc(sizeof(int) * num_images));
    atlas_data->image_heights = static_cast<int *>(malloc(sizeof(int) * num_images));
    atlas_data->buffers = static_cast<color_t **>(malloc(sizeof(color_t *) * num_images));
    if (!atlas_data->image_widths || !atlas_data->image_heights || !atlas_data->buffers) {
        reset_atlas_data(type);
        return 0;
    }
#ifdef __VITA__
    SDL_Texture **list = static_cast<SDL_Texture **>(malloc(sizeof(SDL_Texture *) * num_images));
    if (!list) {
        reset_atlas_data(type);
        return 0;
    }
    memset(list, 0, sizeof(SDL_Texture *) * num_images);
    for (int i = 0; i < num_images; i++) {
        int width = i == num_images - 1 ? last_width : data.max_texture_size.width;
        atlas_data->image_heights[i] = i == num_images - 1 ? last_height : data.max_texture_size.height;
        SDL_Log("Creating atlas texture with size %dx%d", width, atlas_data->image_heights[i]);
        list[i] = SDL_CreateTexture(data.renderer,
            SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, atlas_data->image_heights[i]);
        if (!list[i]) {
            SDL_LogError(SDL_LOG_PRIORITY_ERROR, "Unable to create texture. Reason: %s", SDL_GetError());
            free_texture_atlas(type);
            reset_atlas_data(type);
            continue;
        }
        SDL_Log("Texture created");
        SDL_LockTexture(list[i], NULL, (void **) &atlas_data->buffers[i], &atlas_data->image_widths[i]);
        atlas_data->image_widths[i] /= sizeof(color_t);
        SDL_SetTextureBlendMode(list[i], SDL_BLENDMODE_BLEND);
    }
    data.texture_lists[type] = list;
#else
    for (int i = 0; i < num_images; i++) {
        atlas_data->image_widths[i] = i == num_images - 1 ? last_width : data.max_texture_size.width;
        atlas_data->image_heights[i] = i == num_images - 1 ? last_height : data.max_texture_size.height;
        size_t size = sizeof(color_t) * atlas_data->image_widths[i] * atlas_data->image_heights[i];
        atlas_data->buffers[i] = static_cast<color_t *>(malloc(size));
        if (!atlas_data->buffers[i]) {
            reset_atlas_data(type);
            return 0;
        }
        memset(atlas_data->buffers[i], 0, size);
    }
#endif
    return atlas_data;
}

static int create_texture_atlas(const image_atlas_data *atlas_data, int delete_buffers)
{
    if (!atlas_data || atlas_data != &data.atlas_data[atlas_data->type] || !atlas_data->num_images) {
        return 0;
    }
#ifdef __VITA__
    SDL_Texture **list = data.texture_lists[atlas_data->type];
    for (int i = 0; i < atlas_data->num_images; i++) {
        SDL_UnlockTexture(list[i]);
    }
#else
    data.texture_lists[atlas_data->type] =
        static_cast<SDL_Texture **>(malloc(sizeof(SDL_Texture *) * atlas_data->num_images));
    SDL_Texture **list = data.texture_lists[atlas_data->type];
    if (!list) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create texture lists for atlas %u - out of memory",
            atlas_data->type);
        return 0;
    }
    memset(list, 0, sizeof(SDL_Texture *) * atlas_data->num_images);
    for (int i = 0; i < atlas_data->num_images; i++) {
        SDL_Log("Creating atlas texture with size %dx%d", atlas_data->image_widths[i], atlas_data->image_heights[i]);
        SDL_Surface *surface = SDL_CreateRGBSurfaceFrom((void *) atlas_data->buffers[i],
            atlas_data->image_widths[i], atlas_data->image_heights[i],
            32, atlas_data->image_widths[i] * sizeof(color_t),
            COLOR_CHANNEL_RED, COLOR_CHANNEL_GREEN, COLOR_CHANNEL_BLUE, COLOR_CHANNEL_ALPHA);
        if (!surface) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create surface for texture. Reason: %s",
                SDL_GetError());
            free_texture_atlas(atlas_data->type);
            return 0;
        }
        list[i] = SDL_CreateTextureFromSurface(data.renderer, surface);
        SDL_FreeSurface(surface);
        if (delete_buffers) {
            free(atlas_data->buffers[i]);
            atlas_data->buffers[i] = 0;
        }
        if (!list[i]) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create texture. Reason: %s", SDL_GetError());
            free_texture_atlas(atlas_data->type);
            return 0;
        }
        SDL_SetTextureBlendMode(list[i], SDL_BLENDMODE_BLEND);
    }
#endif
    if (delete_buffers) {
        free_atlas_data_buffers(atlas_data->type);
    }
    return 1;
}

static int has_texture_atlas(atlas_type type)
{
    return data.texture_lists[type] != 0;
}

static const image_atlas_data *get_texture_atlas(atlas_type type)
{
    if (!has_texture_atlas(type)) {
        return 0;
    }
    return &data.atlas_data[type];
}

static void free_all_textures(void)
{
    for (int i = ATLAS_FIRST; i < ATLAS_MAX - 1; i++) {
        free_texture_atlas_and_data(static_cast<atlas_type>(i));
    }
    for (int i = 0; i < CUSTOM_IMAGE_MAX; i++) {
        if (data.custom_textures[i].texture) {
            SDL_DestroyTexture(data.custom_textures[i].texture);
            data.custom_textures[i].texture = 0;
#ifndef __vita__
            free(data.custom_textures[i].buffer);
#endif
            data.custom_textures[i].buffer = 0;
            memset(&data.custom_textures[i].img, 0, sizeof(image));
        }
    }

    free_silhouettes();
    free_managed_image_resources();

    if (data.tooltip.texture) {
        SDL_DestroyTexture(data.tooltip.texture);
        data.tooltip.texture = 0;
    }

    buffer_texture *texture_info = data.texture_buffers.first;
    while (texture_info) {
        buffer_texture *current = texture_info;
        texture_info = texture_info->next;
        SDL_DestroyTexture(current->texture);
        free(current);
    }
    data.texture_buffers.first = 0;
    data.texture_buffers.last = 0;
    data.texture_buffers.current_id = 0;
}

static SDL_Texture *get_texture(int texture_id)
{
    atlas_type type = static_cast<atlas_type>(texture_id >> IMAGE_ATLAS_BIT_OFFSET);
    if (type == ATLAS_CUSTOM) {
        int custom_image_id = texture_id & IMAGE_ATLAS_BIT_MASK;
        if (custom_image_id < 0 || custom_image_id >= CUSTOM_IMAGE_MAX) {
            return 0;
        }
        return data.custom_textures[custom_image_id].texture;
    } else if (type == ATLAS_EXTERNAL) {
        return data.custom_textures[CUSTOM_IMAGE_EXTERNAL].texture;
    } else if (type == ATLAS_UNPACKED_EXTRA_ASSET) {
        int unpacked_asset_id = texture_id & IMAGE_ATLAS_BIT_MASK;
        for (int i = 0; i < MAX_UNPACKED_IMAGES; i++) {
            if (data.unpacked_images[i].id == unpacked_asset_id && data.unpacked_images[i].texture) {
                return data.unpacked_images[i].texture;
            }
        }
        return 0;
    }
    if (!data.texture_lists[type]) {
        return 0;
    }
    return data.texture_lists[type][texture_id & IMAGE_ATLAS_BIT_MASK];
}

static SDL_Texture *get_silhouette_texture(const image *img);

static void set_texture_color_and_filter(SDL_Texture *texture, color_t color, int use_linear_filter)
{
    if (!color) {
        color = COLOR_MASK_NONE;
    }

    SDL_SetTextureColorMod(texture,
        (color & COLOR_CHANNEL_RED) >> COLOR_BITSHIFT_RED,
        (color & COLOR_CHANNEL_GREEN) >> COLOR_BITSHIFT_GREEN,
        (color & COLOR_CHANNEL_BLUE) >> COLOR_BITSHIFT_BLUE);
    SDL_SetTextureAlphaMod(texture, (color & COLOR_CHANNEL_ALPHA) >> COLOR_BITSHIFT_ALPHA);

#ifdef USE_TEXTURE_SCALE_MODE
    if (!HAS_TEXTURE_SCALE_MODE) {
        return;
    }
    SDL_ScaleMode current_scale_mode;
    SDL_GetTextureScaleMode(texture, &current_scale_mode);

    SDL_ScaleMode desired_scale_mode = use_linear_filter ? SDL_ScaleModeLinear : SDL_ScaleModeNearest;
    if (current_scale_mode != desired_scale_mode) {
        SDL_SetTextureScaleMode(texture, desired_scale_mode);
    }
#endif
}

static float request_logical_width(const render_2d_request *request, const image *img)
{
    return request->logical_width > 0.0f ? request->logical_width : (float) img->width;
}

static float request_logical_height(const render_2d_request *request, const image *img)
{
    return request->logical_height > 0.0f ? request->logical_height : (float) img->height;
}

static void draw_texture_request(const render_2d_request *request, SDL_Texture *texture, int use_atlas_coords)
{
    if (data.paused || !request || !request->img || !texture) {
        return;
    }

    const image *img = request->img;
    render_domain previous_domain = data.active_render_domain;
    float previous_scale_x = 1.0f;
    float previous_scale_y = 1.0f;
    int should_restore_scale = previous_domain != request->domain;
    if (should_restore_scale) {
        SDL_RenderGetScale(data.renderer, &previous_scale_x, &previous_scale_y);
        set_render_domain(request->domain);
    }

    float source_scale_x = g_render_2d_pipeline.source_scale_x(*request, *img);
    float source_scale_y = g_render_2d_pipeline.source_scale_y(*request, *img);
    float logical_width = request_logical_width(request, img);
    float logical_height = request_logical_height(request, img);
    int use_linear_filter = g_render_2d_pipeline.should_use_linear_filter(
        *request, *img, data.city_scale, data.disable_linear_filter);
    set_texture_color_and_filter(texture, request->color, use_linear_filter);
    const image_handle handle = request_image_handle(request);
    const int uses_managed_texture = handle > 0 && get_managed_texture(handle) == texture;

    float x = request->x + (img->x_offset / source_scale_x);
    float y = request->y + (img->y_offset / source_scale_y);
    int is_city_scale = fabsf(source_scale_x - data.city_scale) < 0.001f && fabsf(source_scale_y - data.city_scale) < 0.001f;
    int src_correction = uses_managed_texture ? 0 : is_city_scale && data.should_correct_texture_offset ? 1 : 0;

    SDL_Rect src_coords = {
        (use_atlas_coords && !uses_managed_texture ? img->atlas.x_offset : 0) + src_correction,
        (use_atlas_coords && !uses_managed_texture ? img->atlas.y_offset : 0) + src_correction,
        img->width - src_correction, img->height - src_correction
    };

    // When zooming out, instead of drawing the grid image, we reduce the isometric textures' size,
    // which ends up simulating a grid without any performance penalty
    int grid_correction = (img->is_isometric && config_get(CONFIG_UI_SHOW_GRID) && data.city_scale > 2.0f) ?
        2 : -src_correction;
    float grid_correction_x = grid_correction / source_scale_x;
    float grid_correction_y = grid_correction / source_scale_y;
    float dst_width = logical_width - grid_correction_x;
    float dst_height = logical_height - grid_correction_y;

#ifdef USE_RENDERCOPYF
    if (HAS_RENDERCOPYF) {
        SDL_FRect dst_coords = {
            x + grid_correction_x,
            y + grid_correction_y,
            dst_width,
            dst_height
        };
        SDL_RenderCopyExF(data.renderer, texture, &src_coords, &dst_coords, request->angle, NULL, SDL_FLIP_NONE);
    } else
#endif
    {
        SDL_Rect dst_coords = {
            (int) round(x + grid_correction_x),
            (int) round(y + grid_correction_y),
            (int) round(dst_width),
            (int) round(dst_height)
        };
        SDL_RenderCopyEx(data.renderer, texture, &src_coords, &dst_coords, request->angle, NULL, SDL_FLIP_NONE);
    }

    if (should_restore_scale) {
        data.active_render_domain = previous_domain;
        SDL_RenderSetScale(data.renderer, previous_scale_x, previous_scale_y);
    }
}

static void draw_image_request(const render_2d_request *request)
{
    if (!request || !request->img) {
        return;
    }

    SDL_Texture *texture = 0;
    if (request->use_silhouette) {
        texture = get_silhouette_texture(request->img);
    } else {
        texture = get_managed_texture(request_image_handle(request));
        if (!texture) {
            texture = get_texture(request->img->atlas.id);
        }
    }
    if (!texture) {
        return;
    }
    draw_texture_request(request, texture, request->use_silhouette ? 0 : 1);
}

static void draw_texture_advanced(const image *img, float x, float y, color_t color,
    float scale_x, float scale_y, double angle, int disable_coord_scaling)
{
    render_2d_request request = {};
    request.img = img;
    request.handle = img ? img->resource_handle : 0;
    request.x = x;
    request.y = y;
    request.logical_width = scale_x ? img->width / scale_x : (float) img->width;
    request.logical_height = scale_y ? img->height / scale_y : (float) img->height;
    request.color = color;
    request.domain = data.active_render_domain;
    request.scaling_policy = is_pixel_domain(request.domain) ? RENDER_SCALING_POLICY_PIXEL_ART : RENDER_SCALING_POLICY_AUTO;
    request.angle = angle;
    request.disable_coord_scaling = disable_coord_scaling;
    draw_image_request(&request);
}

static void draw_texture(const image *img, int x, int y, color_t color, float scale)
{
    render_2d_request request = {};
    request.img = img;
    request.handle = img ? img->resource_handle : 0;
    request.x = scale ? x / scale : (float) x;
    request.y = scale ? y / scale : (float) y;
    request.logical_width = scale ? img->width / scale : (float) img->width;
    request.logical_height = scale ? img->height / scale : (float) img->height;
    request.color = color;
    request.domain = data.active_render_domain;
    request.scaling_policy = is_pixel_domain(request.domain) ? RENDER_SCALING_POLICY_PIXEL_ART : RENDER_SCALING_POLICY_AUTO;
    draw_image_request(&request);
}

static void create_custom_texture(custom_image_type type, int width, int height, int is_yuv)
{
    if (data.paused) {
        return;
    }
    if (data.custom_textures[type].texture) {
        SDL_DestroyTexture(data.custom_textures[type].texture);
        data.custom_textures[type].texture = 0;
    }
    memset(&data.custom_textures[type].img, 0, sizeof(data.custom_textures[type].img));
#ifndef __vita__
    if (data.custom_textures[type].buffer) {
        free(data.custom_textures[type].buffer);
        data.custom_textures[type].buffer = 0;
    }
#endif

    data.custom_textures[type].texture = SDL_CreateTexture(data.renderer,
        is_yuv ? SDL_PIXELFORMAT_YV12 : SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    data.custom_textures[type].img.width = width;
    data.custom_textures[type].img.height = height;
    data.custom_textures[type].img.atlas.id = (ATLAS_CUSTOM << IMAGE_ATLAS_BIT_OFFSET) | type;
    SDL_SetTextureBlendMode(data.custom_textures[type].texture,
        type == CUSTOM_IMAGE_VIDEO ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
}

static color_t *get_custom_texture_buffer(custom_image_type type, int *actual_texture_width)
{
    if (data.paused || !data.custom_textures[type].texture) {
        return 0;
    }

#ifdef __vita__
    int pitch;
    SDL_LockTexture(data.custom_textures[type].texture, NULL, (void **) &data.custom_textures[type].buffer, &pitch);
    if (actual_texture_width) {
        *actual_texture_width = pitch / sizeof(color_t);
    }
    SDL_UnlockTexture(data.custom_textures[type].texture);
#else
    free(data.custom_textures[type].buffer);
    int width, height;
    Uint32 format;
    SDL_QueryTexture(data.custom_textures[type].texture, &format, NULL, &width, &height);
    if (format == SDL_PIXELFORMAT_YV12) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot get buffer to YUV texture");
        return 0;
    }
    data.custom_textures[type].buffer = (color_t *) malloc((size_t) width * height * sizeof(color_t));
    if (actual_texture_width) {
        *actual_texture_width = width;
    }
#endif
    return data.custom_textures[type].buffer;
}

static void release_custom_texture_buffer(custom_image_type type)
{
#ifndef __vita__
    free(data.custom_textures[type].buffer);
    data.custom_textures[type].buffer = 0;
#endif
}

static void update_custom_texture(custom_image_type type)
{
#ifndef __vita__
    if (data.paused || !data.custom_textures[type].texture || !data.custom_textures[type].buffer) {
        return;
    }
    int width;
    SDL_QueryTexture(data.custom_textures[type].texture, NULL, NULL, &width, NULL);
    SDL_UpdateTexture(data.custom_textures[type].texture, NULL,
        data.custom_textures[type].buffer, sizeof(color_t) * width);
#endif
}

static void update_custom_texture_from(custom_image_type type, const color_t *buffer,
    int x_offset, int y_offset, int width, int height)
{
    if (data.paused || !data.custom_textures[type].texture) {
        return;
    }
    int texture_width, texture_height;
    SDL_QueryTexture(data.custom_textures[type].texture, NULL, NULL, &texture_width, &texture_height);
    if (x_offset + width > texture_width || y_offset + height > texture_height) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Partial texture copy goes out of bounds");
        return;
    }
#ifdef __vita__
    int pitch;
    SDL_LockTexture(data.custom_textures[type].texture, NULL, (void **) &data.custom_textures[type].buffer, &pitch);
    texture_width = pitch / sizeof(color_t);
    color_t *offset = &data.custom_textures[type].buffer[y_offset * texture_width + x_offset];
    for (int y = 0; y < height; y++) {
        memcpy(&offset[y * texture_width], &buffer[y * width], width * sizeof(color_t));
    }
    SDL_UnlockTexture(data.custom_textures[type].texture);
#else
    SDL_Rect rect = { x_offset, y_offset, width, height };
    SDL_UpdateTexture(data.custom_textures[type].texture, &rect, buffer, sizeof(color_t) * width);
#endif
}

static void update_custom_texture_yuv(custom_image_type type, const uint8_t *y_data, int y_width,
    const uint8_t *cb_data, int cb_width, const uint8_t *cr_data, int cr_width)
{
#ifdef USE_YUV_TEXTURES
    if (data.paused || !data.supports_yuv_textures || !data.custom_textures[type].texture) {
        return;
    }
    int width, height;
    Uint32 format;
    SDL_QueryTexture(data.custom_textures[type].texture, &format, NULL, &width, &height);
    if (format != SDL_PIXELFORMAT_YV12) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Texture is not YUV format");
        return;
    }
    SDL_UpdateYUVTexture(data.custom_textures[type].texture, NULL,
        y_data, y_width, cb_data, cb_width, cr_data, cr_width);
#endif
}

static int start_tooltip_creation_for_domain(render_domain domain, int width, int height)
{
    if (data.paused) {
        return 0;
    }
    save_render_state(&tooltip_render_state);
    tooltip_render_state_valid = 1;

    int texture_width = g_render_2d_pipeline.scale_logical_size(domain, width, platform_screen_get_scale());
    int texture_height = g_render_2d_pipeline.scale_logical_size(domain, height, platform_screen_get_scale());
    if (data.tooltip.texture) {
        if (data.tooltip.texture_width < texture_width || data.tooltip.texture_height < texture_height) {
            SDL_DestroyTexture(data.tooltip.texture);
            data.tooltip.texture = 0;
        }
    }
    if (!data.tooltip.texture) {
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, configured_scale_quality_hint());

        data.tooltip.texture = SDL_CreateTexture(data.renderer, SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_TARGET, texture_width, texture_height);
        if (!data.tooltip.texture) {
            restore_render_state(&tooltip_render_state);
            tooltip_render_state_valid = 0;
            return 0;
        }
        SDL_SetTextureBlendMode(data.tooltip.texture, SDL_BLENDMODE_BLEND);
#ifdef USE_TEXTURE_SCALE_MODE
        if (HAS_TEXTURE_SCALE_MODE) {
            SDL_SetTextureScaleMode(data.tooltip.texture, configured_scale_mode());
        }
#endif
        data.tooltip.texture_width = texture_width;
        data.tooltip.texture_height = texture_height;
    }
    data.tooltip.width = texture_width;
    data.tooltip.height = texture_height;
    if (SDL_SetRenderTarget(data.renderer, data.tooltip.texture) != 0) {
        restore_render_state(&tooltip_render_state);
        tooltip_render_state_valid = 0;
        return 0;
    }

    set_render_domain(domain);
    SDL_Rect viewport = { 0, 0, width, height };
    SDL_RenderSetViewport(data.renderer, &viewport);
    SDL_RenderSetClipRect(data.renderer, NULL);
    SDL_SetRenderDrawColor(data.renderer, 0, 0, 0, 0);
    SDL_RenderClear(data.renderer);
    return 1;
}

static int start_tooltip_creation(int width, int height)
{
    render_domain domain = g_render_2d_pipeline.tooltip_domain_for(data.active_render_domain);
    return start_tooltip_creation_for_domain(domain, width, height);
}

static void finish_tooltip_creation(void)
{
    if (data.paused) {
        return;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, configured_scale_quality_hint());
    if (tooltip_render_state_valid) {
        restore_render_state(&tooltip_render_state);
        tooltip_render_state_valid = 0;
    } else {
        SDL_SetRenderTarget(data.renderer, data.render_texture);
    }
}

static int has_tooltip(void)
{
    return data.tooltip.texture != 0;
}

static void set_tooltip_position_for_domain(render_domain domain, int x, int y)
{
    float scale = scale_for_domain(domain);
    data.tooltip.x = (int) roundf(x * scale);
    data.tooltip.y = (int) roundf(y * scale);
}

static void set_tooltip_position(int x, int y)
{
    render_domain domain = g_render_2d_pipeline.tooltip_domain_for(data.active_render_domain);
    set_tooltip_position_for_domain(domain, x, y);
}

static void set_tooltip_opacity(int opacity)
{
    data.tooltip.opacity = calc_adjust_with_percentage(255, opacity);
}

static buffer_texture *get_saved_texture_info(int texture_id)
{
    if (!texture_id || !data.texture_buffers.first) {
        return 0;
    }
    for (buffer_texture *texture_info = data.texture_buffers.first; texture_info; texture_info = texture_info->next) {
        if (texture_info->id == texture_id) {
            return texture_info;
        }
    }
    return 0;
}

static int save_to_texture_for_domain(render_domain domain, int texture_id, int x, int y, int width, int height)
{
    if (data.paused) {
        return 0;
    }
    render_state current_state;
    save_render_state(&current_state);
    if (!current_state.target) {
        return 0;
    }

    float scale = scale_for_domain(domain);
    int texture_width = (int) roundf(width * scale);
    int texture_height = (int) roundf(height * scale);

    buffer_texture *texture_info = get_saved_texture_info(texture_id);
    SDL_Texture *texture = 0;

    if (!texture_info || (texture_info && (texture_info->tex_width < texture_width || texture_info->tex_height < texture_height))) {
        if (texture_info) {
            SDL_DestroyTexture(texture_info->texture);
            texture_info->texture = 0;
            texture_info->tex_width = 0;
            texture_info->tex_height = 0;
        }
        texture = SDL_CreateTexture(data.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, texture_width, texture_height);
        if (!texture) {
            return 0;
        }
#ifdef USE_TEXTURE_SCALE_MODE
        if (HAS_TEXTURE_SCALE_MODE) {
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeNearest);
        }
#endif
    } else {
        texture = texture_info->texture;
    }

    SDL_Rect src_rect = {
        current_state.viewport.x + (int) roundf(x * scale),
        current_state.viewport.y + (int) roundf(y * scale),
        texture_width,
        texture_height
    };
    SDL_Rect dst_rect = { 0, 0, texture_width, texture_height };
    SDL_SetRenderTarget(data.renderer, texture);
    SDL_RenderSetScale(data.renderer, 1.0f, 1.0f);
    SDL_RenderSetViewport(data.renderer, &dst_rect);
    SDL_RenderSetClipRect(data.renderer, NULL);
    SDL_RenderCopy(data.renderer, current_state.target, &src_rect, &dst_rect);
    restore_render_state(&current_state);

    if (!texture_info) {
        texture_info = static_cast<buffer_texture *>(malloc(sizeof(buffer_texture)));

        if (!texture_info) {
            SDL_DestroyTexture(texture);
            return 0;
        }

        memset(texture_info, 0, sizeof(buffer_texture));

        texture_info->id = ++data.texture_buffers.current_id;
        texture_info->next = 0;

        if (!data.texture_buffers.first) {
            data.texture_buffers.first = texture_info;
        } else {
            data.texture_buffers.last->next = texture_info;
        }
        data.texture_buffers.last = texture_info;
    }
    texture_info->texture = texture;
    texture_info->width = width;
    texture_info->height = height;
    texture_info->raw_width = texture_width;
    texture_info->raw_height = texture_height;
    if (texture_width > texture_info->tex_width) {
        texture_info->tex_width = texture_width;
    }
    if (texture_height > texture_info->tex_height) {
        texture_info->tex_height = texture_height;
    }

    return texture_info->id;
}

static int save_to_texture(int texture_id, int x, int y, int width, int height)
{
    render_domain domain = g_render_2d_pipeline.snapshot_domain_for(data.active_render_domain);
    return save_to_texture_for_domain(domain, texture_id, x, y, width, height);
}

static void draw_saved_texture_for_domain(render_domain domain, int texture_id, int x, int y)
{
    if (data.paused) {
        return;
    }
    buffer_texture *texture_info = get_saved_texture_info(texture_id);
    if (!texture_info) {
        return;
    }
    render_domain previous_domain = data.active_render_domain;
    float previous_scale_x = 1.0f;
    float previous_scale_y = 1.0f;
    int should_restore_scale = previous_domain != domain;
    if (should_restore_scale) {
        SDL_RenderGetScale(data.renderer, &previous_scale_x, &previous_scale_y);
        set_render_domain(domain);
    }
    SDL_Rect src_coords = { 0, 0, texture_info->raw_width, texture_info->raw_height };
    SDL_Rect dst_coords = { x, y, texture_info->width, texture_info->height };
    SDL_RenderCopy(data.renderer, texture_info->texture, &src_coords, &dst_coords);
    if (should_restore_scale) {
        data.active_render_domain = previous_domain;
        SDL_RenderSetScale(data.renderer, previous_scale_x, previous_scale_y);
    }
}

static void draw_saved_texture(int texture_id, int x, int y)
{
    render_domain domain = g_render_2d_pipeline.snapshot_domain_for(data.active_render_domain);
    draw_saved_texture_for_domain(domain, texture_id, x, y);
}

static void create_blend_texture(custom_image_type type)
{
    SDL_Texture *texture = SDL_CreateTexture(data.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, 58, 30);
    if (!texture) {
        return;
    }
    const image *img = image_get(image_group(GROUP_TERRAIN_FLAT_TILE));
    if (!img) {
        SDL_DestroyTexture(texture);
        return;
    }
    SDL_Texture *flat_tile = get_managed_texture(img->resource_handle);
    if (!flat_tile) {
        flat_tile = get_texture(img->atlas.id);
    }
    if (!flat_tile) {
        SDL_DestroyTexture(texture);
        return;
    }
    render_state current_state;
    save_render_state(&current_state);

    SDL_SetRenderTarget(data.renderer, texture);
    SDL_Rect rect = { 0, 0, 58, 30 };
    SDL_RenderSetScale(data.renderer, 1.0f, 1.0f);
    SDL_RenderSetClipRect(data.renderer, &rect);
    SDL_RenderSetViewport(data.renderer, &rect);
    SDL_SetRenderDrawColor(data.renderer, 0xff, 0xff, 0xff, 0xff);
    color_t color = type == CUSTOM_IMAGE_RED_FOOTPRINT ? COLOR_MASK_RED : COLOR_MASK_GREEN;
    SDL_RenderClear(data.renderer);
    SDL_SetTextureBlendMode(flat_tile, SDL_BLENDMODE_BLEND);

    SDL_SetTextureColorMod(flat_tile,
        (color & COLOR_CHANNEL_RED) >> COLOR_BITSHIFT_RED,
        (color & COLOR_CHANNEL_GREEN) >> COLOR_BITSHIFT_GREEN,
        (color & COLOR_CHANNEL_BLUE) >> COLOR_BITSHIFT_BLUE);
    SDL_SetTextureAlphaMod(flat_tile, 0xff);
    SDL_Rect src_coords = { img && img->resource_handle ? 0 : img->atlas.x_offset,
        img && img->resource_handle ? 0 : img->atlas.y_offset, img->width, img->height };
    SDL_RenderCopy(data.renderer, flat_tile, &src_coords, 0);

    restore_render_state(&current_state);

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_MOD);

    data.custom_textures[type].texture = texture;
    memset(&data.custom_textures[type].img, 0, sizeof(data.custom_textures[type].img));
    data.custom_textures[type].img.is_isometric = 1;
    data.custom_textures[type].img.width = 58;
    data.custom_textures[type].img.height = 30;
    data.custom_textures[type].img.atlas.id = (ATLAS_CUSTOM << IMAGE_ATLAS_BIT_OFFSET) | type;
}

static SDL_Texture *get_silhouette_texture(const image *img)
{
    if (data.paused || !img) {
        return 0;
    }
    silhouette_texture *last_silhouette = 0;

    for (silhouette_texture *silhouette = data.silhouettes; silhouette; silhouette = silhouette->next) {
        last_silhouette = silhouette;
        if (silhouette->img == img) {
            return silhouette->texture;
        }
    }
    SDL_Texture *texture = SDL_CreateTexture(data.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET,
        img->width, img->height);
    if (!texture) {
        return 0;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_Texture *original_texture = get_managed_texture(img->resource_handle);
    if (!original_texture) {
        original_texture = get_texture(img->atlas.id);
    }
    if (!original_texture) {
        SDL_DestroyTexture(texture);
        return 0;
    }
    render_state current_state;
    save_render_state(&current_state);

    SDL_SetRenderTarget(data.renderer, texture);
    SDL_Rect rect = { 0, 0, img->width, img->height };
    SDL_RenderSetScale(data.renderer, 1.0f, 1.0f);
    SDL_RenderSetClipRect(data.renderer, &rect);
    SDL_RenderSetViewport(data.renderer, &rect);
    SDL_SetRenderDrawBlendMode(data.renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(data.renderer, 0, 0, 0, 0);
    SDL_RenderClear(data.renderer);

    SDL_Rect src_coords = { img && img->resource_handle ? 0 : img->atlas.x_offset,
        img && img->resource_handle ? 0 : img->atlas.y_offset, img->width, img->height };

    set_texture_color_and_filter(original_texture, 0, 0);

    SDL_RenderCopy(data.renderer, original_texture, &src_coords, 0);

    SDL_SetRenderDrawBlendMode(data.renderer, SDL_BLENDMODE_ADD);

    SDL_SetRenderDrawColor(data.renderer, 0xff, 0xff, 0xff, 0xff);
    SDL_RenderFillRect(data.renderer, 0);

    SDL_SetRenderDrawBlendMode(data.renderer, SDL_BLENDMODE_MOD);

    // We want the same color as the "flat tile" image
    SDL_SetRenderDrawColor(data.renderer, 0xd6, 0xf3, 0xd6, 0xff);
    SDL_RenderFillRect(data.renderer, 0);

    // Copy our created texture to a surface and then to a new texture, to get rid of the "target texture" issues
    SDL_Surface *surface = SDL_CreateRGBSurface(0, img->width, img->height, 32,
        0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
    if (!surface) {
        restore_render_state(&current_state);
        SDL_DestroyTexture(texture);
        return 0;
    }
    SDL_RenderReadPixels(data.renderer, 0, 0, surface->pixels, surface->pitch);

    restore_render_state(&current_state);

    SDL_Texture *final_texture = SDL_CreateTextureFromSurface(data.renderer, surface);
    SDL_FreeSurface(surface);

    SDL_DestroyTexture(texture);

    if (!final_texture) {
        return 0;
    }

    silhouette_texture *new_silhouette = static_cast<silhouette_texture *>(malloc(sizeof(silhouette_texture)));
    if (!new_silhouette) {
        SDL_DestroyTexture(final_texture);
        return 0;
    }
    new_silhouette->img = img;
    new_silhouette->texture = final_texture;
    new_silhouette->next = 0;

    if (last_silhouette) {
        last_silhouette->next = new_silhouette;
    } else {
        data.silhouettes = new_silhouette;
    }

    return final_texture;
}

static void draw_silhouetted_texture(const image *img, int x, int y, color_t color, float scale)
{
    render_2d_request request = {};
    request.img = img;
    request.handle = img ? img->resource_handle : 0;
    request.x = scale ? x / scale : (float) x;
    request.y = scale ? y / scale : (float) y;
    request.logical_width = scale ? img->width / scale : (float) img->width;
    request.logical_height = scale ? img->height / scale : (float) img->height;
    request.color = color;
    request.domain = data.active_render_domain;
    request.scaling_policy = is_pixel_domain(request.domain) ? RENDER_SCALING_POLICY_PIXEL_ART : RENDER_SCALING_POLICY_AUTO;
    request.use_silhouette = 1;
    draw_image_request(&request);
}

static void draw_custom_texture(custom_image_type type, int x, int y, float scale, int disable_filtering)
{
    if (data.paused) {
        return;
    }
    if (type == CUSTOM_IMAGE_RED_FOOTPRINT || type == CUSTOM_IMAGE_GREEN_FOOTPRINT) {
        if (!data.custom_textures[type].texture) {
            create_blend_texture(type);
        }
    }
    data.disable_linear_filter = disable_filtering;
    draw_texture(&data.custom_textures[type].img, x, y, 0, scale);
    data.disable_linear_filter = 0;
}

static void release_image_resource(image *img)
{
    if (!img || img->resource_handle <= 0) {
        return;
    }
    if (img->resource_handle < static_cast<image_handle>(data.image_resources.size())) {
        managed_image_resource &resource = data.image_resources[img->resource_handle];
        if (resource.texture) {
            SDL_DestroyTexture(resource.texture);
            resource.texture = 0;
        }
        resource.width = 0;
        resource.height = 0;
    }
    img->resource_handle = 0;
}

static void upload_image_resource(image *img, const color_t *pixels, int width, int height)
{
    if (!img) {
        return;
    }
    release_image_resource(img);
    if (data.paused || !pixels || width <= 0 || height <= 0) {
        return;
    }
    SDL_Texture *texture = create_texture_from_pixels(pixels, width, height);
    if (!texture) {
        return;
    }
    image_handle handle = reserve_managed_image_resource_slot();
    managed_image_resource &resource = data.image_resources[handle];
    resource.texture = texture;
    resource.width = width;
    resource.height = height;
    img->resource_handle = handle;
}

static int has_custom_texture(custom_image_type type)
{
    return data.custom_textures[type].texture != 0;
}

static void load_unpacked_image(const image *img, const color_t *pixels)
{
    if (data.paused) {
        return;
    }
    int first_empty = -1;
    int oldest_texture_index = 0;
    int unpacked_image_id = img->atlas.id & IMAGE_ATLAS_BIT_MASK;
    for (int i = 0; i < MAX_UNPACKED_IMAGES; i++) {
        if (data.unpacked_images[i].id == unpacked_image_id && data.unpacked_images[i].texture) {
            return;
        }
        if (first_empty == -1 && !data.unpacked_images[i].texture) {
            first_empty = i;
            break;
        }
        if (data.unpacked_images[oldest_texture_index].last_used < data.unpacked_images[i].last_used) {
            oldest_texture_index = i;
        }
    }
    int index = first_empty != -1 ? first_empty : oldest_texture_index;

    int image_height = img->height;
    if (img->top) {
        image_height += img->top->height;
    }

    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom((void *) pixels, img->width, image_height, 32,
        img->width * sizeof(color_t), COLOR_CHANNEL_RED, COLOR_CHANNEL_GREEN, COLOR_CHANNEL_BLUE, COLOR_CHANNEL_ALPHA);
    if (!surface) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create surface for texture. Reason: %s", SDL_GetError());
        return;
    }
    data.unpacked_images[index].last_used = time_get_millis();
    data.unpacked_images[index].id = unpacked_image_id;

    if (data.unpacked_images[index].texture) {
        SDL_DestroyTexture(data.unpacked_images[index].texture);
        data.unpacked_images[index].texture = 0;
    }
    data.unpacked_images[index].texture = SDL_CreateTextureFromSurface(data.renderer, surface);
    while (!data.unpacked_images[index].texture) {
        oldest_texture_index = -1;
        for (int i = 0; i < MAX_UNPACKED_IMAGES; i++) {
            if (data.unpacked_images[i].texture &&
                (oldest_texture_index == -1 ||
                data.unpacked_images[oldest_texture_index].last_used < data.unpacked_images[i].last_used)) {
                oldest_texture_index = i;
            }
        }
        if (oldest_texture_index == -1) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create surface for texture - %s", SDL_GetError());
            SDL_FreeSurface(surface);
            return;
        }
        SDL_DestroyTexture(data.unpacked_images[oldest_texture_index].texture);
        data.unpacked_images[oldest_texture_index].texture = 0;
        data.unpacked_images[index].texture = SDL_CreateTextureFromSurface(data.renderer, surface);
    }
    SDL_SetTextureBlendMode(data.unpacked_images[index].texture, SDL_BLENDMODE_BLEND);
    SDL_FreeSurface(surface);
}

static void free_unpacked_image(const image *img)
{
    int unpacked_image_id = img->atlas.id & IMAGE_ATLAS_BIT_MASK;
    int found_id = -1;
    for (int i = 0; i < MAX_UNPACKED_IMAGES; i++) {
        if (data.unpacked_images[i].id == unpacked_image_id && data.unpacked_images[i].texture) {
            found_id = i;
            break;
        }
    }
    if (found_id == -1) {
        return;
    }
    if (data.unpacked_images[found_id].texture) {
        SDL_DestroyTexture(data.unpacked_images[found_id].texture);
    }
    memset(&data.unpacked_images[found_id], 0, sizeof(data.unpacked_images[found_id]));
}

static int should_pack_image(int width, int height)
{
    return width * height < MAX_PACKED_IMAGE_SIZE;
}

static void update_scale(int city_scale)
{
    // The renderer draws the textures off-by-one when "scale * 100" is a multiple of 8, or when zooming out enough,
    // this fixes that rendering bug by properly offseting the textures
    data.should_correct_texture_offset = (city_scale > 250 && (city_scale % 100) != 0) || (city_scale % 8) == 0;
    data.city_scale = city_scale / 100.0f;
}

static int supports_yuv_texture(void)
{
    return data.supports_yuv_textures;
}

static void create_renderer_interface(void)
{
    data.renderer_interface.clear_screen = clear_screen;
    data.renderer_interface.set_viewport = set_viewport;
    data.renderer_interface.reset_viewport = reset_viewport;
    data.renderer_interface.set_clip_rectangle = set_clip_rectangle;
    data.renderer_interface.reset_clip_rectangle = reset_clip_rectangle;
    data.renderer_interface.draw_line = draw_line;
    data.renderer_interface.draw_rect = draw_rect;
    data.renderer_interface.fill_rect = fill_rect;
    data.renderer_interface.set_output_scale = set_output_scale;
    data.renderer_interface.set_render_domain = set_render_domain;
    data.renderer_interface.get_render_domain = get_render_domain;
    data.renderer_interface.push_state = push_render_state;
    data.renderer_interface.pop_state = pop_render_state;
    data.renderer_interface.draw_image = draw_texture;
    data.renderer_interface.draw_image_request = draw_image_request;
    data.renderer_interface.draw_managed_image_request = draw_managed_image_request;
    data.renderer_interface.draw_image_advanced = draw_texture_advanced;
    data.renderer_interface.draw_silhouette = draw_silhouetted_texture;
    data.renderer_interface.create_custom_image = create_custom_texture;
    data.renderer_interface.has_custom_image = has_custom_texture;
    data.renderer_interface.get_custom_image_buffer = get_custom_texture_buffer;
    data.renderer_interface.release_custom_image_buffer = release_custom_texture_buffer;
    data.renderer_interface.update_custom_image = update_custom_texture;
    data.renderer_interface.update_custom_image_from = update_custom_texture_from;
    data.renderer_interface.update_custom_image_yuv = update_custom_texture_yuv;
    data.renderer_interface.draw_custom_image = draw_custom_texture;
    data.renderer_interface.supports_yuv_image_format = supports_yuv_texture;
    data.renderer_interface.start_tooltip_creation = start_tooltip_creation;
    data.renderer_interface.start_tooltip_creation_for_domain = start_tooltip_creation_for_domain;
    data.renderer_interface.finish_tooltip_creation = finish_tooltip_creation;
    data.renderer_interface.set_tooltip_position = set_tooltip_position;
    data.renderer_interface.set_tooltip_position_for_domain = set_tooltip_position_for_domain;
    data.renderer_interface.set_tooltip_opacity = set_tooltip_opacity;
    data.renderer_interface.has_tooltip = has_tooltip;
    data.renderer_interface.save_image_from_screen = save_to_texture;
    data.renderer_interface.save_image_from_screen_for_domain = save_to_texture_for_domain;
    data.renderer_interface.draw_image_to_screen = draw_saved_texture;
    data.renderer_interface.draw_image_to_screen_for_domain = draw_saved_texture_for_domain;
    data.renderer_interface.save_screen_buffer = save_screen_buffer;
    data.renderer_interface.get_max_image_size = get_max_image_size;
    data.renderer_interface.prepare_image_atlas = prepare_texture_atlas;
    data.renderer_interface.create_image_atlas = create_texture_atlas;
    data.renderer_interface.get_image_atlas = get_texture_atlas;
    data.renderer_interface.has_image_atlas = has_texture_atlas;
    data.renderer_interface.free_image_atlas = free_texture_atlas_and_data;
    data.renderer_interface.load_unpacked_image = load_unpacked_image;
    data.renderer_interface.free_unpacked_image = free_unpacked_image;
    data.renderer_interface.upload_image_resource = upload_image_resource;
    data.renderer_interface.release_image_resource = release_image_resource;
    data.renderer_interface.should_pack_image = should_pack_image;
    data.renderer_interface.update_scale = update_scale;

    graphics_renderer_set_interface(&data.renderer_interface);
}

int platform_renderer_init(SDL_Window *window)
{
    free_all_textures();

    SDL_Log("Creating renderer");
    data.renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    if (!data.renderer) {
        SDL_Log("Unable to create renderer, trying software renderer: %s", SDL_GetError());
        data.renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!data.renderer) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create renderer: %s", SDL_GetError());
            return 0;
        }
    }

    SDL_RendererInfo info;
    SDL_GetRendererInfo(data.renderer, &info);
    SDL_Log("Loaded renderer: %s", info.name);

#ifdef USE_YUV_TEXTURES
    if (!data.supports_yuv_textures && HAS_YUV_TEXTURES) {
        for (unsigned int i = 0; i < info.num_texture_formats; i++) {
            if (info.texture_formats[i] == SDL_PIXELFORMAT_YV12) {
                data.supports_yuv_textures = 1;
                break;
            }
        }
    }
#endif

    data.is_software_renderer = info.flags & SDL_RENDERER_SOFTWARE;
    if (data.is_software_renderer) {
        data.max_texture_size.width = 4096;
        data.max_texture_size.height = 4096;
    } else {
        data.max_texture_size.width = info.max_texture_width;
        data.max_texture_size.height = info.max_texture_height;
    }
    data.paused = 0;
    data.active_render_domain = RENDER_DOMAIN_UI;
   
#ifdef MAX_TEXTURE_SIZE
#ifdef __EMSCRIPTEN__
    int is_android = EM_ASM_INT(return navigator.userAgent.toLowerCase().indexOf("android") > -1);
    if (is_android) {
#endif
        if (data.max_texture_size.width > MAX_TEXTURE_SIZE) {
            data.max_texture_size.width = MAX_TEXTURE_SIZE;
        }
        if (data.max_texture_size.height > MAX_TEXTURE_SIZE) {
            data.max_texture_size.height = MAX_TEXTURE_SIZE;
        }
#ifdef __EMSCRIPTEN__
    }
#endif
#endif

    SDL_SetRenderDrawColor(data.renderer, 0, 0, 0, 0xff);

    create_renderer_interface();

    return 1;
}

static void destroy_render_texture(void)
{
    if (data.render_texture) {
        SDL_DestroyTexture(data.render_texture);
        data.render_texture = 0;
    }
    if (data.tooltip.texture) {
        SDL_DestroyTexture(data.tooltip.texture);
        data.tooltip.texture = 0;
    }
}

int platform_renderer_create_render_texture(int width, int height)
{
    if (data.paused) {
        return 1;
    }
    destroy_render_texture();

#ifdef USE_TEXTURE_SCALE_MODE
    if (!HAS_TEXTURE_SCALE_MODE) {
#endif
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, configured_scale_quality_hint());
#ifdef USE_TEXTURE_SCALE_MODE
    }
#endif

    SDL_SetRenderTarget(data.renderer, NULL);

    data.render_texture = SDL_CreateTexture(data.renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, width, height);
    if (data.render_texture) {
        SDL_Log("Render texture created (%d x %d)", width, height);
        SDL_SetRenderTarget(data.renderer, data.render_texture);
        set_render_domain(data.active_render_domain);
        SDL_SetRenderDrawBlendMode(data.renderer, SDL_BLENDMODE_BLEND);

#ifdef USE_TEXTURE_SCALE_MODE
        if (HAS_TEXTURE_SCALE_MODE) {
            SDL_SetTextureScaleMode(data.render_texture, configured_scale_mode());
        } else {
#endif
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, configured_scale_quality_hint());
#ifdef USE_TEXTURE_SCALE_MODE
        }
#endif

        return 1;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to create render texture: %s", SDL_GetError());
        return 0;
    }
}

int platform_renderer_lost_render_texture(void)
{
    return !data.render_texture && data.renderer;
}

void platform_renderer_invalidate_target_textures(void)
{
    if (data.custom_textures[CUSTOM_IMAGE_RED_FOOTPRINT].texture) {
        SDL_DestroyTexture(data.custom_textures[CUSTOM_IMAGE_RED_FOOTPRINT].texture);
        data.custom_textures[CUSTOM_IMAGE_RED_FOOTPRINT].texture = 0;
        create_blend_texture(CUSTOM_IMAGE_RED_FOOTPRINT);
    }
    if (data.custom_textures[CUSTOM_IMAGE_GREEN_FOOTPRINT].texture) {
        SDL_DestroyTexture(data.custom_textures[CUSTOM_IMAGE_GREEN_FOOTPRINT].texture);
        data.custom_textures[CUSTOM_IMAGE_GREEN_FOOTPRINT].texture = 0;
        create_blend_texture(CUSTOM_IMAGE_GREEN_FOOTPRINT);
    }
    if (data.tooltip.texture) {
        SDL_DestroyTexture(data.tooltip.texture);
        data.tooltip.texture = 0;
    }
}

void platform_renderer_clear(void)
{
    clear_screen();
}

static void draw_tooltip(void)
{
    if (!data.tooltip.texture || !data.tooltip.opacity) {
        return;
    }
    color_t opacity = calc_adjust_with_percentage(0x55, calc_percentage(data.tooltip.opacity, 0xff));
    fill_rect(data.tooltip.x + 2, data.tooltip.width,
        data.tooltip.y + 2, data.tooltip.height, opacity << COLOR_BITSHIFT_ALPHA);

    SDL_Rect src;
    src.x = 0;
    src.y = 0;
    src.w = data.tooltip.width;
    src.h = data.tooltip.height;
    SDL_Rect dst;
    dst.x = data.tooltip.x;
    dst.y = data.tooltip.y;
    dst.w = data.tooltip.width;
    dst.h = data.tooltip.height;
    SDL_SetTextureAlphaMod(data.tooltip.texture, data.tooltip.opacity);
    SDL_RenderCopy(data.renderer, data.tooltip.texture, &src, &dst);
}

static void draw_software_mouse_cursor(void)
{
    const mouse *m = mouse_get_pixel();
    if (m->is_touch) {
        return;
    }
    cursor_shape current = platform_cursor_get_current_shape();
    if (current == CURSOR_DISABLED) {
        return;
    }
    int size = calc_adjust_with_percentage(data.cursors[current].size,
        calc_percentage(100, platform_screen_get_scale()));
    SDL_Rect dst;
    dst.x = m->x - data.cursors[current].hotspot.x;
    dst.y = m->y - data.cursors[current].hotspot.y;
    dst.w = size;
    dst.h = size;
    SDL_RenderCopy(data.renderer, data.cursors[current].texture, NULL, &dst);
}

void platform_renderer_render(void)
{
    if (data.paused) {
        return;
    }
    render_domain previous_domain = data.active_render_domain;
    SDL_SetRenderTarget(data.renderer, NULL);
    set_render_domain(RENDER_DOMAIN_PIXEL);
    SDL_RenderCopy(data.renderer, data.render_texture, NULL, NULL);
    draw_tooltip();
    if (platform_cursor_is_software()) {
        draw_software_mouse_cursor();
    }
    SDL_RenderPresent(data.renderer);
    SDL_SetRenderTarget(data.renderer, data.render_texture);
    set_render_domain(previous_domain);
}

void platform_renderer_generate_mouse_cursor_texture(int cursor_id, int size, const color_t *pixels,
    int hotspot_x, int hotspot_y)
{
    if (data.paused) {
        return;
    }
    if (data.cursors[cursor_id].texture) {
        SDL_DestroyTexture(data.cursors[cursor_id].texture);
        SDL_memset(&data.cursors[cursor_id], 0, sizeof(data.cursors[cursor_id]));
    }
    data.cursors[cursor_id].texture = SDL_CreateTexture(data.renderer,
        SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
        size, size);
    if (!data.cursors[cursor_id].texture) {
        return;
    }
    SDL_UpdateTexture(data.cursors[cursor_id].texture, NULL, pixels, size * sizeof(color_t));
    data.cursors[cursor_id].hotspot.x = hotspot_x;
    data.cursors[cursor_id].hotspot.y = hotspot_y;
    data.cursors[cursor_id].size = size;
    SDL_SetTextureBlendMode(data.cursors[cursor_id].texture, SDL_BLENDMODE_BLEND);
}

void platform_renderer_pause(void)
{
    SDL_SetRenderTarget(data.renderer, NULL);
    data.paused = 1;
}

void platform_renderer_resume(void)
{
    data.paused = 0;
    platform_renderer_create_render_texture(screen_pixel_width(), screen_pixel_height());
    SDL_SetRenderTarget(data.renderer, data.render_texture);
    set_render_domain(data.active_render_domain);
}

void platform_renderer_destroy(void)
{
    destroy_render_texture();
    if (data.renderer) {
        SDL_DestroyRenderer(data.renderer);
        data.renderer = 0;
    }
}
