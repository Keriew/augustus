#include "assets/augustus_asset_extractor.h"

#include "core/legacy_image_extractor.h"

extern "C" {
#include "assets/assets.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"
#include "core/png_read.h"
#include "core/xml_parser.h"
#include "game/mod_manager.h"
#include "platform/file_manager.h"
}

#include "spng/spng.h"

#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr char kStampPrefix[] = "augustus_extract_v1:";

struct ExtractionStats {
    int source_files = 0;
    int groups_exported = 0;
    int images_exported = 0;
    int pngs_written = 0;
};

struct AtlasReference {
    std::string src;
    std::string group;
    std::string image;
    std::string part;
    std::string mask;
    std::string invert;
    std::string rotate;
    int src_x = 0;
    int src_y = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool has_src_x = false;
    bool has_src_y = false;
    bool has_x = false;
    bool has_y = false;
    bool has_width = false;
    bool has_height = false;
    bool is_direct_crop = false;
};

struct AtlasAnimationFrame {
    AtlasReference reference;
};

struct AtlasAnimation {
    bool present = false;
    bool has_frames = false;
    int frames = 0;
    bool has_speed = false;
    int speed = 0;
    bool reversible = false;
    bool has_x = false;
    int x = 0;
    bool has_y = false;
    int y = 0;
    std::vector<AtlasAnimationFrame> frames_data;
};

struct AtlasImage {
    std::string id;
    bool synthetic_id = false;
    bool is_isometric = false;
    bool has_width = false;
    bool has_height = false;
    int width = 0;
    int height = 0;
    std::vector<AtlasReference> layers;
    AtlasAnimation animation;
    size_t source_index = 0;
};

struct AtlasDocument {
    std::string assetlist_name;
    std::string family_name;
    std::string xml_path;
    std::string png_path;
    std::vector<AtlasImage> images;
};

struct ParseState {
    AtlasDocument document;
    AtlasImage *current_image = nullptr;
    AtlasAnimation *current_animation = nullptr;
    int error = 0;
};

struct OutputGroup {
    std::string family_name;
    std::string group_name;
    std::string group_key;
    std::string directory_path;
    std::string xml_path;
    std::vector<int> image_indices;
};

ParseState g_parse_state;

static std::string sanitize_component(const char *text)
{
    if (!text || !*text) {
        return "unnamed";
    }

    std::string sanitized;
    sanitized.reserve(strlen(text));
    for (const char *cursor = text; *cursor; ++cursor) {
        const char value = *cursor;
        const bool is_alpha = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
        const bool is_digit = value >= '0' && value <= '9';
        if (is_alpha || is_digit || value == '_' || value == '-') {
            sanitized.push_back(value);
        } else {
            sanitized.push_back('_');
        }
    }

    while (!sanitized.empty() && sanitized.back() == '_') {
        sanitized.pop_back();
    }
    return sanitized.empty() ? std::string("unnamed") : sanitized;
}

static std::string normalize_key(const char *text)
{
    std::string normalized = text ? text : "";
    for (char &value : normalized) {
        if (value == '/') {
            value = '\\';
        }
    }
    return normalized;
}

static int append_path_component(char *buffer, size_t buffer_size, const char *base_path, const char *component)
{
    if (!buffer || buffer_size == 0 || !base_path || !*base_path || !component || !*component) {
        return 0;
    }

    const size_t base_length = strlen(base_path);
    const int has_separator = base_length > 0 &&
        (base_path[base_length - 1] == '/' || base_path[base_length - 1] == '\\');
    return snprintf(buffer, buffer_size, has_separator ? "%s%s" : "%s/%s", base_path, component) <
        static_cast<int>(buffer_size);
}

static std::string append_path_component(const std::string &base_path, const std::string &component)
{
    char buffer[FILE_NAME_MAX];
    if (!append_path_component(buffer, sizeof(buffer), base_path.c_str(), component.c_str())) {
        return {};
    }
    return buffer;
}

static bool read_text_file(const std::string &path, std::string &contents)
{
    FILE *file = file_open(path.c_str(), "rb");
    if (!file) {
        contents.clear();
        return false;
    }

    char buffer[256];
    const size_t bytes_read = fread(buffer, 1, sizeof(buffer) - 1, file);
    buffer[bytes_read] = '\0';
    contents.assign(buffer);
    file_close(file);
    return true;
}

static bool write_text_file(const std::string &path, const std::string &contents)
{
    FILE *file = file_open(path.c_str(), "wb");
    if (!file) {
        return false;
    }

    const size_t bytes_written = fwrite(contents.data(), 1, contents.size(), file);
    file_close(file);
    return bytes_written == contents.size();
}

