#include "assets/image_group_payload.h"

#include "core/crash_context.h"
#include "core/image_payload.h"

extern "C" {
#include "core/file.h"
#include "core/log.h"
#include "core/xml_parser.h"
}

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

GroupPayloadMap g_group_payloads;
std::unordered_set<std::string> g_failed_group_payloads;
std::unordered_set<std::string> g_loading_group_payloads;

struct PendingImage {
    std::string id;
    std::string image_key;
    std::string top_image_key;
    std::string pending_image_ref_id;
    std::string pending_top_image_ref_id;
    std::string pending_full_image_ref_id;
    image_animation animation = {};
    int has_animation = 0;
    int implicit_animation_frame_count = 0;
    std::vector<std::string> explicit_animation_frame_keys;
};

struct PendingAnimationSpec {
    std::string image_id;
    image_animation animation = {};
    int implicit_frame_count = 0;
};

struct ParseState {
    std::string requested_key;
    std::string xml_path;
    xml_asset_source source = XML_ASSET_SOURCE_AUTO;
    std::unique_ptr<ImageGroupPayload> payload;
    PendingImage current_image;
    std::vector<std::string> image_order;
    std::vector<PendingImage> pending_local_references;
    std::vector<PendingAnimationSpec> pending_animations;
    int error = 0;
};

ParseState g_parse_state;

static void set_crash_scope_stage(const char *stage, const char *detail)
{
    char context[FILE_NAME_MAX + 64];
    snprintf(
        context,
        sizeof(context),
        "group=%s detail=%s",
        g_parse_state.requested_key.c_str(),
        detail ? detail : "");
    crash_context_set_stage(stage, context);
}

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

static std::string normalize_group_reference_key(const char *group_name)
{
    const std::string normalized_group = normalize_path_key(group_name);
    if ((normalized_group == "this" || normalized_group == "THIS") && !g_parse_state.requested_key.empty()) {
        return g_parse_state.requested_key;
    }
    return normalized_group;
}

static const ImageGroupPayload *find_group_payload(const char *path_key)
{
    const std::string normalized_key = normalize_path_key(path_key);
    if (normalized_key.empty()) {
        return nullptr;
    }
    auto it = g_group_payloads.find(normalized_key);
    return it == g_group_payloads.end() ? nullptr : it->second.get();
}

static const char *find_local_image_key_in_parse_state(const char *image_id)
{
    if (!image_id || !*image_id || !g_parse_state.payload) {
        return nullptr;
    }

    if (const ImageGroupEntry *entry = g_parse_state.payload->entry_for(image_id)) {
        return entry->image_key().empty() ? nullptr : entry->image_key().c_str();
    }
    return nullptr;
}

static const ImageGroupEntry *find_local_entry_in_parse_state(const char *image_id)
{
    if (!image_id || !*image_id || !g_parse_state.payload) {
        return nullptr;
    }
    return g_parse_state.payload->entry_for(image_id);
}

static int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
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

static std::string make_image_payload_key(const std::string &group_key, const char *image_name)
{
    return group_key + "\\" + normalize_path_key(image_name);
}

static int load_image_from_path(
    const std::string &group_key,
    const char *image_name,
    xml_asset_source source,
    std::string &out_key)
{
    CrashContextScope crash_scope("image_group.resolve_png_path", image_name);
    char full_path[FILE_NAME_MAX] = { 0 };
    if (!xml_resolve_image_path(full_path, group_key.c_str(), image_name, source)) {
        crash_context_report_error("Unable to resolve image group png", image_name);
        return 0;
    }

    out_key = make_image_payload_key(group_key, image_name);
    if (!image_payload_load_png_payload(out_key.c_str(), full_path)) {
        crash_context_report_error("Unable to load image group png payload", full_path);
        return 0;
    }
    return 1;
}

static int load_image_from_group_reference(
    const char *group_name,
    const char *image_id,
    std::string &out_key)
{
    const std::string normalized_group = normalize_group_reference_key(group_name);
    if (normalized_group.empty() || !image_id || !*image_id) {
        return 0;
    }

    if (g_parse_state.payload && normalized_group == g_parse_state.requested_key) {
        if (const char *referenced_key = find_local_image_key_in_parse_state(image_id)) {
            image_payload_retain_key(referenced_key);
            out_key = referenced_key;
            return 1;
        }
    }

    CrashContextScope crash_scope("image_group.resolve_group_reference", image_id);
    if (!image_group_payload_load(normalized_group.c_str())) {
        crash_context_report_error("Unable to load referenced image group", normalized_group.c_str());
        return 0;
    }
    const char *referenced_key = image_group_payload_get_image_key(normalized_group.c_str(), image_id);
    if (!referenced_key || !*referenced_key) {
        crash_context_report_error("Unable to resolve referenced image", image_id);
        return 0;
    }
    image_payload_retain_key(referenced_key);
    out_key = referenced_key;
    return 1;
}

