#pragma once

#include "core/image.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ATLAS_FIRST,
    ATLAS_MAIN = ATLAS_FIRST,
    ATLAS_ENEMY,
    ATLAS_FONT,
    ATLAS_EXTRA_ASSET,
    ATLAS_UNPACKED_EXTRA_ASSET,
    ATLAS_CUSTOM,
    ATLAS_EXTERNAL,
    ATLAS_MAX
} atlas_type;

typedef enum {
    CUSTOM_IMAGE_NONE,
    CUSTOM_IMAGE_EXTERNAL,
    CUSTOM_IMAGE_MINIMAP,
    CUSTOM_IMAGE_VIDEO,
    CUSTOM_IMAGE_EMPIRE_MAP,
    CUSTOM_IMAGE_RED_FOOTPRINT,
    CUSTOM_IMAGE_GREEN_FOOTPRINT,
    CUSTOM_IMAGE_CLOUDS,
    CUSTOM_IMAGE_MAX
} custom_image_type;

typedef enum {
    IMAGE_FILTER_NEAREST = 0,
    IMAGE_FILTER_LINEAR = 1
} image_filter;

typedef enum {
    RENDER_DOMAIN_UI = 0,
    RENDER_DOMAIN_PIXEL = 1,
    RENDER_DOMAIN_TOOLTIP_UI = 2,
    RENDER_DOMAIN_TOOLTIP_PIXEL = 3,
    RENDER_DOMAIN_SNAPSHOT_UI = 4,
    RENDER_DOMAIN_SNAPSHOT_PIXEL = 5
} render_domain;

typedef enum {
    RENDER_SCALING_POLICY_AUTO = 0,
    RENDER_SCALING_POLICY_PIXEL_ART = 1,
    RENDER_SCALING_POLICY_HIGH_QUALITY = 2
} render_scaling_policy;

typedef struct {
    const image *img;
    image_handle handle;
    float x;
    float y;
    float logical_width;
    float logical_height;
    color_t color;
    render_domain domain;
    render_scaling_policy scaling_policy;
    double angle;
    int disable_coord_scaling;
    int use_silhouette;
} render_2d_request;

typedef struct {
    atlas_type type;
    int num_images;
    color_t **buffers;
    int *image_widths;
    int *image_heights;
} image_atlas_data;

typedef struct {
    void (*clear_screen)(void);

    void (*set_viewport)(int x, int y, int width, int height);
    void (*reset_viewport)(void);

    void (*set_clip_rectangle)(int x, int y, int width, int height);
    void (*reset_clip_rectangle)(void);

    void (*draw_line)(int x_start, int x_end, int y_start, int y_end, color_t color);
    void (*draw_rect)(int x_start, int x_end, int y_start, int y_end, color_t color);
    void (*fill_rect)(int x_start, int x_end, int y_start, int y_end, color_t color);
    void (*set_output_scale)(float scale);
    void (*set_render_domain)(render_domain domain);
    render_domain (*get_render_domain)(void);
    void (*push_state)(void);
    void (*pop_state)(void);

    void (*draw_image)(const image *img, int x, int y, color_t color, float scale);
    void (*draw_image_request)(const render_2d_request *request);
    void (*draw_image_advanced)(const image *img, float x, float y, color_t color,
        float scale_x, float scale_y, double angle, int disable_coord_scaling);
    void (*draw_silhouette)(const image *img, int x, int y, color_t color, float scale);

    void (*create_custom_image)(custom_image_type type, int width, int height, int is_yuv);
    int (*has_custom_image)(custom_image_type type);
    color_t *(*get_custom_image_buffer)(custom_image_type type, int *actual_texture_width);
    void (*release_custom_image_buffer)(custom_image_type type);
    void (*update_custom_image)(custom_image_type type);
    void (*update_custom_image_from)(custom_image_type type, const color_t *buffer,
        int x_offset, int y_offset, int width, int height);
    void (*update_custom_image_yuv)(custom_image_type type, const uint8_t *y_data, int y_width,
        const uint8_t *cb_data, int cb_width, const uint8_t *cr_data, int cr_width);
    void (*draw_custom_image)(custom_image_type type, int x, int y, float scale, int disable_filtering);
    int (*supports_yuv_image_format)(void);

    int (*start_tooltip_creation)(int width, int height);
    int (*start_tooltip_creation_for_domain)(render_domain domain, int width, int height);
    void (*finish_tooltip_creation)(void);
    int (*has_tooltip)(void);
    void (*set_tooltip_position)(int x, int y);
    void (*set_tooltip_position_for_domain)(render_domain domain, int x, int y);
    void (*set_tooltip_opacity)(int opacity);

    int (*save_image_from_screen)(int image_id, int x, int y, int width, int height);
    int (*save_image_from_screen_for_domain)(render_domain domain, int image_id, int x, int y, int width, int height);
    void (*draw_image_to_screen)(int image_id, int x, int y);
    void (*draw_image_to_screen_for_domain)(render_domain domain, int image_id, int x, int y);
    int (*save_screen_buffer)(color_t *pixels, int x, int y, int width, int height, int row_width);

    void (*get_max_image_size)(int *width, int *height);

    const image_atlas_data *(*prepare_image_atlas)(atlas_type type, int num_images, int last_width, int last_height);
    int (*create_image_atlas)(const image_atlas_data *data, int delete_buffers);
    const image_atlas_data *(*get_image_atlas)(atlas_type type);
    int (*has_image_atlas)(atlas_type type);
    void (*free_image_atlas)(atlas_type type);

    void (*load_unpacked_image)(const image *img, const color_t *pixels);
    void (*free_unpacked_image)(const image *img);
    void (*upload_image_resource)(image *img, const color_t *pixels, int width, int height);
    void (*release_image_resource)(image *img);

    int (*should_pack_image)(int width, int height);

    void (*update_scale)(int city_scale);
} graphics_renderer_interface;

const graphics_renderer_interface *graphics_renderer(void);

void graphics_renderer_set_interface(const graphics_renderer_interface *new_renderer);

#ifdef __cplusplus
}
#endif

