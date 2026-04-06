#pragma once

#include "assets/image_group_payload.h"

#include "assets/layer.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace image_group_payload_internal {

using GroupPayloadMap = std::unordered_map<std::string, std::unique_ptr<ImageGroupPayload>>;
using GroupDocMap = std::unordered_map<std::string, std::unique_ptr<struct ImageGroupDoc>>;
using MergedGroupMap = std::unordered_map<std::string, std::unique_ptr<struct MergedImageGroup>>;
using ResolvedEntryMap = std::unordered_map<std::string, std::unique_ptr<struct ResolvedImageEntry>>;
using PngRasterMap = std::unordered_map<std::string, std::unique_ptr<struct RasterSurface>>;

extern GroupPayloadMap g_group_payloads;
extern GroupDocMap g_group_docs;
extern MergedGroupMap g_merged_groups;
extern ResolvedEntryMap g_resolved_entries;
extern PngRasterMap g_png_rasters;

extern std::unordered_set<std::string> g_failed_group_payloads;
extern std::unordered_set<std::string> g_loading_group_payloads;
extern std::unordered_set<std::string> g_failed_group_docs;
extern std::unordered_set<std::string> g_failed_merged_groups;
extern std::unordered_set<std::string> g_failed_resolved_entries;
extern std::unordered_set<std::string> g_loading_resolved_entries;
extern std::unordered_set<std::string> g_failed_png_rasters;

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
    std::vector<xml_asset_source> source_chain;
    std::vector<std::pair<xml_asset_source, std::string>> ordered_entries;
};

struct ResolvedTextureSlice {
    std::string texture_key;
    RuntimeDrawSlice slice;
};

struct ResolvedImageEntry {
    ResolvedTextureSlice footprint;
    ResolvedTextureSlice top;
    RuntimeAnimationTrack animation;
    std::vector<std::string> animation_frame_keys;
    int has_top = 0;
    int has_animation = 0;
    std::vector<color_t> split_pixels;
    int split_width = 0;
    int split_height = 0;
    int top_height = 0;
    int is_isometric = 0;
    int tile_span = 0;
};

struct PreparedLayer {
    RasterSurface surface;
    int x_offset = 0;
    int y_offset = 0;
    layer_invert_type invert = INVERT_NONE;
    layer_rotate_type rotate = ROTATE_NONE;
    layer_mask mask = LAYER_MASK_NONE;
};

// Input: raw path text from XML or public API that may be null, spaced, or use '/' separators.
// Output: canonical cache key text using '\' separators and no duplicate/trailing whitespace noise.
std::string normalize_path_key(const char *text);

// Input: an XML group reference and the current group key used for "this" references.
// Output: the normalized target group key that later stages should resolve against.
std::string normalize_group_reference_key(const char *group_name, const std::string &current_group);

// Input: one group key and one source enum.
// Output: the stable cache key for a source-local parsed document.
std::string make_source_doc_cache_key(const std::string &group_key, xml_asset_source source);

// Input: one group key, one winning source, and one image id.
// Output: the stable cache key for a resolved entry selector.
std::string make_selector_cache_key(const std::string &group_key, xml_asset_source source, const std::string &image_id);

// Input: one group key, one source, and one image id.
// Output: the unique managed-texture payload key used for uploaded runtime textures.
std::string make_entry_payload_key(const std::string &group_key, xml_asset_source source, const std::string &image_id);

// Input: a group key and source.
// Output: the cached source-local document if it exists, otherwise null.
const ImageGroupDoc *find_group_doc(const std::string &group_key, xml_asset_source source);

// Input: a merged group key.
// Output: the cached merged group if it exists, otherwise null.
const MergedImageGroup *find_merged_group(const std::string &group_key);

// Input: a group key, source, and image id selector.
// Output: the cached resolved entry if it exists, otherwise null.
const ResolvedImageEntry *find_resolved_entry(const std::string &group_key, xml_asset_source source, const std::string &image_id);