static void clear_pending_image(void)
{
    g_parse_state.current_image = {};
}

static void release_pending_image_keys(void)
{
    if (!g_parse_state.current_image.image_key.empty()) {
        image_payload_release_key(g_parse_state.current_image.image_key.c_str());
    }
    if (!g_parse_state.current_image.top_image_key.empty()) {
        image_payload_release_key(g_parse_state.current_image.top_image_key.c_str());
    }
    clear_pending_image();
}

static int pending_image_has_local_references(const PendingImage &pending)
{
    return !pending.pending_image_ref_id.empty() ||
        !pending.pending_top_image_ref_id.empty() ||
        !pending.pending_full_image_ref_id.empty();
}

static int assign_full_image_reference(const char *group, const char *image_id)
{
    const std::string normalized_group = normalize_group_reference_key(group);
    if (normalized_group.empty() || !image_id || !*image_id) {
        return 0;
    }

    if (g_parse_state.payload && normalized_group == g_parse_state.requested_key) {
        const ImageGroupEntry *entry = find_local_entry_in_parse_state(image_id);
        if (!entry || entry->image_key().empty()) {
            g_parse_state.current_image.pending_full_image_ref_id = image_id;
            return 1;
        }

        if (!g_parse_state.current_image.image_key.empty()) {
            image_payload_release_key(g_parse_state.current_image.image_key.c_str());
        }
        if (!g_parse_state.current_image.top_image_key.empty()) {
            image_payload_release_key(g_parse_state.current_image.top_image_key.c_str());
        }

        image_payload_retain_key(entry->image_key().c_str());
        g_parse_state.current_image.image_key = entry->image_key();

        if (!entry->top_image_key().empty()) {
            image_payload_retain_key(entry->top_image_key().c_str());
            g_parse_state.current_image.top_image_key = entry->top_image_key();
        } else {
            g_parse_state.current_image.top_image_key.clear();
        }

        g_parse_state.current_image.has_animation = entry->has_animation();
        g_parse_state.current_image.animation = entry->animation();
        g_parse_state.current_image.implicit_animation_frame_count = 0;
        g_parse_state.current_image.explicit_animation_frame_keys = entry->animation_frame_keys();
        return 1;
    }

    CrashContextScope crash_scope("image_group.resolve_full_image_reference", image_id);
    if (!image_group_payload_load(normalized_group.c_str())) {
        crash_context_report_error("Unable to load referenced image group", normalized_group.c_str());
        return 0;
    }

    const ImageGroupPayload *payload = find_group_payload(normalized_group.c_str());
    const ImageGroupEntry *entry = payload ? payload->entry_for(image_id) : nullptr;
    if (!entry || entry->image_key().empty()) {
        crash_context_report_error("Unable to resolve referenced image entry", image_id);
        return 0;
    }

    if (!g_parse_state.current_image.image_key.empty()) {
        image_payload_release_key(g_parse_state.current_image.image_key.c_str());
    }
    if (!g_parse_state.current_image.top_image_key.empty()) {
        image_payload_release_key(g_parse_state.current_image.top_image_key.c_str());
    }

    image_payload_retain_key(entry->image_key().c_str());
    g_parse_state.current_image.image_key = entry->image_key();

    if (!entry->top_image_key().empty()) {
        image_payload_retain_key(entry->top_image_key().c_str());
        g_parse_state.current_image.top_image_key = entry->top_image_key();
    } else {
        g_parse_state.current_image.top_image_key.clear();
    }

    g_parse_state.current_image.has_animation = entry->has_animation();
    g_parse_state.current_image.animation = entry->animation();
    g_parse_state.current_image.implicit_animation_frame_count = 0;
    g_parse_state.current_image.explicit_animation_frame_keys = entry->animation_frame_keys();
    return 1;
}

