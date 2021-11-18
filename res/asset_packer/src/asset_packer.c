#include "stub.h"

#include "assets/assets.h"
#include "assets/group.h"
#include "assets/image.h"
#include "assets/layer.h"
#include "assets/xml.h"
#include "core/dir.h"
#include "core/image_packer.h"
#include "core/png_read.h"
#include "graphics/color.h"

#include "png.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#include <sys/stat.h>
#endif

// #define FORMAT_XML

#define ASSETS_IMAGE_SIZE 4096
#define CURSOR_IMAGE_SIZE 256
#define NEW_ASSETS_DIR "packed_assets"
#define CURSORS_DIR "Color_Cursors"
#define LINE_LENGTH 300
#define BYTES_PER_PIXEL 4

#ifdef FORMAT_XML
#define FORMAT_NEWLINE "\n"
#define FORMAT_IDENT "    "
#else
#define FORMAT_NEWLINE ""
#define FORMAT_IDENT ""
#endif

static const char *LAYER_PART[] = { "footprint", "top" };
static const char *LAYER_ROTATE[] = { "90", "180", "270" };
static const char *LAYER_INVERT[] = { "horizontal", "vertical", "both" };

static char current_file[MAX_PATH];

static color_t *final_image_pixels;
static unsigned int final_image_width;
static unsigned int final_image_height;

static int create_new_directory(const char *name)
{
#ifdef _WIN32
    if (CreateDirectoryA(name, 0) != 0) {
        return 1;
    } else {
        return GetLastError() == ERROR_ALREADY_EXISTS;
    }
#else
    if (mkdir(name, 0744) == 0) {
        return 1;
    } else {
        return errno == EEXIST;
    }
#endif
}

static void copy_asset_to_final_image(const layer *l, const image_packer_rect *rect)
{
    color_t *layer_pixels = malloc(sizeof(color_t) * l->width * l->height);
    if (!layer_pixels) {
        log_error("Out of memory.", 0, 0);
        return;
    }
    if (!png_read(l->asset_image_path, layer_pixels, 0, 0, rect->input.width, rect->input.height)) {
        free(layer_pixels);
        return;
    }

    if (!rect->output.rotated) {
        for (unsigned int y = 0; y < rect->input.height; y++) {
            color_t* src_pixel = &layer_pixels[y * rect->input.width];
            color_t* dst_pixel = &final_image_pixels[(y + rect->output.y) * final_image_width + rect->output.x];
            memcpy(dst_pixel, src_pixel, rect->input.width * sizeof(color_t));
        }
    } else {
        for (unsigned int y = 0; y < rect->input.height; y++) {
            color_t* src_pixel = &layer_pixels[y * rect->input.width];
            color_t* dst_pixel = &final_image_pixels[(rect->output.y + rect->input.width - 1) *
                final_image_width + y + rect->output.x];
            for (unsigned int x = 0; x < rect->input.width; x++) {
                *dst_pixel = *src_pixel++;
                dst_pixel -= final_image_width;
            }
        }
    }
    free(layer_pixels);
}

