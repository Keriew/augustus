#include "assets/image_group_payload.h"

#include "assets/layer.h"
#include "core/crash_context.h"
#include "core/image_payload.h"

extern "C" {
#include "core/file.h"
#include "core/log.h"
#include "core/png_read.h"
#include "core/xml_parser.h"
}

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using GroupPayloadMap = std::unordered_map<std::string, std::unique_ptr<ImageGroupPayload>>;
using GroupDocMap = std::unordered_map<std::string, std::unique_ptr<struct ImageGroupDoc>>;
using MergedGroupMap = std::unordered_map<std::string, std::unique_ptr<struct MergedImageGroup>>;
using ResolvedEntryMap = std::unordered_map<std::string, std::unique_ptr<struct ResolvedImageEntry>>;
using PngRasterMap = std::unordered_map<std::string, std::unique_ptr<struct RasterSurface>>;

GroupPayloadMap g_group_payloads;
GroupDocMap g_group_docs;
MergedGroupMap g_merged_groups;
ResolvedEntryMap g_resolved_entries;
PngRasterMap g_png_rasters;

std::unordered_set<std::string> g_failed_group_payloads;
std::unordered_set<std::string> g_loading_group_payloads;
std::unordered_set<std::string> g_failed_group_docs;
std::unordered_set<std::string> g_failed_merged_groups;
std::unordered_set<std::string> g_failed_resolved_entries;
std::unordered_set<std::string> g_loading_resolved_entries;
std::unordered_set<std::string> g_failed_png_rasters;

struct RasterSurface {
    int width = 0;
    int height = 0;
    std::vector<color_t> pixels;
};

enum class RawReferenceType {
    NONE,
    PNG_PATH,
    GROUP_IMAGE
};

struct RawImageReference {
    RawReferenceType type = RawReferenceType::NONE;
    std::string path;
    std::string group_key;
    std::string image_id;
};

struct RawLayerDef {
    RawImageReference reference;
    int src_x = 0;
    int src_y = 0;
    int x_offset = 0;
    int y_offset = 0;
    int width = 0;
    int height = 0;
    layer_invert_type invert = INVERT_NONE;
    layer_rotate_type rotate = ROTATE_NONE;
    layer_isometric_part part = PART_BOTH;
    layer_mask mask = LAYER_MASK_NONE;
};

struct RawAnimationDef {
    int present = 0;
    image_animation metadata = {};
    int implicit_frame_count = 0;
    std::vector<RawLayerDef> explicit_frames;
};

struct ImageEntryDef {
    std::string id;
    xml_asset_source source = XML_ASSET_SOURCE_AUTO;
    int local_order = 0;
    int width = 0;
    int height = 0;
    int is_isometric = 0;
    int has_full_image_ref = 0;
    RawImageReference full_image_ref;
    std::vector<RawLayerDef> layers;
    RawAnimationDef animation;
};

struct ImageGroupDoc {
    std::string key;
    std::string xml_path;
    xml_asset_source source = XML_ASSET_SOURCE_AUTO;
    std::vector<std::string> ordered_ids;
    std::unordered_map<std::string, ImageEntryDef> entries;
};

struct MergedImageGroup {
    std::string key;
    std::string xml_path;
    std::vector<std::string> ordered_ids;
    std::unordered_map<std::string, xml_asset_source> winning_sources;
};

struct ResolvedImageEntry {
    std::string image_key;
    std::string top_image_key;
    std::vector<std::string> animation_frame_keys;
    image_animation animation = {};
    int has_animation = 0;
    std::vector<color_t> split_pixels;
    int split_width = 0;
    int split_height = 0;
    int top_height = 0;
    int is_isometric = 0;
};

struct PreparedLayer {
    RasterSurface surface;
    int x_offset = 0;
    int y_offset = 0;
    layer_invert_type invert = INVERT_NONE;
    layer_rotate_type rotate = ROTATE_NONE;
    layer_mask mask = LAYER_MASK_NONE;
};

struct ParseState {
    std::string requested_key;
    std::string xml_path;
    xml_asset_source source = XML_ASSET_SOURCE_AUTO;
    std::unique_ptr<ImageGroupDoc> doc;
    ImageEntryDef *current_entry = nullptr;
    int error = 0;
};

ParseState g_parse_state;

static const char *INVERT_VALUES[3] = { "horizontal", "vertical", "both" };
static const char *ROTATE_VALUES[3] = { "90", "180", "270" };
static const char *PART_VALUES[2] = { "footprint", "top" };
static const char *MASK_VALUES[2] = { "grayscale", "alpha" };

static std::string normalize_path_key(const char *text)
{
    std::string result = text ? text : "";
    while (!result.empty() && (result.back() == '\r' || result.back() == '\n' || result.back() == ' ' || result.back() == '\t')) {
        result.pop_back();
    }
    size_t first = 0;
    while (first < result.size() && (result[first] == ' ' || result[first] == '\t' || result[first] == '\r' || result[first] == '\n')) {
        first++;
    }
    if (first > 0) {
        result.erase(0, first);
    }

    for (char &value : result) {
        if (value == '/') {
            value = '\\';
        }
    }

    std::string normalized;
    normalized.reserve(result.size());
    char previous = '\0';
    for (char value : result) {
        if (value == '\\' && previous == '\\') {
            continue;
        }
        normalized.push_back(value);
        previous = value;
    }
    return normalized;
}

static std::string normalize_group_reference_key(const char *group_name, const std::string &current_group)
{
    const std::string normalized_group = normalize_path_key(group_name);
    if (normalized_group == "this" || normalized_group == "THIS") {
        return current_group;
    }
    return normalized_group;
}

static std::string make_source_doc_cache_key(const std::string &group_key, xml_asset_source source)
{
    return group_key + "|" + std::to_string(static_cast<int>(source));
}

static std::string make_selector_cache_key(const std::string &group_key, xml_asset_source source, const std::string &image_id)
{
    return group_key + "|" + std::to_string(static_cast<int>(source)) + "|" + image_id;
}

static std::string make_entry_payload_key(const std::string &group_key, xml_asset_source source, const std::string &image_id)
{
    return "MergedGraphics\\" + group_key + "\\" + std::to_string(static_cast<int>(source)) + "\\" + image_id;
}

static const ImageGroupDoc *find_group_doc(const std::string &group_key, xml_asset_source source)
{
    auto it = g_group_docs.find(make_source_doc_cache_key(group_key, source));
    return it == g_group_docs.end() ? nullptr : it->second.get();
}

static const MergedImageGroup *find_merged_group(const std::string &group_key)
{
    auto it = g_merged_groups.find(group_key);
    return it == g_merged_groups.end() ? nullptr : it->second.get();
}

static const ResolvedImageEntry *find_resolved_entry(const std::string &group_key, xml_asset_source source, const std::string &image_id)
{
    auto it = g_resolved_entries.find(make_selector_cache_key(group_key, source, image_id));
    return it == g_resolved_entries.end() ? nullptr : it->second.get();
}

static void release_resolved_entry_resources(const ResolvedImageEntry &entry)
{
    if (!entry.image_key.empty()) {
        image_payload_release_key(entry.image_key.c_str());
    }
    if (!entry.top_image_key.empty()) {
        image_payload_release_key(entry.top_image_key.c_str());
    }
    for (const std::string &frame_key : entry.animation_frame_keys) {
        if (!frame_key.empty()) {
            image_payload_release_key(frame_key.c_str());
        }
    }
}