static int commit_pending_image(void)
{
    if (!g_parse_state.payload) {
        return 0;
    }
    const int has_pending_local_references = pending_image_has_local_references(g_parse_state.current_image);
    if (g_parse_state.current_image.id.empty() ||
        (g_parse_state.current_image.image_key.empty() && !has_pending_local_references)) {
        release_pending_image_keys();
        return 1;
    }

    g_parse_state.payload->set_image(
        g_parse_state.current_image.id,
        g_parse_state.current_image.image_key,
        g_parse_state.current_image.top_image_key);
    if (!g_parse_state.current_image.explicit_animation_frame_keys.empty()) {
        g_parse_state.payload->set_image_animation(
            g_parse_state.current_image.id,
            g_parse_state.current_image.animation,
            std::move(g_parse_state.current_image.explicit_animation_frame_keys));
    } else if (g_parse_state.current_image.has_animation && g_parse_state.current_image.pending_full_image_ref_id.empty()) {
        PendingAnimationSpec animation_spec = {};
        animation_spec.image_id = g_parse_state.current_image.id;
        animation_spec.animation = g_parse_state.current_image.animation;
        animation_spec.implicit_frame_count = g_parse_state.current_image.implicit_animation_frame_count;
        g_parse_state.pending_animations.push_back(std::move(animation_spec));
    }
    if (has_pending_local_references) {
        g_parse_state.pending_local_references.push_back(g_parse_state.current_image);
    }
    clear_pending_image();
    return 1;
}

static int assign_image_reference(const char *path, const char *group, const char *image_id, int is_top)
{
    std::string resolved_key;
    int ok = 0;
    if (path && *path) {
        ok = load_image_from_path(g_parse_state.requested_key, path, g_parse_state.source, resolved_key);
    } else if (group && *group) {
        const std::string normalized_group = normalize_group_reference_key(group);
        if (g_parse_state.payload && normalized_group == g_parse_state.requested_key) {
            if (const char *referenced_key = find_local_image_key_in_parse_state(image_id)) {
                image_payload_retain_key(referenced_key);
                resolved_key = referenced_key;
                ok = 1;
            } else {
                if (is_top) {
                    g_parse_state.current_image.pending_top_image_ref_id = image_id ? image_id : "";
                } else {
                    g_parse_state.current_image.pending_image_ref_id = image_id ? image_id : "";
                }
                return 1;
            }
        } else {
        ok = load_image_from_group_reference(group, image_id, resolved_key);
        }
    }

    if (!ok) {
        g_parse_state.error = 1;
        return 0;
    }

    if (is_top) {
        if (!g_parse_state.current_image.top_image_key.empty()) {
            image_payload_release_key(g_parse_state.current_image.top_image_key.c_str());
        }
        g_parse_state.current_image.top_image_key = std::move(resolved_key);
    } else {
        if (!g_parse_state.current_image.image_key.empty()) {
            image_payload_release_key(g_parse_state.current_image.image_key.c_str());
        }
        g_parse_state.current_image.image_key = std::move(resolved_key);
    }
    return 1;
}

static int xml_start_assetlist(void)
{
    set_crash_scope_stage("image_group.parse_assetlist", g_parse_state.xml_path.c_str());
    const char *name = xml_parser_get_attribute_string("name");
    const std::string normalized_name = normalize_path_key(name);
    if (normalized_name.empty()) {
        crash_context_report_error("ImageGroup assetlist is missing name", g_parse_state.xml_path.c_str());
        g_parse_state.error = 1;
        return 0;
    }
    if (normalized_name != g_parse_state.requested_key) {
        crash_context_report_error("ImageGroup assetlist name does not match requested key", name);
        g_parse_state.error = 1;
        return 0;
    }

    g_parse_state.payload = std::make_unique<ImageGroupPayload>(normalized_name, g_parse_state.xml_path, g_parse_state.source);
    return 1;
}

static int xml_start_image(void)
{
    set_crash_scope_stage("image_group.parse_image", xml_parser_get_attribute_string("id"));
    if (!commit_pending_image()) {
        g_parse_state.error = 1;
        return 0;
    }

    clear_pending_image();
    const char *id = xml_parser_get_attribute_string("id");
    if (!id || !*id) {
        log_error("ImageGroup image is missing id", g_parse_state.requested_key.c_str(), 0);
        g_parse_state.error = 1;
        return 0;
    }
    g_parse_state.current_image.id = id;
    g_parse_state.image_order.push_back(id);

    const char *path = xml_parser_get_attribute_string("src");
    const char *group = xml_parser_get_attribute_string("group");
    const char *image_id = xml_parser_get_attribute_string("image");
    if ((path && *path) || (group && *group)) {
        if (!(path && *path) && group && *group) {
            return assign_full_image_reference(group, image_id);
        }
        return assign_image_reference(path, group, image_id, 0);
    }
    return 1;
}