static void ensure_directory(const std::string &path)
{
    if (!path.empty()) {
        platform_file_manager_create_directory(path.c_str(), 0, 1);
    }
}

static void hash_bytes(uint64_t &hash, const void *data_ptr, size_t size)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data_ptr);
    const uint64_t fnv_prime = 1099511628211ull;
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= fnv_prime;
    }
}

static void hash_string(uint64_t &hash, const char *text)
{
    if (!text) {
        return;
    }
    hash_bytes(hash, text, strlen(text));
}

static void hash_int64(uint64_t &hash, uint64_t value)
{
    hash_bytes(hash, &value, sizeof(value));
}

static std::string make_augustus_graphics_root(void)
{
    return mod_manager_get_augustus_graphics_path();
}

static std::string make_stamp_path(void)
{
    return make_augustus_graphics_root() + ".graphics_extract.stamp";
}

static std::string make_source_graphics_path(void)
{
    char path[FILE_NAME_MAX];
    const char *asset_root = platform_file_manager_get_directory_for_location(PATH_LOCATION_ASSET, 0);
    if (!append_path_component(path, sizeof(path), asset_root, ASSETS_IMAGE_PATH)) {
        return {};
    }
    return path;
}

static int stop_on_first_entry(const char *name, long modified_time)
{
    (void) name;
    (void) modified_time;
    return LIST_MATCH;
}

static bool build_expected_stamp(std::string &stamp)
{
    const std::string source_graphics_path = make_source_graphics_path();
    if (source_graphics_path.empty()) {
        return false;
    }

    uint64_t hash = 1469598103934665603ull;
    hash_string(hash, source_graphics_path.c_str());

    const dir_listing *xml_files = dir_find_files_with_extension(source_graphics_path.c_str(), "xml");
    for (int i = 0; i < xml_files->num_files; ++i) {
        hash_string(hash, xml_files->files[i].name);
        hash_int64(hash, xml_files->files[i].modified_time);
    }

    const dir_listing *png_files = dir_find_files_with_extension(source_graphics_path.c_str(), "png");
    for (int i = 0; i < png_files->num_files; ++i) {
        hash_string(hash, png_files->files[i].name);
        hash_int64(hash, png_files->files[i].modified_time);
    }

    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%s%016llx", kStampPrefix, static_cast<unsigned long long>(hash));
    stamp = buffer;
    return true;
}

static bool write_png(const std::string &path, const color_t *pixels, int width, int height)
{
    if (!pixels || width <= 0 || height <= 0) {
        return false;
    }

    FILE *file = file_open(path.c_str(), "wb");
    if (!file) {
        return false;
    }

    spng_ctx *ctx = spng_ctx_new(SPNG_CTX_ENCODER);
    if (!ctx) {
        file_close(file);
        return false;
    }

    int result = spng_set_option(ctx, SPNG_IMG_COMPRESSION_LEVEL, 1);
    if (result == 0) {
        result = spng_set_png_file(ctx, file);
    }

    spng_ihdr ihdr = {};
    ihdr.width = static_cast<uint32_t>(width);
    ihdr.height = static_cast<uint32_t>(height);
    ihdr.bit_depth = 8;
    ihdr.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;

    if (result == 0) {
        result = spng_set_ihdr(ctx, &ihdr);
    }
    if (result == 0) {
        result = spng_encode_image(ctx, 0, 0, SPNG_FMT_PNG, SPNG_ENCODE_PROGRESSIVE | SPNG_ENCODE_FINALIZE);
    }

    std::vector<uint8_t> scanline(static_cast<size_t>(width) * 4);
    for (int y = 0; result == 0 && y < height; ++y) {
        uint8_t *destination = scanline.data();
        const color_t *source_row = &pixels[static_cast<size_t>(y) * width];
        for (int x = 0; x < width; ++x) {
            const color_t color = source_row[x];
            destination[0] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_RED));
            destination[1] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_GREEN));
            destination[2] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_BLUE));
            destination[3] = static_cast<uint8_t>(COLOR_COMPONENT(color, COLOR_BITSHIFT_ALPHA));
            destination += 4;
        }
        const int scanline_result = spng_encode_scanline(ctx, scanline.data(), scanline.size());
        if (scanline_result != SPNG_OK && scanline_result != SPNG_EOI) {
            result = scanline_result;
        }
    }

    spng_ctx_free(ctx);
    file_close(file);
    return result == 0 || result == SPNG_EOI;
}

static void append_indent(std::string &xml, int depth)
{
    xml.append(static_cast<size_t>(depth) * 4, ' ');
}