static int read_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    FILE *file = file_open(filename, "rb");
    if (!file) {
        log_error("Unable to open image group xml", filename, 0);
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        file_close(file);
        log_error("Unable to seek image group xml", filename, 0);
        return 0;
    }

    long size = ftell(file);
    if (size < 0) {
        file_close(file);
        log_error("Unable to size image group xml", filename, 0);
        return 0;
    }
    rewind(file);

    buffer.resize(static_cast<size_t>(size));
    if (!buffer.empty()) {
        const size_t bytes_read = fread(buffer.data(), 1, buffer.size(), file);
        if (bytes_read != buffer.size()) {
            file_close(file);
            log_error("Unable to read image group xml", filename, 0);
            return 0;
        }
    }

    file_close(file);
    return 1;
}

static void convert_to_grayscale(RasterSurface &surface)
{
    for (color_t &pixel : surface.pixels) {
        color_t r = (pixel & COLOR_CHANNEL_RED) >> COLOR_BITSHIFT_RED;
        color_t g = (pixel & COLOR_CHANNEL_GREEN) >> COLOR_BITSHIFT_GREEN;
        color_t b = (pixel & COLOR_CHANNEL_BLUE) >> COLOR_BITSHIFT_BLUE;
        color_t gray = static_cast<color_t>(r * 0.299f + g * 0.587f + b * 0.114f);
        pixel = (pixel & COLOR_CHANNEL_ALPHA) |
            (gray << COLOR_BITSHIFT_RED) |
            (gray << COLOR_BITSHIFT_GREEN) |
            (gray << COLOR_BITSHIFT_BLUE);
    }
}

static int load_png_raster_from_path(const char *full_path, RasterSurface &out_surface)
{
    if (!full_path || !*full_path) {
        return 0;
    }

    auto cached = g_png_rasters.find(full_path);
    if (cached != g_png_rasters.end()) {
        out_surface = *cached->second;
        return 1;
    }
    if (g_failed_png_rasters.find(full_path) != g_failed_png_rasters.end()) {
        return 0;
    }

    if (!png_load_from_file(full_path, 0)) {
        g_failed_png_rasters.insert(full_path);
        return 0;
    }

    int width = 0;
    int height = 0;
    if (!png_get_image_size(&width, &height) || width <= 0 || height <= 0) {
        png_unload();
        g_failed_png_rasters.insert(full_path);
        return 0;
    }

    std::unique_ptr<RasterSurface> surface = std::make_unique<RasterSurface>();
    surface->width = width;
    surface->height = height;
    surface->pixels.resize(static_cast<size_t>(width) * height);
    if (!png_read(surface->pixels.data(), 0, 0, width, height, 0, 0, width, 0)) {
        png_unload();
        g_failed_png_rasters.insert(full_path);
        return 0;
    }
    png_unload();

    out_surface = *surface;
    g_png_rasters.emplace(full_path, std::move(surface));
    return 1;
}

static int raster_has_pixels(const RasterSurface &surface)
{
    return surface.width > 0 &&
        surface.height > 0 &&
        surface.pixels.size() == static_cast<size_t>(surface.width) * surface.height;
}

static RasterSurface crop_surface(const RasterSurface &source, int src_x, int src_y, int width, int height)
{
    RasterSurface result;
    if (!raster_has_pixels(source)) {
        return result;
    }

    int actual_width = width > 0 ? width : source.width;
    int actual_height = height > 0 ? height : source.height;
    if (src_x < 0 || src_y < 0 || src_x >= source.width || src_y >= source.height) {
        return result;
    }
    if (src_x + actual_width > source.width) {
        actual_width = source.width - src_x;
    }
    if (src_y + actual_height > source.height) {
        actual_height = source.height - src_y;
    }
    if (actual_width <= 0 || actual_height <= 0) {
        return result;
    }

    result.width = actual_width;
    result.height = actual_height;
    result.pixels.resize(static_cast<size_t>(actual_width) * actual_height);
    for (int y = 0; y < actual_height; y++) {
        memcpy(
            &result.pixels[static_cast<size_t>(y) * actual_width],
            &source.pixels[static_cast<size_t>(src_y + y) * source.width + src_x],
            sizeof(color_t) * actual_width);
    }
    return result;
}

// SECTION: raster_reconstruction
static int derive_original_height(const ResolvedImageEntry &entry)
{
    if (!entry.is_isometric || entry.top_height <= 0) {
        return entry.split_height;
    }
    const int footprint_height = entry.split_height - entry.top_height;
    return entry.top_height + footprint_height / 2;
}

static RasterSurface surface_from_resolved_entry_part(const ResolvedImageEntry &entry, layer_isometric_part part)
{
    RasterSurface result;
    if (entry.split_width <= 0 || entry.split_height <= 0 || entry.split_pixels.empty()) {
        return result;
    }

    if (!entry.is_isometric || entry.top_height <= 0) {
        if (part == PART_TOP) {
            return result;
        }
        result.width = entry.split_width;
        result.height = entry.split_height;
        result.pixels = entry.split_pixels;
        return result;
    }

    const int footprint_height = entry.split_height - entry.top_height;
    const int original_height = derive_original_height(entry);
    result.width = entry.split_width;
    result.height = part == PART_TOP ? entry.top_height : original_height;
    if (result.width <= 0 || result.height <= 0) {
        result = {};
        return result;
    }

    result.pixels.resize(static_cast<size_t>(result.width) * result.height);
    memset(result.pixels.data(), 0, sizeof(color_t) * result.pixels.size());

    if (part == PART_TOP || part == PART_BOTH) {
        for (int y = 0; y < entry.top_height; y++) {
            memcpy(
                &result.pixels[static_cast<size_t>(y) * result.width],
                &entry.split_pixels[static_cast<size_t>(y) * entry.split_width],
                sizeof(color_t) * result.width);
        }
        if (part == PART_TOP) {
            return result;
        }
    }

    image_copy_info copy = {
        { 0, entry.top_height, entry.split_width, entry.split_height, entry.split_pixels.data() },
        { 0, result.height - footprint_height, result.width, result.height, result.pixels.data() },
        { 0, 0, entry.split_width, footprint_height }
    };
    image_copy_isometric_footprint(&copy);
    return result;
}

static int transformed_layer_width(const PreparedLayer &layer)
{
    return layer.rotate == ROTATE_90_DEGREES || layer.rotate == ROTATE_270_DEGREES ?
        layer.surface.height : layer.surface.width;
}

static int transformed_layer_height(const PreparedLayer &layer)
{
    return layer.rotate == ROTATE_90_DEGREES || layer.rotate == ROTATE_270_DEGREES ?
        layer.surface.width : layer.surface.height;
}

static const color_t *prepared_layer_get_color(const PreparedLayer &layer, int x, int y)
{
    x -= layer.x_offset;
    y -= layer.y_offset;

    if (layer.rotate == ROTATE_90_DEGREES || layer.rotate == ROTATE_270_DEGREES) {
        int temp = x;
        x = y;
        y = temp;
    }

    layer_invert_type invert = layer.invert;
    if (layer.rotate == ROTATE_90_DEGREES) {
        invert = static_cast<layer_invert_type>(static_cast<int>(invert) ^ static_cast<int>(INVERT_VERTICAL));
    } else if (layer.rotate == ROTATE_180_DEGREES) {
        invert = static_cast<layer_invert_type>(static_cast<int>(invert) ^ static_cast<int>(INVERT_BOTH));
    } else if (layer.rotate == ROTATE_270_DEGREES) {
        invert = static_cast<layer_invert_type>(static_cast<int>(invert) ^ static_cast<int>(INVERT_HORIZONTAL));
    }
    if ((static_cast<int>(invert) & static_cast<int>(INVERT_HORIZONTAL)) != 0) {
        x = layer.surface.width - x - 1;
    }
    if ((static_cast<int>(invert) & static_cast<int>(INVERT_VERTICAL)) != 0) {
        y = layer.surface.height - y - 1;
    }
    if (x < 0 || y < 0 || x >= layer.surface.width || y >= layer.surface.height) {
        return nullptr;
    }
    return &layer.surface.pixels[static_cast<size_t>(y) * layer.surface.width + x];
}