static void save_final_image(const char *path)
{
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);

    if (!png_ptr) {
        log_error("Error creating png structure for", path, 0);
        return;
    }
    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr) {
        log_error("Error creating png structure for", path, 0);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return;
    }
    png_set_compression_level(png_ptr, 3);
    unsigned int row_size = final_image_width * BYTES_PER_PIXEL;

    FILE *fp = fopen(path, "wb");
    if (!fp) {
        log_error("Error creating final png file at", path, 0);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return;
    }
    png_init_io(png_ptr, fp);

    if (setjmp(png_jmpbuf(png_ptr))) {
        log_error("Error constructing png file", path, 0);
        fclose(fp);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return;
    }
    png_set_IHDR(png_ptr, info_ptr, final_image_width, final_image_height, 8, PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png_ptr, info_ptr);

    uint8_t *row_pixels = malloc(final_image_width * BYTES_PER_PIXEL);
    if (!row_pixels) {
        log_error("Out of memory for png creation", path, 0);
        fclose(fp);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return;
    }
    memset(row_pixels, 0, final_image_width * BYTES_PER_PIXEL);

    if (setjmp(png_jmpbuf(png_ptr))) {
        log_error("Error constructing png file", path, 0);
        free(row_pixels);
        fclose(fp);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return;
    }
    for (unsigned int y = 0; y < final_image_height; ++y) {
        uint8_t *pixel = row_pixels;
        for (unsigned int x = 0; x < final_image_width; x++) {
            color_t input = final_image_pixels[y * final_image_width + x];
            *(pixel + 0) = (uint8_t) COLOR_COMPONENT(input, COLOR_BITSHIFT_RED);
            *(pixel + 1) = (uint8_t) COLOR_COMPONENT(input, COLOR_BITSHIFT_GREEN);
            *(pixel + 2) = (uint8_t) COLOR_COMPONENT(input, COLOR_BITSHIFT_BLUE);
            *(pixel + 3) = (uint8_t) COLOR_COMPONENT(input, COLOR_BITSHIFT_ALPHA);
            pixel += BYTES_PER_PIXEL;
        }
        png_write_row(png_ptr, row_pixels);
    }
    png_write_end(png_ptr, info_ptr);

    free(row_pixels);
    fclose(fp);
    png_destroy_write_struct(&png_ptr, &info_ptr);
}

int is_extra_asset(const layer *l)
{
    return l->asset_image_path != 0 && l->width != 0 && l->height != 0;
}

static unsigned int count_assets_in_group(int group_id)
{
    int extra_assets = 0;
    const image_groups *group = group_get_from_id(group_id);
    for (int image_id = group->first_image_index; image_id <= group->last_image_index; image_id++) {
        const asset_image *image = asset_image_get_from_id(image_id);
        for (const layer *l = &image->first_layer; l; l = l->next) {
            if (is_extra_asset(l)) {
                extra_assets++;
            }
        }
    }
    return extra_assets;
}

static void add_attribute_int(char **offset, unsigned int *max_length, const char *name, int value)
{
    if (value != 0) {
        int written = snprintf(*offset, *max_length, " %s=\"%d\"", name, value);
        *offset += written;
        *max_length -= written;
    }
}

static void add_attribute_bool(char **offset, unsigned int *max_length,
    const char *name, int value, const char *expression_if_true)
{
    if (value != 0) {
        int written = snprintf(*offset, *max_length, " %s=\"%s\"", name, expression_if_true);
        *offset += written;
        *max_length -= written;
    }
}

static void add_attribute_enum(char **offset, unsigned int *max_length,
    const char *name, int value, const char **display_value, int max_values)
{
    if (value > 0 && value <= max_values) {
        int written = snprintf(*offset, *max_length, " %s=\"%s\"", name, display_value[value - 1]);
        *offset += written;
        *max_length -= written;
    }
}

static void add_attribute_string(char **offset, unsigned int *max_length, const char *name, const char *value)
{
    if (value && *value != 0) {
        int written = snprintf(*offset, *max_length, " %s=\"%s\"", name, value);
        *offset += written;
        *max_length -= written;
    }
}

static void create_image_xml_line(char *dest, unsigned int max_length, const asset_image *image)
{
    char *offset = dest;
    int written = 0;

    written = snprintf(offset, max_length, "%s<image", FORMAT_IDENT);
    offset += written;
    max_length -= written;

    add_attribute_string(&offset, &max_length, "id", image->id);
    if (image->has_defined_size) {
        add_attribute_int(&offset, &max_length, "width", image->img.width);
        add_attribute_int(&offset, &max_length, "height", image->img.height);
    }
    snprintf(offset, max_length, ">%s", FORMAT_NEWLINE);
}