static void append_attribute(std::string &xml, const char *name, const std::string &value)
{
    xml += " ";
    xml += name;
    xml += "=\"";
    xml += value;
    xml += "\"";
}

static void append_attribute(std::string &xml, const char *name, int value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    append_attribute(xml, name, std::string(buffer));
}

static int xml_start_assetlist(void)
{
    const char *name = xml_parser_get_attribute_string("name");
    if (!name || !*name) {
        log_error("Augustus atlas assetlist is missing a name", 0, 0);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.document.assetlist_name = name;
    g_parse_state.document.family_name = sanitize_component(name);
    return 1;
}

static void push_reference(
    std::vector<AtlasReference> &destination,
    const char *path,
    const char *group,
    const char *image_id)
{
    AtlasReference reference;
    if (path && *path) {
        reference.src = path;
    }
    if (group && *group) {
        reference.group = group;
    }
    if (image_id && *image_id) {
        reference.image = image_id;
    }

    reference.has_src_x = xml_parser_has_attribute("src_x") != 0;
    reference.has_src_y = xml_parser_has_attribute("src_y") != 0;
    reference.has_x = xml_parser_has_attribute("x") != 0;
    reference.has_y = xml_parser_has_attribute("y") != 0;
    reference.has_width = xml_parser_has_attribute("width") != 0;
    reference.has_height = xml_parser_has_attribute("height") != 0;

    reference.src_x = xml_parser_get_attribute_int("src_x");
    reference.src_y = xml_parser_get_attribute_int("src_y");
    reference.x = xml_parser_get_attribute_int("x");
    reference.y = xml_parser_get_attribute_int("y");
    reference.width = xml_parser_get_attribute_int("width");
    reference.height = xml_parser_get_attribute_int("height");

    const char *part = xml_parser_get_attribute_string("part");
    if (part && *part) {
        reference.part = part;
    }
    const char *mask = xml_parser_get_attribute_string("mask");
    if (mask && *mask) {
        reference.mask = mask;
    }
    const char *invert = xml_parser_get_attribute_string("invert");
    if (invert && *invert) {
        reference.invert = invert;
    }
    const char *rotate = xml_parser_get_attribute_string("rotate");
    if (rotate && *rotate) {
        reference.rotate = rotate;
    }

    reference.is_direct_crop = reference.src.empty() && reference.group.empty();
    destination.push_back(std::move(reference));
}

static bool is_local_reference(const AtlasReference &reference);

static int translate_reference_group_key(const AtlasReference &source_reference, std::string &translated_group_key)
{
    translated_group_key.clear();
    if (source_reference.group.empty() || is_local_reference(source_reference)) {
        return 0;
    }

    char *end = nullptr;
    const long numeric_group = strtol(source_reference.group.c_str(), &end, 10);
    if (end && *end == '\0') {
        char group_key[FILE_NAME_MAX];
        if (!legacy_image_extractor_get_group_key(static_cast<int>(numeric_group), group_key, sizeof(group_key))) {
            return -1;
        }
        translated_group_key = group_key;
        return translated_group_key.empty() ? -1 : 1;
    }

    translated_group_key = normalize_key(source_reference.group.c_str());
    return translated_group_key.empty() ? -1 : 1;
}

static void set_output_group_key(OutputGroup &output_group, const std::string &group_key)
{
    output_group.group_key = group_key;

    const size_t separator = group_key.rfind('\\');
    if (separator == std::string::npos) {
        output_group.family_name.clear();
        output_group.group_name = group_key;
    } else {
        output_group.family_name = group_key.substr(0, separator);
        output_group.group_name = group_key.substr(separator + 1);
    }

    output_group.directory_path = append_path_component(make_augustus_graphics_root(), output_group.group_key);
    output_group.xml_path = append_path_component(make_augustus_graphics_root(), output_group.group_key + ".xml");
}

static std::string choose_canonical_group_key(const AtlasDocument &document, const OutputGroup &output_group)
{
    std::string canonical_group_key;
    int invalid_reference = 0;

    for (int image_index : output_group.image_indices) {
        const AtlasImage &image_data = document.images[image_index];

        auto visit_reference = [&](const AtlasReference &reference) {
            std::string translated_group_key;
            const int translated = translate_reference_group_key(reference, translated_group_key);
            if (translated < 0) {
                invalid_reference = 1;
                return;
            }
            if (translated == 0) {
                return;
            }
            if (canonical_group_key.empty()) {
                canonical_group_key = std::move(translated_group_key);
                return;
            }
            if (canonical_group_key != translated_group_key) {
                invalid_reference = 2;
                canonical_group_key.clear();
            }
        };

        for (const AtlasReference &reference : image_data.layers) {
            visit_reference(reference);
            if (invalid_reference == 2) {
                break;
            }
        }
        if (invalid_reference == 2) {
            break;
        }

        for (const AtlasAnimationFrame &frame : image_data.animation.frames_data) {
            visit_reference(frame.reference);
            if (invalid_reference == 2) {
                break;
            }
        }
        if (invalid_reference == 2) {
            break;
        }
    }

    if (invalid_reference == 1) {
        log_info(
            "Falling back to assetlist-derived Augustus output path because a referenced group key was invalid",
            output_group.group_key.c_str(),
            0);
        return {};
    }
    if (invalid_reference == 2) {
        log_info(
            "Falling back to assetlist-derived Augustus output path because wrapper references were ambiguous",
            output_group.group_key.c_str(),
            0);
        return {};
    }
    return canonical_group_key;
}

static int xml_start_image(void)
{
    AtlasImage image_data;
    const char *id = xml_parser_get_attribute_string("id");
    if (id && *id) {
        image_data.id = id;
    }
    image_data.synthetic_id = image_data.id.empty();
    image_data.is_isometric = xml_parser_get_attribute_bool("isometric") != 0;
    image_data.has_width = xml_parser_has_attribute("width") != 0;
    image_data.has_height = xml_parser_has_attribute("height") != 0;
    image_data.width = xml_parser_get_attribute_int("width");
    image_data.height = xml_parser_get_attribute_int("height");
    image_data.source_index = g_parse_state.document.images.size();

    g_parse_state.document.images.push_back(std::move(image_data));
    g_parse_state.current_image = &g_parse_state.document.images.back();
    g_parse_state.current_animation = nullptr;

    const char *path = xml_parser_get_attribute_string("src");
    const char *group = xml_parser_get_attribute_string("group");
    const char *image_id = xml_parser_get_attribute_string("image");
    if ((path && *path) || (group && *group)) {
        push_reference(g_parse_state.current_image->layers, path, group, image_id);
    }
    return 1;
}

static int xml_start_layer(void)
{
    if (!g_parse_state.current_image) {
        g_parse_state.error = 1;
        return 0;
    }

    push_reference(
        g_parse_state.current_image->layers,
        xml_parser_get_attribute_string("src"),
        xml_parser_get_attribute_string("group"),
        xml_parser_get_attribute_string("image"));
    return 1;
}

static int xml_start_animation(void)
{
    if (!g_parse_state.current_image) {
        g_parse_state.error = 1;
        return 0;
    }

    AtlasAnimation &animation = g_parse_state.current_image->animation;
    animation.present = true;
    animation.has_frames = xml_parser_has_attribute("frames") != 0;
    animation.frames = xml_parser_get_attribute_int("frames");
    animation.has_speed = xml_parser_has_attribute("speed") != 0;
    animation.speed = xml_parser_get_attribute_int("speed");
    animation.reversible = xml_parser_get_attribute_bool("reversible") != 0;
    animation.has_x = xml_parser_has_attribute("x") != 0;
    animation.x = xml_parser_get_attribute_int("x");
    animation.has_y = xml_parser_has_attribute("y") != 0;
    animation.y = xml_parser_get_attribute_int("y");
    g_parse_state.current_animation = &animation;
    return 1;
}

static int xml_start_frame(void)
{
    if (!g_parse_state.current_animation) {
        g_parse_state.error = 1;
        return 0;
    }

    AtlasAnimationFrame frame;
    std::vector<AtlasReference> frame_reference;
    push_reference(
        frame_reference,
        xml_parser_get_attribute_string("src"),
        xml_parser_get_attribute_string("group"),
        xml_parser_get_attribute_string("image"));
    if (frame_reference.empty()) {
        g_parse_state.error = 1;
        return 0;
    }
    frame.reference = std::move(frame_reference.front());
    g_parse_state.current_animation->frames_data.push_back(std::move(frame));
    return 1;
}

static void xml_end_image(void)
{
    g_parse_state.current_image = nullptr;
    g_parse_state.current_animation = nullptr;
}

static void xml_end_animation(void)
{
    g_parse_state.current_animation = nullptr;
}

static const xml_parser_element kXmlElements[] = {
    { "assetlist", xml_start_assetlist, 0 },
    { "image", xml_start_image, xml_end_image, "assetlist" },
    { "layer", xml_start_layer, 0, "image" },
    { "animation", xml_start_animation, xml_end_animation, "image" },
    { "frame", xml_start_frame, 0, "animation" }
};

static bool load_file_to_buffer(const std::string &path, std::vector<char> &buffer)
{
    FILE *file = file_open(path.c_str(), "rb");
    if (!file) {
        log_error("Unable to open Augustus source xml", path.c_str(), 0);
        return false;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        file_close(file);
        return false;
    }

    long file_size = ftell(file);
    if (file_size < 0) {
        file_close(file);
        return false;
    }
    rewind(file);

    buffer.resize(static_cast<size_t>(file_size));
    if (!buffer.empty()) {
        const size_t bytes_read = fread(buffer.data(), 1, buffer.size(), file);
        if (bytes_read != buffer.size()) {
            file_close(file);
            return false;
        }
    }

    file_close(file);
    return true;
}

static bool parse_document(const std::string &xml_path, AtlasDocument &document)
{
    std::vector<char> buffer;
    if (!load_file_to_buffer(xml_path, buffer)) {
        return false;
    }

    g_parse_state = {};
    g_parse_state.document.xml_path = xml_path;

    if (!xml_parser_init(kXmlElements, static_cast<int>(sizeof(kXmlElements) / sizeof(kXmlElements[0])), 1)) {
        log_error("Unable to initialize Augustus extractor xml parser", xml_path.c_str(), 0);
        return false;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();
    if (!parsed || g_parse_state.error || g_parse_state.document.assetlist_name.empty()) {
        log_error("Failed to parse Augustus source xml", xml_path.c_str(), 0);
        return false;
    }

    int synthetic_counter = 0;
    for (AtlasImage &image_data : g_parse_state.document.images) {
        if (!image_data.id.empty()) {
            continue;
        }
        char synthetic_name[32];
        snprintf(synthetic_name, sizeof(synthetic_name), "Image_%04d", synthetic_counter++);
        image_data.id = synthetic_name;
        image_data.synthetic_id = true;
    }

    document = std::move(g_parse_state.document);
    return true;
}

struct DisjointSet {
    std::vector<int> parent;
    std::vector<int> rank;

    explicit DisjointSet(size_t size)
        : parent(size)
        , rank(size, 0)
    {
        for (size_t i = 0; i < size; ++i) {
            parent[i] = static_cast<int>(i);
        }
    }

    int find(int value)
    {
        if (parent[value] != value) {
            parent[value] = find(parent[value]);
        }
        return parent[value];
    }

    void unite(int a, int b)
    {
        a = find(a);
        b = find(b);
        if (a == b) {
            return;
        }
        if (rank[a] < rank[b]) {
            std::swap(a, b);
        }
        parent[b] = a;
        if (rank[a] == rank[b]) {
            rank[a]++;
        }
    }
};

static bool is_local_reference(const AtlasReference &reference)
{
    return reference.group == "this" || reference.group == "THIS";
}

static void connect_local_reference(
    const AtlasReference &reference,
    int image_index,
    const std::unordered_map<std::string, int> &id_lookup,
    DisjointSet &disjoint_set)
{
    if (!is_local_reference(reference) || reference.image.empty()) {
        return;
    }
    auto it = id_lookup.find(reference.image);
    if (it == id_lookup.end()) {
        return;
    }
    disjoint_set.unite(image_index, it->second);
}

static std::vector<OutputGroup> build_output_groups(const AtlasDocument &document)
{
    if (document.images.empty()) {
        return {};
    }

    std::unordered_map<std::string, int> id_lookup;
    for (size_t index = 0; index < document.images.size(); ++index) {
        id_lookup.emplace(document.images[index].id, static_cast<int>(index));
    }

    DisjointSet disjoint_set(document.images.size());
    for (size_t index = 0; index < document.images.size(); ++index) {
        const AtlasImage &image_data = document.images[index];
        for (const AtlasReference &reference : image_data.layers) {
            connect_local_reference(reference, static_cast<int>(index), id_lookup, disjoint_set);
        }
        for (const AtlasAnimationFrame &frame : image_data.animation.frames_data) {
            connect_local_reference(frame.reference, static_cast<int>(index), id_lookup, disjoint_set);
        }
    }

    int last_named_index = -1;
    for (size_t index = 0; index < document.images.size(); ++index) {
        const AtlasImage &image_data = document.images[index];
        if (!image_data.synthetic_id) {
            last_named_index = static_cast<int>(index);
            continue;
        }
        if (last_named_index >= 0) {
            disjoint_set.unite(static_cast<int>(index), last_named_index);
        }
    }

    std::vector<OutputGroup> grouped_output;
    std::unordered_map<int, size_t> root_to_group_index;
    for (size_t index = 0; index < document.images.size(); ++index) {
        const int root = disjoint_set.find(static_cast<int>(index));
        auto it = root_to_group_index.find(root);
        if (it == root_to_group_index.end()) {
            OutputGroup output_group;
            output_group.family_name = document.family_name;
            output_group.image_indices.push_back(static_cast<int>(index));
            root_to_group_index.emplace(root, grouped_output.size());
            grouped_output.push_back(std::move(output_group));
        } else {
            grouped_output[it->second].image_indices.push_back(static_cast<int>(index));
        }
    }

    std::unordered_map<std::string, int> family_name_counts;
    for (size_t group_index = 0; group_index < grouped_output.size(); ++group_index) {
        OutputGroup &output_group = grouped_output[group_index];

        std::string group_name;
        for (int image_index : output_group.image_indices) {
            const AtlasImage &image_data = document.images[image_index];
            if (image_data.synthetic_id) {
                continue;
            }
            group_name = sanitize_component(image_data.id.c_str());
            break;
        }
        if (group_name.empty()) {
            char unnamed_group_name[32];
            snprintf(unnamed_group_name, sizeof(unnamed_group_name), "Group_%04d", static_cast<int>(group_index));
            group_name = unnamed_group_name;
        }

        int &name_count = family_name_counts[group_name];
        if (name_count > 0) {
            char unique_name[FILE_NAME_MAX];
            snprintf(unique_name, sizeof(unique_name), "%s_%02d", group_name.c_str(), name_count + 1);
            output_group.group_name = unique_name;
        } else {
            output_group.group_name = group_name;
        }
        name_count++;

        set_output_group_key(output_group, output_group.family_name + "\\" + output_group.group_name);

        const std::string canonical_group_key = choose_canonical_group_key(document, output_group);
        if (!canonical_group_key.empty()) {
            set_output_group_key(output_group, canonical_group_key);
        }
    }

    return grouped_output;
}

static int resolve_crop_width(const AtlasReference &reference, const AtlasImage &image_data)
{
    if (reference.has_width && reference.width > 0) {
        return reference.width;
    }
    if (image_data.has_width && image_data.width > 0) {
        return image_data.width;
    }
    return 0;
}

static int resolve_crop_height(const AtlasReference &reference, const AtlasImage &image_data)
{
    if (reference.has_height && reference.height > 0) {
        return reference.height;
    }
    if (image_data.has_height && image_data.height > 0) {
        return image_data.height;
    }
    return 0;
}

static bool write_direct_crop(
    const AtlasImage &image_data,
    const AtlasReference &reference,
    const std::string &output_path,
    ExtractionStats &stats)
{
    const int width = resolve_crop_width(reference, image_data);
    const int height = resolve_crop_height(reference, image_data);
    if (width <= 0 || height <= 0) {
        log_error("Augustus extracted crop has invalid size", image_data.id.c_str(), 0);
        return false;
    }

    std::vector<color_t> pixels(static_cast<size_t>(width) * height, ALPHA_TRANSPARENT);
    const int src_x = reference.has_src_x ? reference.src_x : 0;
    const int src_y = reference.has_src_y ? reference.src_y : 0;
    if (!png_read(pixels.data(), src_x, src_y, width, height, 0, 0, width, 0)) {
        log_error("Failed to read Augustus atlas region", image_data.id.c_str(), 0);
        return false;
    }

    if (!write_png(output_path, pixels.data(), width, height)) {
        log_error("Failed to write Augustus extracted png", output_path.c_str(), 0);
        return false;
    }
    stats.pngs_written++;
    return true;
}

static std::string make_numeric_image_id(const char *image_name)
{
    if (!image_name || !*image_name) {
        return "Image_0000";
    }
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Image_%04d", atoi(image_name));
    return buffer;
}

static bool translate_reference(
    const AtlasReference &source_reference,
    const OutputGroup &output_group,
    AtlasReference &translated_reference)
{
    translated_reference = source_reference;
    translated_reference.src.clear();
    translated_reference.is_direct_crop = false;

    if (source_reference.group.empty()) {
        return true;
    }

    if (is_local_reference(source_reference)) {
        translated_reference.group = output_group.group_key;
        if (translated_reference.image.empty()) {
            translated_reference.image = "Image_0000";
        }
        return true;
    }

    char *end = nullptr;
    const long numeric_group = strtol(source_reference.group.c_str(), &end, 10);
    if (end && *end == '\0') {
        char group_key[FILE_NAME_MAX];
        if (!legacy_image_extractor_get_group_key(static_cast<int>(numeric_group), group_key, sizeof(group_key))) {
            log_error("Unable to translate Augustus legacy group reference", source_reference.group.c_str(), 0);
            return false;
        }
        translated_reference.group = group_key;
        translated_reference.image = make_numeric_image_id(source_reference.image.c_str());
        return true;
    }

    translated_reference.group = normalize_key(source_reference.group.c_str());
    return true;
}

static void append_reference_attributes(std::string &xml, const AtlasReference &reference)
{
    if (!reference.src.empty()) {
        append_attribute(xml, "src", reference.src);
    }
    if (!reference.group.empty()) {
        append_attribute(xml, "group", reference.group);
    }
    if (!reference.image.empty()) {
        append_attribute(xml, "image", reference.image);
    }
    if (reference.has_x && reference.x != 0) {
        append_attribute(xml, "x", reference.x);
    }
    if (reference.has_y && reference.y != 0) {
        append_attribute(xml, "y", reference.y);
    }
    if (!reference.part.empty()) {
        append_attribute(xml, "part", reference.part);
    }
    if (!reference.mask.empty()) {
        append_attribute(xml, "mask", reference.mask);
    }
    if (!reference.invert.empty()) {
        append_attribute(xml, "invert", reference.invert);
    }
    if (!reference.rotate.empty()) {
        append_attribute(xml, "rotate", reference.rotate);
    }
}

static bool append_reference_xml(
    std::string &xml,
    const AtlasImage &image_data,
    const AtlasReference &reference,
    const OutputGroup &output_group,
    const char *tag_name,
    const char *file_stem,
    ExtractionStats &stats)
{
    AtlasReference translated_reference;
    if (reference.is_direct_crop) {
        const std::string output_path = append_path_component(output_group.directory_path, std::string(file_stem) + ".png");
        if (!write_direct_crop(image_data, reference, output_path, stats)) {
            return false;
        }
        translated_reference = reference;
        translated_reference.src = file_stem;
        translated_reference.group.clear();
        translated_reference.image.clear();
        translated_reference.is_direct_crop = false;
    } else if (!translate_reference(reference, output_group, translated_reference)) {
        return false;
    }

    append_indent(xml, strcmp(tag_name, "frame") == 0 ? 3 : 2);
    xml += "<";
    xml += tag_name;
    append_reference_attributes(xml, translated_reference);
    xml += "/>\n";
    return true;
}

static bool append_image_xml(
    std::string &xml,
    const AtlasImage &image_data,
    const OutputGroup &output_group,
    ExtractionStats &stats)
{
    append_indent(xml, 1);
    xml += "<image";
    append_attribute(xml, "id", image_data.id);
    if (image_data.has_width && image_data.width > 0) {
        append_attribute(xml, "width", image_data.width);
    }
    if (image_data.has_height && image_data.height > 0) {
        append_attribute(xml, "height", image_data.height);
    }
    if (image_data.is_isometric) {
        append_attribute(xml, "isometric", "true");
    }
    xml += ">\n";

    const std::string image_stem = sanitize_component(image_data.id.c_str());
    for (size_t layer_index = 0; layer_index < image_data.layers.size(); ++layer_index) {
        char stem[FILE_NAME_MAX];
        if (image_data.layers.size() == 1 && !image_data.animation.present && image_stem != "unnamed") {
            snprintf(stem, sizeof(stem), "%s", image_stem.c_str());
        } else {
            snprintf(stem, sizeof(stem), "%s_Layer_%02d", image_stem.c_str(), static_cast<int>(layer_index + 1));
        }
        if (!append_reference_xml(xml, image_data, image_data.layers[layer_index], output_group, "layer", stem, stats)) {
            return false;
        }
    }

    if (image_data.animation.present) {
        append_indent(xml, 2);
        xml += "<animation";
        if (image_data.animation.has_frames) {
            append_attribute(xml, "frames", image_data.animation.frames);
        }
        if (image_data.animation.has_speed) {
            append_attribute(xml, "speed", image_data.animation.speed);
        }
        if (image_data.animation.reversible) {
            append_attribute(xml, "reversible", "true");
        }
        if (image_data.animation.has_x && image_data.animation.x != 0) {
            append_attribute(xml, "x", image_data.animation.x);
        }
        if (image_data.animation.has_y && image_data.animation.y != 0) {
            append_attribute(xml, "y", image_data.animation.y);
        }
        if (image_data.animation.frames_data.empty()) {
            xml += "/>\n";
        } else {
            xml += ">\n";
            for (size_t frame_index = 0; frame_index < image_data.animation.frames_data.size(); ++frame_index) {
                char frame_stem[FILE_NAME_MAX];
                snprintf(frame_stem, sizeof(frame_stem), "%s_Frame_%02d", image_stem.c_str(), static_cast<int>(frame_index + 1));
                if (!append_reference_xml(
                        xml,
                        image_data,
                        image_data.animation.frames_data[frame_index].reference,
                        output_group,
                        "frame",
                        frame_stem,
                        stats)) {
                    return false;
                }
            }
            append_indent(xml, 2);
            xml += "</animation>\n";
        }
    }

    append_indent(xml, 1);
    xml += "</image>\n";
    stats.images_exported++;
    return true;
}

static bool export_group(const AtlasDocument &document, const OutputGroup &output_group, ExtractionStats &stats)
{
    ensure_directory(append_path_component(make_augustus_graphics_root(), output_group.family_name));
    ensure_directory(output_group.directory_path);

    std::string xml = "<?xml version=\"1.0\"?>\n<!DOCTYPE assetlist>\n<assetlist";
    append_attribute(xml, "name", output_group.group_key);
    xml += ">\n";

    for (int image_index : output_group.image_indices) {
        if (!append_image_xml(xml, document.images[image_index], output_group, stats)) {
            return false;
        }
    }

    xml += "</assetlist>\n";
    if (!write_text_file(output_group.xml_path, xml)) {
        log_error("Failed to write Augustus extracted xml", output_group.xml_path.c_str(), 0);
        return false;
    }

    stats.groups_exported++;
    return true;
}

static bool export_document(const AtlasDocument &document, ExtractionStats &stats)
{
    if (!png_load_from_file(document.png_path.c_str(), 0)) {
        log_error("Unable to load Augustus source atlas png", document.png_path.c_str(), 0);
        return false;
    }

    const std::vector<OutputGroup> groups = build_output_groups(document);
    for (const OutputGroup &output_group : groups) {
        if (!export_group(document, output_group, stats)) {
            png_unload();
            return false;
        }
    }

    png_unload();
    return true;
}

static bool extract_all_documents(ExtractionStats &stats)
{
    const std::string source_graphics_path = make_source_graphics_path();
    if (source_graphics_path.empty()) {
        log_error("Unable to resolve Augustus source graphics directory", 0, 0);
        return false;
    }

    const dir_listing *xml_files = dir_find_files_with_extension(source_graphics_path.c_str(), "xml");
    for (int i = 0; i < xml_files->num_files; ++i) {
        const std::string xml_name = xml_files->files[i].name;
        const std::string xml_path = append_path_component(source_graphics_path, xml_name);

        AtlasDocument document;
        if (!parse_document(xml_path, document)) {
            return false;
        }

        document.png_path = append_path_component(source_graphics_path, document.assetlist_name + ".png");
        stats.source_files++;
        if (!export_document(document, stats)) {
            return false;
        }
    }

    return true;
}

} // namespace

extern "C" int augustus_asset_extractor_bootstrap(void)
{
    const std::string graphics_root = make_augustus_graphics_root();
    const std::string stamp_path = make_stamp_path();

    std::string expected_stamp;
    if (!build_expected_stamp(expected_stamp)) {
        log_error("Failed to fingerprint Augustus source graphics", 0, 0);
        return 0;
    }

    std::string existing_stamp;
    const int has_existing_stamp = read_text_file(stamp_path, existing_stamp);
    const int has_current_stamp = has_existing_stamp && existing_stamp == expected_stamp;
    if (has_current_stamp &&
        platform_file_manager_list_directory_contents(
            graphics_root.c_str(), TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) == LIST_MATCH) {
        return 1;
    }

    if (!has_existing_stamp) {
        log_info("Bootstrapping Augustus graphics because no extraction stamp was found", 0, 0);
    } else if (!has_current_stamp) {
        log_info("Bootstrapping Augustus graphics because the source fingerprint or XML metadata version changed", 0, 0);
    } else {
        log_info("Bootstrapping Augustus graphics because the fallback directory is missing or empty", 0, 0);
    }

    platform_file_manager_remove_directory(graphics_root.c_str());
    ensure_directory("Mods");
    ensure_directory("Mods/Augustus");
    ensure_directory(graphics_root);

    ExtractionStats stats;
    log_info("Extracting canonical Augustus graphics from packed source atlases", 0, 0);
    if (!extract_all_documents(stats)) {
        log_error("Augustus graphics extraction failed", 0, 0);
        return 0;
    }
    if (!write_text_file(stamp_path, expected_stamp)) {
        log_error("Failed to write Augustus extraction stamp", stamp_path.c_str(), 0);
        return 0;
    }

    char summary[256];
    snprintf(
        summary,
        sizeof(summary),
        "%d source atlases, %d groups, %d images, %d pngs",
        stats.source_files,
        stats.groups_exported,
        stats.images_exported,
        stats.pngs_written);
    log_info("Augustus graphics extraction completed", summary, 0);
    return 1;
}