static void compose_prepared_layer(RasterSurface &target, const PreparedLayer &layer, int has_alpha_mask)
{
    if (!raster_has_pixels(target) || !raster_has_pixels(layer.surface)) {
        return;
    }

    int image_start_x = layer.x_offset < 0 ? 0 : layer.x_offset;
    int image_start_y = layer.y_offset < 0 ? 0 : layer.y_offset;
    int image_valid_width = image_start_x + transformed_layer_width(layer) - (layer.x_offset < 0 ? -layer.x_offset : 0);
    int image_valid_height = image_start_y + transformed_layer_height(layer) - (layer.y_offset < 0 ? -layer.y_offset : 0);
    if (image_valid_width > target.width) {
        image_valid_width = target.width;
    }
    if (image_valid_height > target.height) {
        image_valid_height = target.height;
    }

    const int inverts_and_rotates = layer.invert != INVERT_NONE &&
        (layer.rotate == ROTATE_90_DEGREES || layer.rotate == ROTATE_270_DEGREES);

    int layer_step_x = 1;
    if (layer.rotate == ROTATE_90_DEGREES || layer.rotate == ROTATE_270_DEGREES) {
        layer_step_x = layer.surface.width;
    }

    if (!inverts_and_rotates) {
        layer_invert_type invert = layer.invert;
        if (layer.rotate == ROTATE_90_DEGREES || layer.rotate == ROTATE_180_DEGREES) {
            invert = static_cast<layer_invert_type>(static_cast<int>(invert) ^ static_cast<int>(INVERT_HORIZONTAL));
        }
        if ((static_cast<int>(invert) & static_cast<int>(INVERT_HORIZONTAL)) != 0) {
            layer_step_x = -layer_step_x;
        }
    }

    for (int y = image_start_y; y < image_valid_height; y++) {
        color_t *pixel = &target.pixels[static_cast<size_t>(y) * target.width + image_start_x];
        const color_t *layer_pixel = nullptr;
        if (!inverts_and_rotates) {
            layer_pixel = prepared_layer_get_color(layer, image_start_x, y);
        }

        for (int x = image_start_x; x < image_valid_width; x++) {
            if (inverts_and_rotates) {
                layer_pixel = prepared_layer_get_color(layer, x, y);
            }
            if (!layer_pixel) {
                pixel++;
                continue;
            }

            color_t image_pixel_alpha = *pixel & COLOR_CHANNEL_ALPHA;
            if (layer.mask == LAYER_MASK_ALPHA) {
                color_t alpha_mask = (*layer_pixel & COLOR_CHANNEL_BLUE) << COLOR_BITSHIFT_ALPHA;
                if (alpha_mask == ALPHA_TRANSPARENT || image_pixel_alpha == ALPHA_TRANSPARENT) {
                    *pixel = ALPHA_TRANSPARENT;
                } else if (alpha_mask != ALPHA_OPAQUE) {
                    if (image_pixel_alpha != ALPHA_OPAQUE) {
                        color_t alpha_src = image_pixel_alpha >> COLOR_BITSHIFT_ALPHA;
                        color_t alpha_dst = alpha_mask >> COLOR_BITSHIFT_ALPHA;
                        alpha_mask = COLOR_MIX_ALPHA(alpha_src, alpha_dst) << COLOR_BITSHIFT_ALPHA;
                    }
                    color_t result = *pixel | ALPHA_OPAQUE;
                    result &= alpha_mask & COLOR_WHITE;
                    *pixel = result;
                }
                pixel++;
                layer_pixel += layer_step_x;
                continue;
            }

            color_t layer_pixel_alpha = *layer_pixel & COLOR_CHANNEL_ALPHA;
            if ((image_pixel_alpha == ALPHA_OPAQUE && !has_alpha_mask) || layer_pixel_alpha == ALPHA_TRANSPARENT) {
                pixel++;
                layer_pixel += layer_step_x;
                continue;
            }
            if (image_pixel_alpha == ALPHA_TRANSPARENT || (layer_pixel_alpha == ALPHA_OPAQUE && has_alpha_mask)) {
                *pixel = *layer_pixel;
            } else if (layer_pixel_alpha == ALPHA_OPAQUE) {
                color_t alpha = image_pixel_alpha >> COLOR_BITSHIFT_ALPHA;
                *pixel = COLOR_BLEND_ALPHA_TO_OPAQUE(*pixel, *layer_pixel, alpha);
            } else {
                color_t alpha_src = image_pixel_alpha >> COLOR_BITSHIFT_ALPHA;
                color_t alpha_dst = layer_pixel_alpha >> COLOR_BITSHIFT_ALPHA;
                color_t alpha_mix = COLOR_MIX_ALPHA(alpha_src, alpha_dst);
                *pixel = COLOR_BLEND_ALPHAS(*pixel, *layer_pixel, alpha_src, alpha_dst, alpha_mix);
            }
            pixel++;
            layer_pixel += layer_step_x;
        }
    }
}

static int is_bare_group_reference(const RawLayerDef &layer)
{
    return layer.reference.type == RawReferenceType::GROUP_IMAGE &&
        layer.src_x == 0 &&
        layer.src_y == 0 &&
        layer.x_offset == 0 &&
        layer.y_offset == 0 &&
        layer.width == 0 &&
        layer.height == 0 &&
        layer.invert == INVERT_NONE &&
        layer.rotate == ROTATE_NONE &&
        layer.part == PART_BOTH &&
        layer.mask == LAYER_MASK_NONE;
}

static int has_top_part(const RasterSurface &surface)
{
    if (surface.width <= 0 || surface.height <= 0 || surface.pixels.empty()) {
        return 0;
    }
    int tiles = (surface.width + 2) / (FOOTPRINT_WIDTH + 2);
    int top_height = surface.height - tiles * FOOTPRINT_HEIGHT;
    for (int y = 0; y < top_height; y++) {
        const color_t *row = &surface.pixels[static_cast<size_t>(y) * surface.width];
        int footprint_row = y - surface.height - 1 - tiles * FOOTPRINT_HALF_HEIGHT;
        int half_top_pixels_in_row = (footprint_row < 0 ? surface.width : surface.width - 2 + 4 * footprint_row) / 2;
        for (int x = 0; x < half_top_pixels_in_row; x++) {
            if ((row[x] & COLOR_CHANNEL_ALPHA) != ALPHA_TRANSPARENT) {
                return 1;
            }
        }
        row += surface.width - half_top_pixels_in_row;
        for (int x = 0; x < half_top_pixels_in_row; x++) {
            if ((row[x] & COLOR_CHANNEL_ALPHA) != ALPHA_TRANSPARENT) {
                return 1;
            }
        }
    }
    return 0;
}