static int xml_start_layer(void)
{
    static const char *part_values[2] = { "footprint", "top" };

    const char *path = xml_parser_get_attribute_string("src");
    const char *group = xml_parser_get_attribute_string("group");
    const char *image_id = xml_parser_get_attribute_string("image");
    const int has_part = xml_parser_has_attribute("part") != 0;
    const int part = xml_parser_get_attribute_enum("part", part_values, 2, 0);
    const int is_top = part == 1;

    if (!(path && *path) && !(group && *group)) {
        return 1;
    }
    if (!has_part && !(path && *path) && group && *group &&
        g_parse_state.current_image.image_key.empty() &&
        g_parse_state.current_image.top_image_key.empty()) {
        return assign_full_image_reference(group, image_id);
    }
    return assign_image_reference(path, group, image_id, is_top);
}

static int xml_start_animation(void)
{
    g_parse_state.current_image.has_animation = 1;
    g_parse_state.current_image.animation = {};
    g_parse_state.current_image.animation.num_sprites = xml_parser_get_attribute_int("frames");
    g_parse_state.current_image.animation.speed_id = xml_parser_get_attribute_int("speed");
    g_parse_state.current_image.animation.can_reverse = xml_parser_get_attribute_bool("reversible");
    g_parse_state.current_image.animation.sprite_offset_x = xml_parser_get_attribute_int("x");
    g_parse_state.current_image.animation.sprite_offset_y = xml_parser_get_attribute_int("y");
    g_parse_state.current_image.implicit_animation_frame_count = g_parse_state.current_image.animation.num_sprites;
    return 1;
}

static int xml_start_frame(void)
{
    return 1;
}

static void xml_end_assetlist(void)
{
    commit_pending_image();
}

static void xml_end_image(void)
{
    commit_pending_image();
}

static void xml_end_animation(void)
{
}

static void finalize_pending_animations(void)
{
    if (!g_parse_state.payload) {
        return;
    }

    for (const PendingAnimationSpec &animation_spec : g_parse_state.pending_animations) {
        if (animation_spec.implicit_frame_count <= 0) {
            continue;
        }

        std::vector<std::string> frame_image_keys;
        for (size_t i = 0; i < g_parse_state.image_order.size(); i++) {
            if (g_parse_state.image_order[i] != animation_spec.image_id) {
                continue;
            }

            for (int frame_index = 1; frame_index <= animation_spec.implicit_frame_count; frame_index++) {
                const size_t order_index = i + static_cast<size_t>(frame_index);
                if (order_index >= g_parse_state.image_order.size()) {
                    break;
                }

                const char *frame_key = g_parse_state.payload->image_key_for(g_parse_state.image_order[order_index].c_str());
                if (!frame_key || !*frame_key) {
                    break;
                }
                frame_image_keys.emplace_back(frame_key);
            }
            break;
        }

        if (!frame_image_keys.empty()) {
            image_animation animation = animation_spec.animation;
            animation.num_sprites = static_cast<int>(frame_image_keys.size());
            g_parse_state.payload->set_image_animation(animation_spec.image_id, animation, std::move(frame_image_keys));
        }
    }
}