static void create_layer_xml_line(char *dest, unsigned int max_length, const layer *l)
{
    char *offset = dest;
    int written = 0;

    written = snprintf(offset, max_length, "%s%s<layer", FORMAT_IDENT, FORMAT_IDENT);
    offset += written;
    max_length -= written;

    add_attribute_string(&offset, &max_length, "group", l->original_image_group);
    add_attribute_string(&offset, &max_length, "image", l->original_image_id);
    add_attribute_int(&offset, &max_length, "src_x", l->src_x);
    add_attribute_int(&offset, &max_length, "src_y", l->src_y);
    add_attribute_int(&offset, &max_length, "x", l->x_offset);
    add_attribute_int(&offset, &max_length, "y", l->y_offset);
    add_attribute_int(&offset, &max_length, "width", l->width);
    add_attribute_int(&offset, &max_length, "height", l->height);
    add_attribute_enum(&offset, &max_length, "invert", l->invert, LAYER_INVERT, 3);
    add_attribute_enum(&offset, &max_length, "rotate", l->rotate, LAYER_ROTATE, 3);
    add_attribute_enum(&offset, &max_length, "part", l->part, LAYER_PART, 2);

    snprintf(offset, max_length, "/>%s", FORMAT_NEWLINE);
}

static int create_animation_xml_line(char *dest, unsigned int max_length, const asset_image *image)
{
    char *offset = dest;
    int written = 0;

    written = snprintf(offset, max_length, "%s%s<animation", FORMAT_IDENT, FORMAT_IDENT);
    offset += written;
    max_length -= written;

    if (!image->has_frame_elements) {
        add_attribute_int(&offset, &max_length, "frames", image->img.num_animation_sprites);
    }
    add_attribute_int(&offset, &max_length, "speed", image->img.animation_speed_id);
    add_attribute_int(&offset, &max_length, "x", image->img.sprite_offset_x);
    add_attribute_int(&offset, &max_length, "y", image->img.sprite_offset_y);
    add_attribute_bool(&offset, &max_length, "reversible", image->img.animation_can_reverse, "true");

    snprintf(offset, max_length, "%s>%s", image->has_frame_elements ? "" : "/", FORMAT_NEWLINE);
}

static void create_frame_xml_line(char *dest, unsigned int max_length, const layer *l)
{
    char *offset = dest;
    int written = 0;

    written = snprintf(offset, max_length, "%s%s%s<frame", FORMAT_IDENT, FORMAT_IDENT, FORMAT_IDENT);
    offset += written;
    max_length -= written;

    add_attribute_string(&offset, &max_length, "group", l->original_image_group);
    add_attribute_string(&offset, &max_length, "image", l->original_image_id);
    add_attribute_int(&offset, &max_length, "src_x", l->src_x);
    add_attribute_int(&offset, &max_length, "src_y", l->src_y);
    add_attribute_int(&offset, &max_length, "width", l->width);
    add_attribute_int(&offset, &max_length, "height", l->height);

    snprintf(offset, max_length, "/>%s", FORMAT_NEWLINE);
}

static int pack_layer(const image_packer *packer, layer *l, int rect_id)
{
    if (!is_extra_asset(l)) {
        return 0;
    }
    image_packer_rect *rect = &packer->rects[rect_id];
    l->src_x = rect->output.x;
    l->src_y = rect->output.y;
    if (rect->output.rotated) {
        int width = l->width;
        l->width = l->height;
        l->height = width;
        switch (l->rotate) {
            case ROTATE_90_DEGREES:
                l->rotate = ROTATE_180_DEGREES;
                break;
            case ROTATE_180_DEGREES:
                l->rotate = ROTATE_270_DEGREES;
                break;
            case ROTATE_270_DEGREES:
                l->rotate = ROTATE_NONE;
                break;
            case ROTATE_NONE:
            default:
                l->rotate = ROTATE_90_DEGREES;
                break;
        }
    }
    copy_asset_to_final_image(l, rect);
    return 1;
}