static void split_top_and_footprint(const RasterSurface &source, int top_height, RasterSurface &split_surface)
{
    const int tiles = (source.width + 2) / (FOOTPRINT_WIDTH + 2);
    const int footprint_height = tiles * FOOTPRINT_HEIGHT;

    split_surface.width = source.width;
    split_surface.height = footprint_height + top_height;
    split_surface.pixels.resize(static_cast<size_t>(split_surface.width) * split_surface.height);
    memset(split_surface.pixels.data(), 0, sizeof(color_t) * split_surface.pixels.size());

    for (int y = 0; y < top_height; y++) {
        const color_t *src_row = &source.pixels[static_cast<size_t>(y) * source.width];
        color_t *dst_row = &split_surface.pixels[static_cast<size_t>(y) * split_surface.width];
        int footprint_row = y - top_height - 1 + tiles * FOOTPRINT_HALF_HEIGHT;
        int half_top_pixels_in_row = (footprint_row < 0 ? source.width : source.width - 2 - 4 * footprint_row) / 2;
        memcpy(dst_row, src_row, sizeof(color_t) * half_top_pixels_in_row);
        src_row += source.width - half_top_pixels_in_row;
        dst_row += split_surface.width - half_top_pixels_in_row;
        memcpy(dst_row, src_row, sizeof(color_t) * half_top_pixels_in_row);
    }

    image_copy_info copy = {
        { 0, source.height - footprint_height, source.width, source.height, source.pixels.data() },
        { 0, top_height, split_surface.width, split_surface.height, split_surface.pixels.data() },
        { 0, 0, source.width, footprint_height }
    };
    image_copy_isometric_footprint(&copy);
}

static int upload_split_surface(
    const std::string &base_key,
    const RasterSurface &split_surface,
    int top_height,
    int is_isometric,
    std::string &out_image_key,
    std::string &out_top_image_key)
{
    if (!raster_has_pixels(split_surface)) {
        return 0;
    }

    if (top_height > 0) {
        const int footprint_height = split_surface.height - top_height;
        const int original_canvas_height = top_height + footprint_height / 2;
        image base_image = {};
        base_image.width = split_surface.width;
        base_image.height = footprint_height;
        base_image.original.width = split_surface.width;
        base_image.original.height = original_canvas_height;
        base_image.is_isometric = is_isometric;
        base_image.atlas.y_offset = top_height;

        if (!image_payload_load_pixels_payload(
                base_key.c_str(),
                base_image,
                &split_surface.pixels[static_cast<size_t>(top_height) * split_surface.width],
                split_surface.width,
                footprint_height)) {
            return 0;
        }

        image top_image = {};
        top_image.width = split_surface.width;
        top_image.height = top_height;
        top_image.original.width = split_surface.width;
        top_image.original.height = top_height;
        if (!image_payload_load_pixels_payload(
                (base_key + "\\top").c_str(),
                top_image,
                split_surface.pixels.data(),
                split_surface.width,
                top_height)) {
            image_payload_release_key(base_key.c_str());
            return 0;
        }

        out_image_key = base_key;
        out_top_image_key = base_key + "\\top";
        return 1;
    }

    image base_image = {};
    base_image.width = split_surface.width;
    base_image.height = split_surface.height;
    base_image.original.width = split_surface.width;
    base_image.original.height = split_surface.height;
    base_image.is_isometric = is_isometric;
    if (!image_payload_load_pixels_payload(
            base_key.c_str(),
            base_image,
            split_surface.pixels.data(),
            split_surface.width,
            split_surface.height)) {
        return 0;
    }

    out_image_key = base_key;
    out_top_image_key.clear();
    return 1;
}

