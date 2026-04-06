#include "assets/image_group_payload_internal.h"

#include "core/crash_context.h"
#include "core/image_payload.h"

extern "C" {
#include "core/file.h"
#include "core/png_read.h"
}

#include <algorithm>
#include <cstring>
#include <memory>
#include <utility>

namespace image_group_payload_internal {

namespace {

// Input: one uploaded payload object plus one logical slice template.
// Output: a runtime slice populated from the payload's actual renderer handle and logical dimensions.
int populate_slice_from_payload(const ImagePayload *payload, int is_isometric, RuntimeDrawSlice &out_slice)
{
    out_slice = RuntimeDrawSlice();
    if (!payload) {
        return 0;
    }

    out_slice.handle = payload->handle();
    out_slice.width = payload->legacy().width;
    out_slice.height = payload->legacy().height;
    out_slice.is_isometric = is_isometric;
    return out_slice.is_valid();
}

// Input: one resolved entry that already stores split-source pixels.
// Output: the logical combined-canvas height used by legacy isometric references.
int derive_original_height(const ResolvedImageEntry &entry)
{
    if (!entry.is_isometric || entry.top_height <= 0) {
        return entry.split_height;
    }
    const int footprint_height = entry.split_height - entry.top_height;
    return entry.top_height + footprint_height / 2;
}

struct CroppedRuntimeSurface {
    RasterSurface surface;
    int x_offset = 0;
    int y_offset = 0;
};

// Input: one runtime-composed raster surface.
// Output: the opaque-bounds crop plus renderer placement offsets for top-flush, bottom-padded runtime footprints.
int crop_runtime_surface_like_legacy(const RasterSurface &source, CroppedRuntimeSurface &out_surface)
{
    out_surface = CroppedRuntimeSurface();
    if (!raster_has_pixels(source)) {
        return 0;
    }

    int x_first_opaque = source.width;
    int y_first_opaque = source.height;
    int x_last_opaque = -1;
    int y_last_opaque = -1;

    for (int y = 0; y < source.height; y++) {
        const color_t *row = &source.pixels[static_cast<size_t>(y) * source.width];
        for (int x = 0; x < source.width; x++) {
            if ((row[x] & COLOR_CHANNEL_ALPHA) != ALPHA_TRANSPARENT) {
                x_first_opaque = std::min(x_first_opaque, x);
                y_first_opaque = std::min(y_first_opaque, y);
                x_last_opaque = std::max(x_last_opaque, x);
                y_last_opaque = std::max(y_last_opaque, y);
            }
        }
    }

    if (x_last_opaque < 0 || y_last_opaque < 0) {
        return 0;
    }

    const int bottom_padding = source.height - y_last_opaque - 1;

    out_surface.x_offset = x_first_opaque;
    // Runtime-extracted footprint rasters stay top-flush and carry transparent padding below the diamond.
    // Cancel that trailing padding before the renderer sees the slice so the footprint anchors to the tile.
    out_surface.y_offset = y_first_opaque - bottom_padding;
    out_surface.surface.width = x_last_opaque - x_first_opaque + 1;
    out_surface.surface.height = y_last_opaque - y_first_opaque + 1;
    out_surface.surface.pixels.resize(
        static_cast<size_t>(out_surface.surface.width) * out_surface.surface.height);

    for (int y = 0; y < out_surface.surface.height; y++) {
        memcpy(
            &out_surface.surface.pixels[static_cast<size_t>(y) * out_surface.surface.width],
            &source.pixels[static_cast<size_t>(y_first_opaque + y) * source.width + x_first_opaque],
            sizeof(color_t) * out_surface.surface.width);
    }

    return raster_has_pixels(out_surface.surface);
}

// Input: one source-local document and one raw PNG layer reference.
// Output: a prepared layer containing a cropped raster plus transform metadata.
int prepare_png_layer(const ImageGroupDoc &doc, const RawLayerDef &raw_layer, PreparedLayer &out_layer)
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

// Input: one source-local document and one symbolic group-image layer reference.
// Output: a prepared layer whose raster was reconstructed from a resolved entry part selection.
int prepare_group_image_layer(const ImageGroupDoc &doc, const RawLayerDef &raw_layer, PreparedLayer &out_layer)
{
    const std::string target_group = normalize_group_reference_key(raw_layer.reference.group_key.c_str(), doc.key);
    if (target_group.empty() || raw_layer.reference.image_id.empty()) {
        return 0;
    }

    const ResolvedImageEntry *resolved = materialize_merged_entry(target_group, doc.source, raw_layer.reference.image_id);
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

// Input: one source-local document and one symbolic layer definition.
// Output: a prepared layer with source pixels ready for composition, or false on resolution failure.
int prepare_layer(const ImageGroupDoc &doc, const RawLayerDef &raw_layer, PreparedLayer &out_layer)
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

// Input: one entry that may inherit a full image from another resolved entry.
// Output: the referenced full raster plus the referenced entry pointer when a full-image ref is present.
int prepare_full_image_reference_surface(
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
    const ResolvedImageEntry *resolved = materialize_merged_entry(target_group, doc.source, entry.full_image_ref.image_id);
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

// Input: one composed entry surface that is ready to become a runtime payload.
// Output: a resolved entry populated with split-source pixels and render-facing managed texture keys.
int finalize_surface_to_resolved_entry(
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
    if (!upload_split_surface(payload_key, split_surface, top_height, entry.is_isometric, out_entry.footprint, out_entry.top)) {
        crash_context_report_error("Unable to upload resolved image group entry", entry.id.c_str());
        return 0;
    }

    const int tile_span = split_surface.width > 0 ? (split_surface.width + 2) / (FOOTPRINT_WIDTH + 2) : 0;
    out_entry.tile_span = tile_span;
    if (tile_span > 0) {
        out_entry.footprint.slice.draw_offset_y += -FOOTPRINT_HALF_HEIGHT * (tile_span - 1);
    }
    if (top_height > 0) {
        out_entry.has_top = 1;
        out_entry.top.slice.draw_offset_y = -top_height + FOOTPRINT_HALF_HEIGHT;
    }

    out_entry.split_width = split_surface.width;
    out_entry.split_height = split_surface.height;
    out_entry.top_height = top_height;
    out_entry.split_pixels = std::move(split_surface.pixels);
    return 1;
}

// Input: one explicit animation frame definition.
// Output: a managed texture key for that frame, either by reusing a bare ref or by uploading a composed frame raster.
int materialize_explicit_frame(
    const ImageGroupDoc &doc,
    const ImageEntryDef &entry,
    int frame_index,
    const RawLayerDef &frame_def,
    std::string &out_frame_key)
{
    if (is_bare_group_reference(frame_def)) {
        const std::string target_group = normalize_group_reference_key(frame_def.reference.group_key.c_str(), doc.key);
        const ResolvedImageEntry *resolved = materialize_merged_entry(target_group, doc.source, frame_def.reference.image_id);
        if (!resolved || resolved->footprint.texture_key.empty()) {
            return 0;
        }
        image_payload_retain_key(resolved->footprint.texture_key.c_str());
        out_frame_key = resolved->footprint.texture_key;
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
    const ImagePayload *payload = image_payload_load_pixels_payload(
            out_frame_key.c_str(),
            frame_image,
            frame_surface.pixels.data(),
            frame_surface.width,
            frame_surface.height);
    if (!payload) {
        out_frame_key.clear();
        return 0;
    }
    return 1;
}

// Input: one fully resolved base entry plus an optional inherited full-image reference.
// Output: animation metadata and frame payload keys copied or materialized onto the resolved entry.
int resolve_animation(
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

    out_entry.animation.sprite_offset_x = entry.animation.metadata.sprite_offset_x;
    out_entry.animation.sprite_offset_y = entry.animation.metadata.sprite_offset_y;
    out_entry.animation.can_reverse = entry.animation.metadata.can_reverse;
    out_entry.animation.speed_id = entry.animation.metadata.speed_id;
    if (!entry.animation.explicit_frames.empty()) {
        for (size_t i = 0; i < entry.animation.explicit_frames.size(); i++) {
            std::string frame_key;
            if (!materialize_explicit_frame(doc, entry, static_cast<int>(i), entry.animation.explicit_frames[i], frame_key)) {
                return 0;
            }
            if (const ImagePayload *payload = image_payload_get(frame_key.c_str())) {
                RuntimeDrawSlice frame_slice;
                if (!populate_slice_from_payload(payload, 0, frame_slice)) {
                    crash_context_report_error("Animation frame materialized with an invalid runtime slice", frame_key.c_str());
                    return 0;
                }
                out_entry.animation.frames.push_back(frame_slice);
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
            if (!frame_entry || frame_entry->footprint.texture_key.empty()) {
                return 0;
            }
            image_payload_retain_key(frame_entry->footprint.texture_key.c_str());
            out_entry.animation_frame_keys.push_back(frame_entry->footprint.texture_key);
            out_entry.animation.frames.push_back(frame_entry->footprint.slice);
        }
    }

    out_entry.animation.num_frames = static_cast<int>(out_entry.animation.frames.size());
    out_entry.has_animation = !out_entry.animation.frames.empty();
    return 1;
}

} // namespace

// Input: one mutable raster surface.
// Output: no return value; pixels are converted in place to grayscale.
void convert_to_grayscale(RasterSurface &surface)
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

// Input: one absolute PNG path.
// Output: the cached or freshly loaded raster surface for that PNG, or false on load failure.
int load_png_raster_from_path(const char *full_path, RasterSurface &out_surface)
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

// Input: one raster surface.
// Output: true when width, height, and pixel count are internally consistent.
int raster_has_pixels(const RasterSurface &surface)
{
    return surface.width > 0 &&
        surface.height > 0 &&
        surface.pixels.size() == static_cast<size_t>(surface.width) * surface.height;
}

// Input: one loaded PNG/full raster plus crop rectangle values.
// Output: the cropped surface, or an empty surface when the rectangle is invalid.
RasterSurface crop_surface(const RasterSurface &source, int src_x, int src_y, int width, int height)
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

// Input: one resolved entry split buffer and a requested isometric part selection.
// Output: a renderer-independent raster surface for PART_TOP, PART_FOOTPRINT, or PART_BOTH.
RasterSurface surface_from_resolved_entry_part(const ResolvedImageEntry &entry, layer_isometric_part part)
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

// Input: one prepared layer.
// Output: the post-rotation width used when sizing the composition canvas.
int transformed_layer_width(const PreparedLayer &layer)
{
    return layer.rotate == ROTATE_90_DEGREES || layer.rotate == ROTATE_270_DEGREES ?
        layer.surface.height : layer.surface.width;
}

// Input: one prepared layer.
// Output: the post-rotation height used when sizing the composition canvas.
int transformed_layer_height(const PreparedLayer &layer)
{
    return layer.rotate == ROTATE_90_DEGREES || layer.rotate == ROTATE_270_DEGREES ?
        layer.surface.width : layer.surface.height;
}

// Input: one prepared layer and one destination-space coordinate.
// Output: the source pixel that should appear there after offsets/invert/rotate, or null if outside the layer.
const color_t *prepared_layer_get_color(const PreparedLayer &layer, int x, int y)
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

// Input: one mutable target surface plus one prepared layer in destination coordinates.
// Output: no return value; the layer is composited into the target using legacy alpha/mask semantics.
void compose_prepared_layer(RasterSurface &target, const PreparedLayer &layer, int has_alpha_mask)
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

// Input: one raw layer definition.
// Output: true when the layer is a plain group-image reference with no crop/offset/transform overrides.
int is_bare_group_reference(const RawLayerDef &layer)
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

// Input: one composed raster surface.
// Output: true when the surface contains a non-empty top section above the footprint rows.
int has_top_part(const RasterSurface &surface)
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

// Input: one combined isometric raster and the computed top height.
// Output: no return value; split_surface receives legacy-style top rows followed by footprint rows.
void split_top_and_footprint(const RasterSurface &source, int top_height, RasterSurface &split_surface)
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

// Input: one split raster plus top-height/isometric metadata and a payload base key.
// Output: renderer-facing managed payload keys for the footprint and optional top images.
int upload_split_surface(
    const std::string &base_key,
    const RasterSurface &split_surface,
    int top_height,
    int is_isometric,
    ResolvedTextureSlice &out_footprint,
    ResolvedTextureSlice &out_top)
{
    if (!raster_has_pixels(split_surface)) {
        return 0;
    }

    const int footprint_source_y = top_height > 0 ? top_height : 0;
    const int footprint_source_height = split_surface.height - footprint_source_y;
    RasterSurface footprint_source = crop_surface(
        split_surface,
        0,
        footprint_source_y,
        split_surface.width,
        footprint_source_height);
    if (!raster_has_pixels(footprint_source)) {
        return 0;
    }

    CroppedRuntimeSurface cropped_footprint;
    const RasterSurface *footprint_upload_surface = &footprint_source;
    if (is_isometric) {
        if (!crop_runtime_surface_like_legacy(footprint_source, cropped_footprint)) {
            return 0;
        }
        footprint_upload_surface = &cropped_footprint.surface;
    }

    if (top_height > 0) {
        const int footprint_height = footprint_source.height;
        const int original_canvas_height = top_height + footprint_height / 2;
        image base_image = {};
        base_image.width = footprint_upload_surface->width;
        base_image.height = footprint_upload_surface->height;
        base_image.original.width = split_surface.width;
        base_image.original.height = original_canvas_height;
        base_image.is_isometric = is_isometric;
        base_image.atlas.y_offset = top_height;
        base_image.x_offset = cropped_footprint.x_offset;
        base_image.y_offset = cropped_footprint.y_offset;

        const ImagePayload *footprint_payload = image_payload_load_pixels_payload(
                base_key.c_str(),
                base_image,
                footprint_upload_surface->pixels.data(),
                footprint_upload_surface->width,
                footprint_upload_surface->height);
        if (!footprint_payload) {
            return 0;
        }

        image top_image = {};
        top_image.width = split_surface.width;
        top_image.height = top_height;
        top_image.original.width = split_surface.width;
        top_image.original.height = top_height;
        const ImagePayload *top_payload = image_payload_load_pixels_payload(
                (base_key + "\\top").c_str(),
                top_image,
                split_surface.pixels.data(),
                split_surface.width,
                top_height);
        if (!top_payload) {
            image_payload_release_key(base_key.c_str());
            return 0;
        }

        out_footprint.texture_key = base_key;
        if (!populate_slice_from_payload(footprint_payload, is_isometric, out_footprint.slice)) {
            crash_context_report_error("Resolved image group footprint materialized with an invalid runtime slice", base_key.c_str());
            image_payload_release_key((base_key + "\\top").c_str());
            image_payload_release_key(base_key.c_str());
            return 0;
        }
        out_footprint.slice.draw_offset_x = base_image.x_offset;
        out_footprint.slice.draw_offset_y = base_image.y_offset;

        out_top.texture_key = base_key + "\\top";
        if (!populate_slice_from_payload(top_payload, 0, out_top.slice)) {
            crash_context_report_error("Resolved image group top materialized with an invalid runtime slice", (base_key + "\\top").c_str());
            image_payload_release_key((base_key + "\\top").c_str());
            image_payload_release_key(base_key.c_str());
            return 0;
        }
        return 1;
    }

    image base_image = {};
    base_image.width = footprint_upload_surface->width;
    base_image.height = footprint_upload_surface->height;
    base_image.original.width = split_surface.width;
    base_image.original.height = split_surface.height;
    base_image.is_isometric = is_isometric;
    base_image.x_offset = cropped_footprint.x_offset;
    base_image.y_offset = cropped_footprint.y_offset;
    const ImagePayload *footprint_payload = image_payload_load_pixels_payload(
            base_key.c_str(),
            base_image,
            footprint_upload_surface->pixels.data(),
            footprint_upload_surface->width,
            footprint_upload_surface->height);
    if (!footprint_payload) {
        return 0;
    }

    out_footprint.texture_key = base_key;
    if (!populate_slice_from_payload(footprint_payload, is_isometric, out_footprint.slice)) {
        crash_context_report_error("Resolved image group footprint materialized with an invalid runtime slice", base_key.c_str());
        image_payload_release_key(base_key.c_str());
        return 0;
    }
    out_footprint.slice.draw_offset_x = base_image.x_offset;
    out_footprint.slice.draw_offset_y = base_image.y_offset;
    out_top = {};
    return 1;
}

// Input: one source-local entry selector.
// Output: a resolved entry that owns split-source pixels plus render-facing payload keys, or null on failure.
const ResolvedImageEntry *materialize_source_entry(const std::string &group_key, xml_asset_source source, const std::string &image_id)
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

} // namespace image_group_payload_internal