static int finalize_pending_local_references(void)
{
    if (!g_parse_state.payload || g_parse_state.pending_local_references.empty()) {
        return 1;
    }

    std::vector<PendingImage> unresolved = std::move(g_parse_state.pending_local_references);
    for (size_t pass = 0; pass < unresolved.size(); ++pass) {
        int made_progress = 0;
        std::vector<PendingImage> next_unresolved;

        for (PendingImage &pending : unresolved) {
            const ImageGroupEntry *entry = g_parse_state.payload->entry_for(pending.id.c_str());
            std::string image_key = (entry && !entry->image_key().empty()) ? entry->image_key() : std::string();
            std::string top_image_key = (entry && !entry->top_image_key().empty()) ? entry->top_image_key() : std::string();

            if (!pending.pending_full_image_ref_id.empty()) {
                const ImageGroupEntry *target = g_parse_state.payload->entry_for(pending.pending_full_image_ref_id.c_str());
                if (!target || target->image_key().empty()) {
                    next_unresolved.push_back(std::move(pending));
                    continue;
                }

                image_payload_retain_key(target->image_key().c_str());
                image_key = target->image_key();
                if (!target->top_image_key().empty()) {
                    image_payload_retain_key(target->top_image_key().c_str());
                    top_image_key = target->top_image_key();
                } else {
                    top_image_key.clear();
                }

                g_parse_state.payload->set_image(pending.id, image_key, top_image_key);
                if (target->has_animation()) {
                    g_parse_state.payload->set_image_animation(
                        pending.id,
                        target->animation(),
                        target->animation_frame_keys());
                }
                pending.pending_full_image_ref_id.clear();
                made_progress = 1;
            }

            if (!pending.pending_image_ref_id.empty()) {
                const char *resolved_key = g_parse_state.payload->image_key_for(pending.pending_image_ref_id.c_str());
                if (!resolved_key || !*resolved_key) {
                    next_unresolved.push_back(std::move(pending));
                    continue;
                }

                image_payload_retain_key(resolved_key);
                image_key = resolved_key;
                g_parse_state.payload->set_image(pending.id, image_key, top_image_key);
                pending.pending_image_ref_id.clear();
                made_progress = 1;
            }

            if (!pending.pending_top_image_ref_id.empty()) {
                const char *resolved_key = g_parse_state.payload->image_key_for(pending.pending_top_image_ref_id.c_str());
                if (!resolved_key || !*resolved_key) {
                    next_unresolved.push_back(std::move(pending));
                    continue;
                }

                image_payload_retain_key(resolved_key);
                top_image_key = resolved_key;
                g_parse_state.payload->set_image(pending.id, image_key, top_image_key);
                pending.pending_top_image_ref_id.clear();
                made_progress = 1;
            }

            if (pending_image_has_local_references(pending)) {
                next_unresolved.push_back(std::move(pending));
            }
        }

        if (next_unresolved.empty()) {
            return 1;
        }
        if (!made_progress) {
            unresolved = std::move(next_unresolved);
            break;
        }
        unresolved = std::move(next_unresolved);
    }

    for (const PendingImage &pending : unresolved) {
        crash_context_report_error("Unable to resolve same-group image reference", pending.id.c_str());
    }
    return 0;
}

static const xml_parser_element kXmlElements[] = {
    { "assetlist", xml_start_assetlist, xml_end_assetlist },
    { "image", xml_start_image, xml_end_image, "assetlist" },
    { "layer", xml_start_layer, 0, "image" },
    { "animation", xml_start_animation, xml_end_animation, "image" },
    { "frame", xml_start_frame, 0, "animation" }
};

static std::unique_ptr<ImageGroupPayload> parse_group_file(
    const char *xml_path,
    const std::string &path_key,
    xml_asset_source source)
{
    CrashContextScope crash_scope("image_group.parse_group_file", xml_path);
    std::vector<char> buffer;
    if (!load_file_to_buffer(xml_path, buffer)) {
        return nullptr;
    }

    g_parse_state = {};
    g_parse_state.requested_key = path_key;
    g_parse_state.xml_path = xml_path;
    g_parse_state.source = source;
    set_crash_scope_stage("image_group.parse_group_file", xml_path);

    if (!xml_parser_init(kXmlElements, static_cast<int>(sizeof(kXmlElements) / sizeof(kXmlElements[0])), 1)) {
        crash_context_report_error("Unable to initialize image group parser", xml_path);
        return nullptr;
    }

    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();

    if (!parsed || g_parse_state.error || !g_parse_state.payload) {
        release_pending_image_keys();
        g_parse_state.payload.reset();
        return nullptr;
    }
    if (!finalize_pending_local_references()) {
        g_parse_state.payload.reset();
        return nullptr;
    }
    finalize_pending_animations();
    return std::move(g_parse_state.payload);
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

    auto it = images_.find(image_id);
    if (it != images_.end()) {
        if (!it->second.image_key().empty()) {
            image_payload_release_key(it->second.image_key().c_str());
        }
        if (!it->second.top_image_key().empty()) {
            image_payload_release_key(it->second.top_image_key().c_str());
        }
        it->second.set_keys(image_key, top_image_key);
        return;
    }

    image_order_.push_back(image_id);
    images_.emplace(image_id, ImageGroupEntry(image_id, image_key, top_image_key));
}

void ImageGroupPayload::set_image_animation(const std::string &image_id, const image_animation &animation, std::vector<std::string> frame_image_keys)
{
    auto it = images_.find(image_id);
    if (it == images_.end()) {
        return;
    }
    it->second.set_animation(animation, std::move(frame_image_keys));
}

const char *ImageGroupPayload::image_key_for(const char *image_id) const
{
    if (!image_id || !*image_id) {
        return nullptr;
    }
    auto it = images_.find(image_id);
    return it == images_.end() ? nullptr : it->second.image_key().c_str();
}