// SECTION: parser
static int xml_start_assetlist(void)
{
    const char *name = xml_parser_get_attribute_string("name");
    const std::string normalized_name = normalize_path_key(name);
    if (normalized_name.empty() || normalized_name != g_parse_state.requested_key) {
        crash_context_report_error("ImageGroup assetlist name does not match requested key", name ? name : "");
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.doc = std::make_unique<ImageGroupDoc>();
    g_parse_state.doc->key = normalized_name;
    g_parse_state.doc->xml_path = g_parse_state.xml_path;
    g_parse_state.doc->source = g_parse_state.source;
    return 1;
}

static RawLayerDef parse_raw_layer(layer_isometric_part default_part, layer_mask default_mask)
{
    RawLayerDef layer;
    const char *path = xml_parser_get_attribute_string("src");
    const char *group = xml_parser_get_attribute_string("group");
    const char *image_id = xml_parser_get_attribute_string("image");

    if (path && *path) {
        layer.reference.type = RawReferenceType::PNG_PATH;
        layer.reference.path = normalize_path_key(path);
    } else if (group && *group) {
        layer.reference.type = RawReferenceType::GROUP_IMAGE;
        layer.reference.group_key = normalize_group_reference_key(group, g_parse_state.requested_key);
        layer.reference.image_id = image_id ? image_id : "";
    }

    layer.src_x = xml_parser_get_attribute_int("src_x");
    layer.src_y = xml_parser_get_attribute_int("src_y");
    layer.x_offset = xml_parser_get_attribute_int("x");
    layer.y_offset = xml_parser_get_attribute_int("y");
    layer.width = xml_parser_get_attribute_int("width");
    layer.height = xml_parser_get_attribute_int("height");
    layer.invert = static_cast<layer_invert_type>(
        xml_parser_get_attribute_enum("invert", INVERT_VALUES, 3, INVERT_HORIZONTAL));
    layer.rotate = static_cast<layer_rotate_type>(
        xml_parser_get_attribute_enum("rotate", ROTATE_VALUES, 3, ROTATE_90_DEGREES));
    layer.part = static_cast<layer_isometric_part>(
        xml_parser_get_attribute_enum("part", PART_VALUES, 2, PART_FOOTPRINT));
    if (layer.part == PART_NONE) {
        layer.part = default_part;
    }
    layer.mask = static_cast<layer_mask>(
        xml_parser_get_attribute_enum("mask", MASK_VALUES, 2, LAYER_MASK_GRAYSCALE));
    if (!xml_parser_has_attribute("mask")) {
        layer.mask = default_mask;
    }
    return layer;
}

static int xml_start_image(void)
{
    if (!g_parse_state.doc) {
        g_parse_state.error = 1;
        return 0;
    }

    const char *id = xml_parser_get_attribute_string("id");
    if (!id || !*id) {
        crash_context_report_error("ImageGroup image is missing id", g_parse_state.requested_key.c_str());
        g_parse_state.error = 1;
        return 0;
    }

    ImageEntryDef entry;
    entry.id = id;
    entry.source = g_parse_state.source;
    entry.local_order = static_cast<int>(g_parse_state.doc->ordered_ids.size());
    entry.width = xml_parser_get_attribute_int("width");
    entry.height = xml_parser_get_attribute_int("height");
    entry.is_isometric = xml_parser_get_attribute_bool("isometric");

    const char *path = xml_parser_get_attribute_string("src");
    const char *group = xml_parser_get_attribute_string("group");
    const char *image_id = xml_parser_get_attribute_string("image");
    if (group && *group) {
        entry.has_full_image_ref = 1;
        entry.full_image_ref.type = RawReferenceType::GROUP_IMAGE;
        entry.full_image_ref.group_key = normalize_group_reference_key(group, g_parse_state.requested_key);
        entry.full_image_ref.image_id = image_id ? image_id : "";
    } else if (path && *path) {
        RawLayerDef base_layer;
        base_layer.reference.type = RawReferenceType::PNG_PATH;
        base_layer.reference.path = normalize_path_key(path);
        entry.layers.push_back(std::move(base_layer));
    }

    g_parse_state.doc->ordered_ids.push_back(entry.id);
    auto inserted = g_parse_state.doc->entries.emplace(entry.id, std::move(entry));
    g_parse_state.current_entry = &inserted.first->second;
    return 1;
}

static int xml_start_layer(void)
{
    if (!g_parse_state.current_entry) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.current_entry->layers.push_back(parse_raw_layer(PART_BOTH, LAYER_MASK_NONE));
    return 1;
}

static int xml_start_animation(void)
{
    if (!g_parse_state.current_entry) {
        g_parse_state.error = 1;
        return 0;
    }
    RawAnimationDef &animation = g_parse_state.current_entry->animation;
    animation.present = 1;
    animation.metadata.num_sprites = xml_parser_get_attribute_int("frames");
    animation.metadata.speed_id = xml_parser_get_attribute_int("speed");
    animation.metadata.can_reverse = xml_parser_get_attribute_bool("reversible");
    animation.metadata.sprite_offset_x = xml_parser_get_attribute_int("x");
    animation.metadata.sprite_offset_y = xml_parser_get_attribute_int("y");
    animation.implicit_frame_count = animation.metadata.num_sprites;
    return 1;
}

static int xml_start_frame(void)
{
    if (!g_parse_state.current_entry || !g_parse_state.current_entry->animation.present) {
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.current_entry->animation.explicit_frames.push_back(parse_raw_layer(PART_BOTH, LAYER_MASK_NONE));
    return 1;
}

static const xml_parser_element kXmlElements[] = {
    { "assetlist", xml_start_assetlist, nullptr },
    { "image", xml_start_image, nullptr, "assetlist" },
    { "layer", xml_start_layer, nullptr, "image" },
    { "animation", xml_start_animation, nullptr, "image" },
    { "frame", xml_start_frame, nullptr, "animation" }
};

static std::unique_ptr<ImageGroupDoc> parse_group_doc(const char *xml_path, const std::string &path_key, xml_asset_source source)
{
    CrashContextScope crash_scope("image_group.parse_group_doc", xml_path);
    std::vector<char> buffer;
    if (!read_file_to_buffer(xml_path, buffer)) {
        return nullptr;
    }

    g_parse_state = {};
    g_parse_state.requested_key = path_key;
    g_parse_state.xml_path = xml_path;
    g_parse_state.source = source;

    if (!xml_parser_init(kXmlElements, static_cast<int>(sizeof(kXmlElements) / sizeof(kXmlElements[0])), 1)) {
        crash_context_report_error("Unable to initialize image group parser", xml_path);
        return nullptr;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();

    if (!parsed || g_parse_state.error || !g_parse_state.doc) {
        return nullptr;
    }
    return std::move(g_parse_state.doc);
}

static const ImageGroupDoc *load_group_doc(const std::string &group_key, xml_asset_source source)
{
    if (const ImageGroupDoc *existing = find_group_doc(group_key, source)) {
        return existing;
    }

    const std::string cache_key = make_source_doc_cache_key(group_key, source);
    if (g_failed_group_docs.find(cache_key) != g_failed_group_docs.end()) {
        return nullptr;
    }

    char xml_path[FILE_NAME_MAX] = { 0 };
    xml_asset_source resolved_source = XML_ASSET_SOURCE_AUTO;
    if (!xml_resolve_assetlist_path(xml_path, group_key.c_str(), source, &resolved_source) || resolved_source != source) {
        g_failed_group_docs.insert(cache_key);
        return nullptr;
    }

    std::unique_ptr<ImageGroupDoc> doc = parse_group_doc(xml_path, group_key, source);
    if (!doc) {
        g_failed_group_docs.insert(cache_key);
        return nullptr;
    }

    const ImageGroupDoc *doc_ptr = doc.get();
    g_group_docs.emplace(cache_key, std::move(doc));
    return doc_ptr;
}

static const MergedImageGroup *load_merged_group(const std::string &group_key)
{
    if (const MergedImageGroup *existing = find_merged_group(group_key)) {
        return existing;
    }
    if (g_failed_merged_groups.find(group_key) != g_failed_merged_groups.end()) {
        return nullptr;
    }

    xml_asset_source sources[8] = {};
    const int source_count = xml_collect_assetlist_sources(group_key.c_str(), sources, static_cast<int>(sizeof(sources) / sizeof(sources[0])));
    if (source_count <= 0) {
        g_failed_merged_groups.insert(group_key);
        return nullptr;
    }

    std::unique_ptr<MergedImageGroup> merged = std::make_unique<MergedImageGroup>();
    merged->key = group_key;

    std::unordered_set<std::string> seen_ids;
    for (int i = 0; i < source_count; i++) {
        const ImageGroupDoc *doc = load_group_doc(group_key, sources[i]);
        if (!doc) {
            g_failed_merged_groups.insert(group_key);
            return nullptr;
        }
        if (merged->xml_path.empty()) {
            merged->xml_path = doc->xml_path;
        }

        for (const std::string &image_id : doc->ordered_ids) {
            if (!seen_ids.insert(image_id).second) {
                continue;
            }
            merged->ordered_ids.push_back(image_id);
            merged->winning_sources.emplace(image_id, doc->source);
        }
    }

    const MergedImageGroup *merged_ptr = merged.get();
    g_merged_groups.emplace(group_key, std::move(merged));
    return merged_ptr;
}

static const ResolvedImageEntry *materialize_source_entry(const std::string &group_key, xml_asset_source source, const std::string &image_id);

static const ResolvedImageEntry *materialize_merged_entry(const std::string &group_key, const std::string &image_id)
{
    const MergedImageGroup *merged = load_merged_group(group_key);
    if (!merged) {
        return nullptr;
    }
    auto it = merged->winning_sources.find(image_id);
    if (it == merged->winning_sources.end()) {
        crash_context_report_error("Unable to resolve merged image id", image_id.c_str());
        return nullptr;
    }
    return materialize_source_entry(group_key, it->second, image_id);
}

// SECTION: materializer
static int prepare_png_layer(
    const ImageGroupDoc &doc,
    const RawLayerDef &raw_layer,
    PreparedLayer &out_layer)
{
    char full_path[FILE_NAME_MAX] = { 0 };
    if (!xml_resolve_image_path(full_path, doc.key.c_str(), raw_layer.reference.path.c_str(), doc.source)) {
        crash_context_report_error("Unable to resolve image group png", raw_layer.reference.path.c_str());
        return 0;
    }

    RasterSurface source_surface;
    if (!load_png_raster_from_path(full_path, source_surface)) {
        crash_context_report_error("Unable to load image group png", full_path);
        return 0;
    }

    out_layer.surface = crop_surface(source_surface, raw_layer.src_x, raw_layer.src_y, raw_layer.width, raw_layer.height);
    if (!raster_has_pixels(out_layer.surface)) {
        crash_context_report_error("Unable to crop image group png", raw_layer.reference.path.c_str());
        return 0;
    }
    if (raw_layer.mask == LAYER_MASK_GRAYSCALE) {
        convert_to_grayscale(out_layer.surface);
    }

    out_layer.x_offset = raw_layer.x_offset;
    out_layer.y_offset = raw_layer.y_offset;
    out_layer.invert = raw_layer.invert;
    out_layer.rotate = raw_layer.rotate;
    out_layer.mask = raw_layer.mask;
    return 1;
}

static int prepare_group_image_layer(
    const ImageGroupDoc &doc,
    const RawLayerDef &raw_layer,
    PreparedLayer &out_layer)
{
    const std::string target_group = normalize_group_reference_key(raw_layer.reference.group_key.c_str(), doc.key);
    if (target_group.empty() || raw_layer.reference.image_id.empty()) {
        return 0;
    }

    const ResolvedImageEntry *resolved = materialize_merged_entry(target_group, raw_layer.reference.image_id);
    if (!resolved) {
        crash_context_report_error("Unable to resolve referenced image entry", raw_layer.reference.image_id.c_str());
        return 0;
    }

    out_layer.surface = surface_from_resolved_entry_part(*resolved, raw_layer.part);
    if (!raster_has_pixels(out_layer.surface)) {
        crash_context_report_error("Unable to reconstruct referenced image layer", raw_layer.reference.image_id.c_str());
        return 0;
    }
    if (raw_layer.mask == LAYER_MASK_GRAYSCALE) {
        convert_to_grayscale(out_layer.surface);
    }

    out_layer.x_offset = raw_layer.x_offset;
    out_layer.y_offset = raw_layer.y_offset;
    out_layer.invert = raw_layer.invert;
    out_layer.rotate = raw_layer.rotate;
    out_layer.mask = raw_layer.mask;
    return 1;
}

static int prepare_layer(const ImageGroupDoc &doc, const RawLayerDef &raw_layer, PreparedLayer &out_layer)
{
    switch (raw_layer.reference.type) {
        case RawReferenceType::PNG_PATH:
            return prepare_png_layer(doc, raw_layer, out_layer);
        case RawReferenceType::GROUP_IMAGE:
            return prepare_group_image_layer(doc, raw_layer, out_layer);
        case RawReferenceType::NONE:
        default:
            return 0;
    }
}

static int prepare_full_image_reference_surface(
    const ImageGroupDoc &doc,
    const ImageEntryDef &entry,
    RasterSurface &out_surface,
    const ResolvedImageEntry **out_referenced_entry)
{
    *out_referenced_entry = nullptr;
    if (!entry.has_full_image_ref) {
        return 1;
    }

    if (entry.full_image_ref.type != RawReferenceType::GROUP_IMAGE || entry.full_image_ref.image_id.empty()) {
        crash_context_report_error("Invalid full image reference", entry.id.c_str());
        return 0;
    }

    const std::string target_group = normalize_group_reference_key(entry.full_image_ref.group_key.c_str(), doc.key);
    const ResolvedImageEntry *resolved = materialize_merged_entry(target_group, entry.full_image_ref.image_id);
    if (!resolved) {
        crash_context_report_error("Unable to resolve referenced image entry", entry.full_image_ref.image_id.c_str());
        return 0;
    }

    out_surface = surface_from_resolved_entry_part(*resolved, PART_BOTH);
    if (!raster_has_pixels(out_surface)) {
        crash_context_report_error("Unable to reconstruct referenced full image", entry.full_image_ref.image_id.c_str());
        return 0;
    }
    *out_referenced_entry = resolved;
    return 1;
}

static int finalize_surface_to_resolved_entry(
    const ImageGroupDoc &doc,
    const ImageEntryDef &entry,
    RasterSurface &&composed_surface,
    ResolvedImageEntry &out_entry)
{
    out_entry.is_isometric = entry.is_isometric;

    RasterSurface split_surface;
    int top_height = 0;
    if (entry.is_isometric && has_top_part(composed_surface)) {
        const int tiles = (composed_surface.width + 2) / (FOOTPRINT_WIDTH + 2);
        const int footprint_height = tiles * FOOTPRINT_HEIGHT;
        if (footprint_height > 0 && composed_surface.height > footprint_height / 2) {
            top_height = composed_surface.height - footprint_height / 2;
            split_top_and_footprint(composed_surface, top_height, split_surface);
        }
    }
    if (!raster_has_pixels(split_surface)) {
        split_surface = std::move(composed_surface);
        top_height = 0;
    }

    const std::string payload_key = make_entry_payload_key(doc.key, doc.source, entry.id);
    if (!upload_split_surface(payload_key, split_surface, top_height, entry.is_isometric, out_entry.image_key, out_entry.top_image_key)) {
        crash_context_report_error("Unable to upload resolved image group entry", entry.id.c_str());
        return 0;
    }

    out_entry.split_width = split_surface.width;
    out_entry.split_height = split_surface.height;
    out_entry.top_height = top_height;
    out_entry.split_pixels = std::move(split_surface.pixels);
    return 1;
}

static int materialize_explicit_frame(
    const ImageGroupDoc &doc,
    const ImageEntryDef &entry,
    int frame_index,
    const RawLayerDef &frame_def,
    std::string &out_frame_key)
{
    if (is_bare_group_reference(frame_def)) {
        const std::string target_group = normalize_group_reference_key(frame_def.reference.group_key.c_str(), doc.key);
        const ResolvedImageEntry *resolved = materialize_merged_entry(target_group, frame_def.reference.image_id);
        if (!resolved || resolved->image_key.empty()) {
            return 0;
        }
        image_payload_retain_key(resolved->image_key.c_str());
        out_frame_key = resolved->image_key;
        return 1;
    }

    PreparedLayer frame_layer;
    if (!prepare_layer(doc, frame_def, frame_layer)) {
        return 0;
    }

    const int frame_width = transformed_layer_width(frame_layer) + std::max(0, frame_layer.x_offset);
    const int frame_height = transformed_layer_height(frame_layer) + std::max(0, frame_layer.y_offset);
    if (frame_width <= 0 || frame_height <= 0) {
        return 0;
    }

    RasterSurface frame_surface;
    frame_surface.width = frame_width;
    frame_surface.height = frame_height;
    frame_surface.pixels.resize(static_cast<size_t>(frame_width) * frame_height);
    memset(frame_surface.pixels.data(), 0, sizeof(color_t) * frame_surface.pixels.size());
    compose_prepared_layer(frame_surface, frame_layer, frame_layer.mask == LAYER_MASK_ALPHA);

    image frame_image = {};
    frame_image.width = frame_surface.width;
    frame_image.height = frame_surface.height;
    frame_image.original.width = frame_surface.width;
    frame_image.original.height = frame_surface.height;
    out_frame_key = make_entry_payload_key(doc.key, doc.source, entry.id) + "\\frame_" + std::to_string(frame_index);
    if (!image_payload_load_pixels_payload(
            out_frame_key.c_str(),
            frame_image,
            frame_surface.pixels.data(),
            frame_surface.width,
            frame_surface.height)) {
        out_frame_key.clear();
        return 0;
    }
    return 1;
}

static int resolve_animation(
    const ImageGroupDoc &doc,
    const ImageEntryDef &entry,
    const ResolvedImageEntry *referenced_entry,
    ResolvedImageEntry &out_entry)
{
    if (!entry.animation.present) {
        if (referenced_entry && referenced_entry->has_animation) {
            out_entry.animation = referenced_entry->animation;
            out_entry.has_animation = referenced_entry->has_animation;
            for (const std::string &frame_key : referenced_entry->animation_frame_keys) {
                if (!frame_key.empty()) {
                    image_payload_retain_key(frame_key.c_str());
                    out_entry.animation_frame_keys.push_back(frame_key);
                }
            }
        }
        return 1;
    }

    out_entry.animation = entry.animation.metadata;
    if (!entry.animation.explicit_frames.empty()) {
        for (size_t i = 0; i < entry.animation.explicit_frames.size(); i++) {
            std::string frame_key;
            if (!materialize_explicit_frame(doc, entry, static_cast<int>(i), entry.animation.explicit_frames[i], frame_key)) {
                return 0;
            }
            out_entry.animation_frame_keys.push_back(std::move(frame_key));
        }
    } else if (entry.animation.implicit_frame_count > 0) {
        for (int i = 1; i <= entry.animation.implicit_frame_count; i++) {
            const size_t next_index = static_cast<size_t>(entry.local_order + i);
            if (next_index >= doc.ordered_ids.size()) {
                break;
            }
            const std::string &frame_id = doc.ordered_ids[next_index];
            const ResolvedImageEntry *frame_entry = materialize_source_entry(doc.key, doc.source, frame_id);
            if (!frame_entry || frame_entry->image_key.empty()) {
                return 0;
            }
            image_payload_retain_key(frame_entry->image_key.c_str());
            out_entry.animation_frame_keys.push_back(frame_entry->image_key);
        }
    }

    out_entry.animation.num_sprites = static_cast<int>(out_entry.animation_frame_keys.size());
    out_entry.has_animation = !out_entry.animation_frame_keys.empty();
    return 1;
}

static const ResolvedImageEntry *materialize_source_entry(const std::string &group_key, xml_asset_source source, const std::string &image_id)
{
    if (const ResolvedImageEntry *existing = find_resolved_entry(group_key, source, image_id)) {
        return existing;
    }

    const std::string selector_key = make_selector_cache_key(group_key, source, image_id);
    if (g_failed_resolved_entries.find(selector_key) != g_failed_resolved_entries.end()) {
        return nullptr;
    }
    if (g_loading_resolved_entries.find(selector_key) != g_loading_resolved_entries.end()) {
        crash_context_report_error("Detected recursive image entry resolution", selector_key.c_str());
        g_failed_resolved_entries.insert(selector_key);
        return nullptr;
    }

    const ImageGroupDoc *doc = load_group_doc(group_key, source);
    if (!doc) {
        g_failed_resolved_entries.insert(selector_key);
        return nullptr;
    }
    auto entry_it = doc->entries.find(image_id);
    if (entry_it == doc->entries.end()) {
        crash_context_report_error("Unable to resolve source image id", selector_key.c_str());
        g_failed_resolved_entries.insert(selector_key);
        return nullptr;
    }
    const ImageEntryDef &entry = entry_it->second;

    g_loading_resolved_entries.insert(selector_key);

    ResolvedImageEntry resolved_entry;
    RasterSurface full_reference_surface;
    const ResolvedImageEntry *referenced_entry = nullptr;
    int ok = prepare_full_image_reference_surface(*doc, entry, full_reference_surface, &referenced_entry);
    int canvas_width = entry.width;
    int canvas_height = entry.height;
    if (ok && raster_has_pixels(full_reference_surface)) {
        if (canvas_width <= 0) {
            canvas_width = full_reference_surface.width;
        }
        if (canvas_height <= 0) {
            canvas_height = full_reference_surface.height;
        }
    }

    std::vector<PreparedLayer> prepared_layers;
    if (ok) {
        prepared_layers.reserve(entry.layers.size());
        for (const RawLayerDef &raw_layer : entry.layers) {
            PreparedLayer prepared_layer;
            if (!prepare_layer(*doc, raw_layer, prepared_layer)) {
                ok = 0;
                break;
            }
            if (canvas_width <= 0) {
                canvas_width = transformed_layer_width(prepared_layer);
            }
            if (canvas_height <= 0) {
                canvas_height = transformed_layer_height(prepared_layer);
            }
            prepared_layers.push_back(std::move(prepared_layer));
        }
    }

    if (ok && (canvas_width <= 0 || canvas_height <= 0)) {
        crash_context_report_error("Resolved image group entry has invalid canvas size", selector_key.c_str());
        ok = 0;
    }

    if (ok) {
        RasterSurface composed_surface;
        composed_surface.width = canvas_width;
        composed_surface.height = canvas_height;
        composed_surface.pixels.resize(static_cast<size_t>(canvas_width) * canvas_height);
        memset(composed_surface.pixels.data(), 0, sizeof(color_t) * composed_surface.pixels.size());

        if (raster_has_pixels(full_reference_surface)) {
            PreparedLayer base_layer;
            base_layer.surface = std::move(full_reference_surface);
            compose_prepared_layer(composed_surface, base_layer, 0);
        }

        const int has_alpha_mask = std::any_of(
            prepared_layers.begin(),
            prepared_layers.end(),
            [](const PreparedLayer &layer) { return layer.mask == LAYER_MASK_ALPHA; });

        if (has_alpha_mask) {
            for (const PreparedLayer &layer : prepared_layers) {
                compose_prepared_layer(composed_surface, layer, 1);
            }
        } else {
            for (auto it = prepared_layers.rbegin(); it != prepared_layers.rend(); ++it) {
                compose_prepared_layer(composed_surface, *it, 0);
            }
        }

        ok = finalize_surface_to_resolved_entry(*doc, entry, std::move(composed_surface), resolved_entry);
    }

    if (ok) {
        ok = resolve_animation(*doc, entry, referenced_entry, resolved_entry);
    }

    g_loading_resolved_entries.erase(selector_key);
    if (!ok) {
        release_resolved_entry_resources(resolved_entry);
        g_failed_resolved_entries.insert(selector_key);
        return nullptr;
    }

    std::unique_ptr<ResolvedImageEntry> stored_entry = std::make_unique<ResolvedImageEntry>(std::move(resolved_entry));
    const ResolvedImageEntry *stored_ptr = stored_entry.get();
    g_resolved_entries.emplace(selector_key, std::move(stored_entry));
    return stored_ptr;
}

} // namespace

ImageGroupPayload::ImageGroupPayload(std::string key, std::string xml_path, xml_asset_source source)
    : key_(std::move(key))
    , xml_path_(std::move(xml_path))
    , source_(source)
{
}

ImageGroupPayload::~ImageGroupPayload()
{
    for (const auto &entry_pair : images_) {
        const ImageGroupEntry &entry = entry_pair.second;
        if (!entry.image_key().empty()) {
            image_payload_release_key(entry.image_key().c_str());
        }
        if (!entry.top_image_key().empty()) {
            image_payload_release_key(entry.top_image_key().c_str());
        }
        for (const std::string &frame_key : entry.animation_frame_keys()) {
            if (!frame_key.empty()) {
                image_payload_release_key(frame_key.c_str());
            }
        }
    }
}

const std::string &ImageGroupPayload::key() const
{
    return key_;
}

const std::string &ImageGroupPayload::xml_path() const
{
    return xml_path_;
}

xml_asset_source ImageGroupPayload::source() const
{
    return source_;
}

void ImageGroupPayload::set_image(const std::string &image_id, const std::string &image_key, const std::string &top_image_key)
{
    if (default_image_id_.empty()) {
        default_image_id_ = image_id;
    }

    if (!image_key.empty()) {
        image_payload_retain_key(image_key.c_str());
    }
    if (!top_image_key.empty()) {
        image_payload_retain_key(top_image_key.c_str());
    }

    auto it = images_.find(image_id);
    if (it != images_.end()) {
        if (!it->second.image_key().empty()) {
            image_payload_release_key(it->second.image_key().c_str());
        }
        if (!it->second.top_image_key().empty()) {
            image_payload_release_key(it->second.top_image_key().c_str());
        }
        for (const std::string &frame_key : it->second.animation_frame_keys()) {
            if (!frame_key.empty()) {
                image_payload_release_key(frame_key.c_str());
            }
        }
        it->second.set_keys(image_key, top_image_key);
        it->second.set_animation({}, {});
        return;
    }

    image_order_.push_back(image_id);
    images_.emplace(image_id, ImageGroupEntry(image_id, image_key, top_image_key));
}

void ImageGroupPayload::set_image_raster(
    const std::string &image_id,
    std::vector<color_t> combined_pixels,
    int combined_width,
    int combined_height,
    int top_height)
{
    auto it = images_.find(image_id);
    if (it == images_.end()) {
        return;
    }
    it->second.set_raster(std::move(combined_pixels), combined_width, combined_height, top_height);
}

void ImageGroupPayload::set_image_animation(
    const std::string &image_id,
    const image_animation &animation,
    std::vector<std::string> frame_image_keys)
{
    auto it = images_.find(image_id);
    if (it == images_.end()) {
        return;
    }
    for (const std::string &frame_key : frame_image_keys) {
        if (!frame_key.empty()) {
            image_payload_retain_key(frame_key.c_str());
        }
    }
    for (const std::string &frame_key : it->second.animation_frame_keys()) {
        if (!frame_key.empty()) {
            image_payload_release_key(frame_key.c_str());
        }
    }
    it->second.set_animation(animation, std::move(frame_image_keys));
}

const char *ImageGroupPayload::image_key_for(const char *image_id) const
{
    const ImageGroupEntry *entry = entry_for(image_id);
    return entry ? entry->image_key().c_str() : nullptr;
}

const char *ImageGroupPayload::top_image_key_for(const char *image_id) const
{
    const ImageGroupEntry *entry = entry_for(image_id);
    return (!entry || entry->top_image_key().empty()) ? nullptr : entry->top_image_key().c_str();
}

const ImageGroupEntry *ImageGroupPayload::entry_for(const char *image_id) const
{
    if (!image_id || !*image_id) {
        return nullptr;
    }
    auto it = images_.find(image_id);
    return it == images_.end() ? nullptr : &it->second;
}

const char *ImageGroupPayload::image_id_at_index(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= image_order_.size()) {
        return nullptr;
    }
    return image_order_[static_cast<size_t>(index)].c_str();
}

const image *ImageGroupPayload::legacy_image_for(const char *image_id) const
{
    const ImageGroupEntry *entry = entry_for(image_id);
    return entry ? entry->legacy_image() : nullptr;
}

const image *ImageGroupPayload::legacy_image_at_index(int index) const
{
    const char *image_id = image_id_at_index(index);
    return image_id ? legacy_image_for(image_id) : nullptr;
}

const image *ImageGroupPayload::animation_frame_for(const char *image_id, int animation_offset) const
{
    const ImageGroupEntry *entry = entry_for(image_id);
    return (!entry || animation_offset <= 0) ? nullptr : entry->animation_frame(animation_offset);
}

const image *ImageGroupPayload::default_legacy_image() const
{
    if (default_image_id_.empty()) {
        return nullptr;
    }
    return legacy_image_for(default_image_id_.c_str());
}

const char *ImageGroupPayload::default_image_id() const
{
    return default_image_id_.empty() ? nullptr : default_image_id_.c_str();
}

const ImageGroupPayload *image_group_payload_get(const char *path_key)
{
    const std::string normalized_key = normalize_path_key(path_key);
    auto it = g_group_payloads.find(normalized_key);
    return it == g_group_payloads.end() ? nullptr : it->second.get();
}

extern "C" int image_group_payload_load(const char *path_key)
{
    const std::string normalized_key = normalize_path_key(path_key);
    if (normalized_key.empty()) {
        return 0;
    }
    CrashContextScope crash_scope("image_group.load", normalized_key.c_str());
    if (image_group_payload_get(normalized_key.c_str())) {
        return 1;
    }
    if (g_failed_group_payloads.find(normalized_key) != g_failed_group_payloads.end()) {
        return 0;
    }
    if (g_loading_group_payloads.find(normalized_key) != g_loading_group_payloads.end()) {
        crash_context_report_error("Detected recursive image group load", normalized_key.c_str());
        g_failed_group_payloads.insert(normalized_key);
        return 0;
    }

    g_loading_group_payloads.insert(normalized_key);

    const MergedImageGroup *merged = load_merged_group(normalized_key);
    if (!merged) {
        g_loading_group_payloads.erase(normalized_key);
        g_failed_group_payloads.insert(normalized_key);
        return 0;
    }

    xml_asset_source payload_source = XML_ASSET_SOURCE_AUTO;
    if (!merged->ordered_ids.empty()) {
        auto first_source = merged->winning_sources.find(merged->ordered_ids.front());
        if (first_source != merged->winning_sources.end()) {
            payload_source = first_source->second;
        }
    }

    std::unique_ptr<ImageGroupPayload> payload = std::make_unique<ImageGroupPayload>(
        merged->key,
        merged->xml_path,
        payload_source);

    for (const std::string &image_id : merged->ordered_ids) {
        auto source_it = merged->winning_sources.find(image_id);
        if (source_it == merged->winning_sources.end()) {
            continue;
        }
        const ResolvedImageEntry *resolved = materialize_source_entry(normalized_key, source_it->second, image_id);
        if (!resolved || resolved->image_key.empty()) {
            crash_context_report_error("Building graphics image id could not be resolved", image_id.c_str());
            g_loading_group_payloads.erase(normalized_key);
            g_failed_group_payloads.insert(normalized_key);
            return 0;
        }

        payload->set_image(image_id, resolved->image_key, resolved->top_image_key);
        payload->set_image_raster(image_id, resolved->split_pixels, resolved->split_width, resolved->split_height, resolved->top_height);
        if (resolved->has_animation) {
            payload->set_image_animation(image_id, resolved->animation, resolved->animation_frame_keys);
        }
    }

    g_loading_group_payloads.erase(normalized_key);
    g_failed_group_payloads.erase(normalized_key);
    g_group_payloads.emplace(normalized_key, std::move(payload));
    return 1;
}

extern "C" const image *image_group_payload_get_image(const char *path_key, const char *image_id)
{
    const ImageGroupPayload *payload = image_group_payload_get(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = image_group_payload_get(path_key);
    return payload ? payload->legacy_image_for(image_id) : nullptr;
}

extern "C" const image *image_group_payload_get_image_at_index(const char *path_key, int index)
{
    const ImageGroupPayload *payload = image_group_payload_get(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = image_group_payload_get(path_key);
    return payload ? payload->legacy_image_at_index(index) : nullptr;
}

extern "C" const image *image_group_payload_get_default_image(const char *path_key)
{
    const ImageGroupPayload *payload = image_group_payload_get(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = image_group_payload_get(path_key);
    return payload ? payload->default_legacy_image() : nullptr;
}

extern "C" const image *image_group_payload_get_animation_frame(const char *path_key, const char *image_id, int animation_offset)
{
    const ImageGroupPayload *payload = image_group_payload_get(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = image_group_payload_get(path_key);
    return payload ? payload->animation_frame_for(image_id, animation_offset) : nullptr;
}

extern "C" const char *image_group_payload_get_default_image_id(const char *path_key)
{
    const ImageGroupPayload *payload = image_group_payload_get(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = image_group_payload_get(path_key);
    return payload ? payload->default_image_id() : nullptr;
}

extern "C" const char *image_group_payload_get_image_id_at_index(const char *path_key, int index)
{
    const ImageGroupPayload *payload = image_group_payload_get(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = image_group_payload_get(path_key);
    return payload ? payload->image_id_at_index(index) : nullptr;
}

extern "C" const char *image_group_payload_get_image_key(const char *path_key, const char *image_id)
{
    const ImageGroupPayload *payload = image_group_payload_get(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = image_group_payload_get(path_key);
    return payload ? payload->image_key_for(image_id) : nullptr;
}

extern "C" void image_group_payload_clear_all(void)
{
    g_group_payloads.clear();
    for (const auto &entry_pair : g_resolved_entries) {
        release_resolved_entry_resources(*entry_pair.second);
    }
    g_resolved_entries.clear();
    g_merged_groups.clear();
    g_group_docs.clear();
    g_png_rasters.clear();
    g_failed_group_payloads.clear();
    g_loading_group_payloads.clear();
    g_failed_group_docs.clear();
    g_failed_merged_groups.clear();
    g_failed_resolved_entries.clear();
    g_loading_resolved_entries.clear();
    g_failed_png_rasters.clear();
}