static void pack_group(int group_id)
{
    const image_groups *group = group_get_from_id(group_id);

    if (!group || !group->name) {
        log_error("Could not retreive a valid group from id", 0, group_id);
        return;
    }

    static char current_dir[MAX_PATH];
    static char current_line[LINE_LENGTH];

    snprintf(current_dir, MAX_PATH, "%s/%s/", NEW_ASSETS_DIR, group->name);
    snprintf(current_file, MAX_PATH, "%s/%s", NEW_ASSETS_DIR, group->path);

    FILE *xml_dest = fopen(current_file, "wb");

    if (!xml_dest) {
        log_error("Failed to create file", group->path, 0);
        return;
    }

    unsigned int num_images = count_assets_in_group(group_id);

    image_packer packer;
    image_packer_init(&packer, num_images, ASSETS_IMAGE_SIZE, ASSETS_IMAGE_SIZE);

    packer.options.allow_rotation = 1;
    packer.options.fail_policy = IMAGE_PACKER_NEW_IMAGE;
    packer.options.reduce_image_size = 1;

    int rect_id = 0;

    log_info("Packing", group->name, 0);

    for (int image_id = group->first_image_index; image_id <= group->last_image_index; image_id++) {
        const asset_image *image = asset_image_get_from_id(image_id);
        for (const layer *l = &image->first_layer; l; l = l->next) {
            if (is_extra_asset(l)) {
                image_packer_rect *rect = &packer.rects[rect_id++];
                rect->input.width = l->width;
                rect->input.height = l->height;
            }
        }
    }

    if (image_packer_pack(&packer) != num_images) {
        log_error("Error during pack.", 0, 0);
        fclose(xml_dest);
        image_packer_free(&packer);
        return;
    }

    printf("Info: %d Images packed. Texture size: %dx%d.\n", num_images,
    packer.result.last_image_width, packer.result.last_image_height);

    final_image_width = packer.result.last_image_width;
    final_image_height = packer.result.last_image_height;
    final_image_pixels = malloc(sizeof(color_t) * final_image_width * final_image_height);
    if (!final_image_pixels) {
        log_error("Out of memory when creating the final image.", 0, 0);
        fclose(xml_dest);
        image_packer_free(&packer);
        return;
    }
    memset(final_image_pixels, 0, sizeof(color_t) * final_image_width * final_image_height);
        
    rect_id = 0;

    log_info("Creating xml file...", 0, 0);

    fprintf(xml_dest, "<?xml version=\"1.0\"?>\n");
    fprintf(xml_dest, "<!DOCTYPE assetlist>\n\n");

    fprintf(xml_dest, "<!-- XML auto packed by asset_packer. DO NOT use as a reference.\n");
    fprintf(xml_dest, "     Use the assets directory from the source code instead. -->\n\n");

    fprintf(xml_dest, "<assetlist name=\"%s\">%s", group->name, FORMAT_NEWLINE);

    for (int image_id = group->first_image_index; image_id <= group->last_image_index; image_id++) {
        asset_image *image = asset_image_get_from_id(image_id);
        create_image_xml_line(current_line, LINE_LENGTH, image);
        fprintf(xml_dest, current_line);
        for (layer *l = &image->first_layer; l; l = l->next) {
            rect_id += pack_layer(&packer, l, rect_id);
            create_layer_xml_line(current_line, LINE_LENGTH, l);
            fprintf(xml_dest, current_line);
        }
        if (image->img.num_animation_sprites) {
            create_animation_xml_line(current_line, LINE_LENGTH, image);
            fprintf(xml_dest, current_line);
            if (image->has_frame_elements) {
                for (int i = 0; i < image->img.num_animation_sprites; i++) {
                    image_id++;
                    asset_image *frame = asset_image_get_from_id(image_id);
                    layer *l = frame->last_layer;
                    rect_id += pack_layer(&packer, l, rect_id);
                    create_frame_xml_line(current_line, LINE_LENGTH, l);
                    fprintf(xml_dest, current_line);
                }
                fprintf(xml_dest, "%s%s</animation>%s", FORMAT_IDENT, FORMAT_IDENT, FORMAT_NEWLINE);
            }
        }
        fprintf(xml_dest, "%s</image>%s", FORMAT_IDENT, FORMAT_NEWLINE);
    }

    fprintf(xml_dest, "</assetlist>\n");

    fclose(xml_dest);
    image_packer_free(&packer);

    snprintf(current_file, MAX_PATH, "%s/%s.png", NEW_ASSETS_DIR, group->name);

    log_info("Creating png file...", 0, 0);

    save_final_image(current_file);

    free(final_image_pixels);
}