// Input: one resolved entry whose managed texture keys were retained during materialization.
// Output: no return value; all retained payload keys owned by that resolved entry are released.
void release_resolved_entry_resources(const ResolvedImageEntry &entry);

// Input: a source-local XML document path, requested group key, and source.
// Output: a parsed document that contains unresolved symbolic entry data only, or null on parse failure.
const ImageGroupDoc *load_group_doc(const std::string &group_key, xml_asset_source source);

// Input: a group key.
// Output: the merged entry-id namespace across the source chain for that group, or null on failure.
const MergedImageGroup *load_merged_group(const std::string &group_key);

// Input: a merged group key and image id.
// Output: the resolved entry for the winning source of that id, or null on resolution/materialization failure.
const ResolvedImageEntry *materialize_merged_entry(const std::string &group_key, xml_asset_source preferred_source, const std::string &image_id);

// Input: a source-local entry selector.
// Output: a resolved entry that owns split-source pixels plus render-facing payload keys, or null on failure.
const ResolvedImageEntry *materialize_source_entry(const std::string &group_key, xml_asset_source source, const std::string &image_id);

// Input: one resolved entry split buffer and a requested isometric part selection.
// Output: a renderer-independent raster surface for PART_TOP, PART_FOOTPRINT, or PART_BOTH.
RasterSurface surface_from_resolved_entry_part(const ResolvedImageEntry &entry, layer_isometric_part part);

// Input: one raster surface.
// Output: true when width, height, and pixel count are internally consistent.
int raster_has_pixels(const RasterSurface &surface);

// Input: one loaded PNG/full raster plus crop rectangle values.
// Output: the cropped surface, or an empty surface when the rectangle is invalid.
RasterSurface crop_surface(const RasterSurface &source, int src_x, int src_y, int width, int height);

// Input: one mutable raster surface.
// Output: no return value; pixels are converted in place to grayscale.
void convert_to_grayscale(RasterSurface &surface);

// Input: one absolute PNG path.
// Output: the cached or freshly loaded raster surface for that PNG, or false on load failure.
int load_png_raster_from_path(const char *full_path, RasterSurface &out_surface);

// Input: one prepared layer.
// Output: the post-rotation width used when sizing the composition canvas.
int transformed_layer_width(const PreparedLayer &layer);

// Input: one prepared layer.
// Output: the post-rotation height used when sizing the composition canvas.
int transformed_layer_height(const PreparedLayer &layer);

// Input: one prepared layer and one destination-space coordinate.
// Output: the source pixel that should appear there after offsets/invert/rotate, or null if outside the layer.
const color_t *prepared_layer_get_color(const PreparedLayer &layer, int x, int y);

// Input: one mutable target surface plus one prepared layer in destination coordinates.
// Output: no return value; the layer is composited into the target using legacy alpha/mask semantics.
void compose_prepared_layer(RasterSurface &target, const PreparedLayer &layer, int has_alpha_mask);

// Input: one raw layer definition.
// Output: true when the layer is a plain group-image reference with no crop/offset/transform overrides.
int is_bare_group_reference(const RawLayerDef &layer);

// Input: one composed raster surface.
// Output: true when the surface contains a non-empty top section above the footprint rows.
int has_top_part(const RasterSurface &surface);

// Input: one combined isometric raster and the computed top height.
// Output: no return value; split_surface receives legacy-style top rows followed by footprint rows.
void split_top_and_footprint(const RasterSurface &source, int top_height, RasterSurface &split_surface);

// Input: one split raster plus top-height/isometric metadata and a payload base key.
// Output: renderer-facing managed payload keys for the footprint and optional top images.
int upload_split_surface(
    const std::string &base_key,
    const RasterSurface &split_surface,
    int top_height,
    int is_isometric,
    ResolvedTextureSlice &out_footprint,
    ResolvedTextureSlice &out_top);

// Input: no arguments.
// Output: no return value; all cached docs, resolved entries, payloads, and failure sets are cleared.
void clear_all_cached_payload_state(void);

} // namespace image_group_payload_internal