const char *ImageGroupPayload::top_image_key_for(const char *image_id) const
{
    if (!image_id || !*image_id) {
        return nullptr;
    }
    auto it = images_.find(image_id);
    return (it == images_.end() || it->second.top_image_key().empty()) ? nullptr : it->second.top_image_key().c_str();
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
    if (!image_id || !*image_id) {
        return nullptr;
    }
    auto it = images_.find(image_id);
    return it == images_.end() ? nullptr : it->second.legacy_image();
}

const image *ImageGroupPayload::legacy_image_at_index(int index) const
{
    const char *image_id = image_id_at_index(index);
    return image_id ? legacy_image_for(image_id) : nullptr;
}

const image *ImageGroupPayload::animation_frame_for(const char *image_id, int animation_offset) const
{
    if (!image_id || !*image_id || animation_offset <= 0) {
        return nullptr;
    }
    auto it = images_.find(image_id);
    return it == images_.end() ? nullptr : it->second.animation_frame(animation_offset);
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
    return find_group_payload(path_key);
}

extern "C" int image_group_payload_load(const char *path_key)
{
    const std::string normalized_key = normalize_path_key(path_key);
    if (normalized_key.empty()) {
        return 0;
    }
    CrashContextScope crash_scope("image_group.load", normalized_key.c_str());
    if (find_group_payload(normalized_key.c_str())) {
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

    char xml_path[FILE_NAME_MAX] = { 0 };
    xml_asset_source resolved_source = XML_ASSET_SOURCE_AUTO;
    if (!xml_resolve_assetlist_path(xml_path, normalized_key.c_str(), XML_ASSET_SOURCE_AUTO, &resolved_source)) {
        crash_context_report_error("Unable to resolve image group xml", normalized_key.c_str());
        g_failed_group_payloads.insert(normalized_key);
        return 0;
    }

    g_loading_group_payloads.insert(normalized_key);
    std::unique_ptr<ImageGroupPayload> payload = parse_group_file(xml_path, normalized_key, resolved_source);
    g_loading_group_payloads.erase(normalized_key);
    if (!payload) {
        g_failed_group_payloads.insert(normalized_key);
        return 0;
    }

    g_failed_group_payloads.erase(normalized_key);
    g_group_payloads.emplace(normalized_key, std::move(payload));
    return 1;
}

extern "C" const image *image_group_payload_get_image(const char *path_key, const char *image_id)
{
    const ImageGroupPayload *payload = find_group_payload(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = find_group_payload(path_key);
    return payload ? payload->legacy_image_for(image_id) : nullptr;
}

extern "C" const image *image_group_payload_get_image_at_index(const char *path_key, int index)
{
    const ImageGroupPayload *payload = find_group_payload(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = find_group_payload(path_key);
    return payload ? payload->legacy_image_at_index(index) : nullptr;
}

extern "C" const image *image_group_payload_get_default_image(const char *path_key)
{
    const ImageGroupPayload *payload = find_group_payload(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = find_group_payload(path_key);
    return payload ? payload->default_legacy_image() : nullptr;
}

extern "C" const image *image_group_payload_get_animation_frame(const char *path_key, const char *image_id, int animation_offset)
{
    const ImageGroupPayload *payload = find_group_payload(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = find_group_payload(path_key);
    return payload ? payload->animation_frame_for(image_id, animation_offset) : nullptr;
}

extern "C" const char *image_group_payload_get_default_image_id(const char *path_key)
{
    const ImageGroupPayload *payload = find_group_payload(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = find_group_payload(path_key);
    return payload ? payload->default_image_id() : nullptr;
}

extern "C" const char *image_group_payload_get_image_id_at_index(const char *path_key, int index)
{
    const ImageGroupPayload *payload = find_group_payload(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = find_group_payload(path_key);
    return payload ? payload->image_id_at_index(index) : nullptr;
}

extern "C" const char *image_group_payload_get_image_key(const char *path_key, const char *image_id)
{
    const ImageGroupPayload *payload = find_group_payload(path_key);
    if (!payload && !image_group_payload_load(path_key)) {
        return nullptr;
    }
    payload = find_group_payload(path_key);
    return payload ? payload->image_key_for(image_id) : nullptr;
}

extern "C" void image_group_payload_clear_all(void)
{
    g_group_payloads.clear();
    g_failed_group_payloads.clear();
    g_loading_group_payloads.clear();
}