static void pack_cursors(void)
{
    static const char *cursor_names[] = { "Arrow", "Shovel", "Sword" };
    static const char *cursor_sizes[] = { "150", "200" };

    #define NUM_CURSOR_NAMES (sizeof(cursor_names) / sizeof(cursor_names[0]))
    #define NUM_CURSOR_SIZES (sizeof(cursor_sizes) / sizeof(cursor_sizes[0]) + 1)

    static layer cursors[NUM_CURSOR_NAMES * NUM_CURSOR_SIZES];
    
    image_packer packer;
    image_packer_init(&packer, NUM_CURSOR_NAMES * NUM_CURSOR_SIZES, CURSOR_IMAGE_SIZE, CURSOR_IMAGE_SIZE);

    packer.options.reduce_image_size = 1;
    packer.options.sort_by = IMAGE_PACKER_SORT_BY_AREA;

    for (int i = 0; i < NUM_CURSOR_NAMES; i++) {    
        for (int j = 0; j < NUM_CURSOR_SIZES; j++) {
            int index = i * NUM_CURSOR_SIZES + j;
            layer *cursor = &cursors[index];
            cursor->asset_image_path = malloc(MAX_PATH);
            if (!cursor->asset_image_path) {
                log_error("Out of memory.", 0, 0);
                image_packer_free(&packer);
                return;
            }
            if (j > 0) {
                snprintf(cursor->asset_image_path, MAX_PATH, "%s/%s_%s.png", CURSORS_DIR,
                    cursor_names[i], cursor_sizes[j - 1]);
            } else {
                snprintf(cursor->asset_image_path, MAX_PATH, "%s/%s.png", CURSORS_DIR, cursor_names[i]);
            }
            if (!png_get_image_size(cursor->asset_image_path, &cursor->width, &cursor->height)) {
                image_packer_free(&packer);
                return;
            }
            packer.rects[index].input.width = cursor->width;
            packer.rects[index].input.height = cursor->height;
        }
    }

    image_packer_pack(&packer);

    final_image_width = packer.result.last_image_width;
    final_image_height = packer.result.last_image_height;
    final_image_pixels = malloc(sizeof(color_t) * final_image_width * final_image_height);
    if (!final_image_pixels) {
        log_error("Out of memory when creating the final cursor image.", 0, 0);
        image_packer_free(&packer);
        return;
    }
    memset(final_image_pixels, 0, sizeof(color_t) * final_image_width * final_image_height);

    log_info("Cursor positions and sizes in packed image:", 0, 0);

    printf("   Name             x       y      width      height\n");

    for (int i = 0; i < NUM_CURSOR_NAMES * NUM_CURSOR_SIZES; i++) {
        layer *cursor = &cursors[i];
        pack_layer(&packer, cursor, i);
        printf("%-16s  %3d     %3d        %3d         %3d\n",
            cursor->asset_image_path + strlen(CURSORS_DIR) + 1,
            packer.rects[i].output.x, packer.rects[i].output.y, cursor->width, cursor->height);
    }

    snprintf(current_file, MAX_PATH, "%s/%s.png", NEW_ASSETS_DIR, CURSORS_DIR);

    save_final_image(current_file);

    free(final_image_pixels);

    image_packer_free(&packer);
}

int main(int argc, char **argv)
{
    const dir_listing *xml_files = dir_find_files_with_extension(ASSETS_DIRECTORY, "xml");
    if (xml_files->num_files == 0) {
        log_error("Please add a valid assets folder to this directory", 0, 0);
        return 1;
    }

    if (!create_new_directory(NEW_ASSETS_DIR)) {
        log_error("Failed to create directory", NEW_ASSETS_DIR, 0);
        return 2;
    }

    if (!group_create_all(xml_files->num_files) || !asset_image_init_array()) {
        log_error("Not enough memory to initialize extra assets.", 0, 0);
        return 3;
    }

    for (int i = 0; i < xml_files->num_files; ++i) {
        xml_process_assetlist_file(xml_files->files[i]);
    }

    log_info("Preparing to pack...", 0, 0);

    for (int i = 0; i < group_get_total(); i++) {
        pack_group(i);
    }

    log_info("Packing cursors...", 0, 0);

    pack_cursors();

    log_info("All done!", 0, 0);

    png_release();
    return 0;
}
