#include "assets/image_group_payload_internal.h"

#include "core/image_payload.h"

namespace image_group_payload_internal {

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

// Input: raw path text from XML or public API that may be null, spaced, or use '/' separators.
// Output: canonical cache key text using '\' separators and no duplicate/trailing whitespace noise.
std::string normalize_path_key(const char *text)
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

// Input: an XML group reference and the current group key used for "this" references.
// Output: the normalized target group key that later stages should resolve against.
std::string normalize_group_reference_key(const char *group_name, const std::string &current_group)
{
    const std::string normalized_group = normalize_path_key(group_name);
    if (normalized_group == "this" || normalized_group == "THIS") {
        return current_group;
    }
    return normalized_group;
}

// Input: one group key and one source enum.
// Output: the stable cache key for a source-local parsed document.
std::string make_source_doc_cache_key(const std::string &group_key, xml_asset_source source)
{
    return group_key + "|" + std::to_string(static_cast<int>(source));
}

// Input: one group key, one winning source, and one image id.
// Output: the stable cache key for a resolved entry selector.
std::string make_selector_cache_key(const std::string &group_key, xml_asset_source source, const std::string &image_id)
{
    return group_key + "|" + std::to_string(static_cast<int>(source)) + "|" + image_id;
}

// Input: one group key, one source, and one image id.
// Output: the unique managed-texture payload key used for uploaded runtime textures.
std::string make_entry_payload_key(const std::string &group_key, xml_asset_source source, const std::string &image_id)
{
    return "MergedGraphics\\" + group_key + "\\" + std::to_string(static_cast<int>(source)) + "\\" + image_id;
}

// Input: a group key and source.
// Output: the cached source-local document if it exists, otherwise null.
const ImageGroupDoc *find_group_doc(const std::string &group_key, xml_asset_source source)
{
    auto it = g_group_docs.find(make_source_doc_cache_key(group_key, source));
    return it == g_group_docs.end() ? nullptr : it->second.get();
}

// Input: a merged group key.
// Output: the cached merged group if it exists, otherwise null.
const MergedImageGroup *find_merged_group(const std::string &group_key)
{
    auto it = g_merged_groups.find(group_key);
    return it == g_merged_groups.end() ? nullptr : it->second.get();
}

// Input: a group key, source, and image id selector.
// Output: the cached resolved entry if it exists, otherwise null.
const ResolvedImageEntry *find_resolved_entry(const std::string &group_key, xml_asset_source source, const std::string &image_id)
{
    auto it = g_resolved_entries.find(make_selector_cache_key(group_key, source, image_id));
    return it == g_resolved_entries.end() ? nullptr : it->second.get();
}

// Input: one resolved entry whose managed texture keys were retained during materialization.
// Output: no return value; all retained payload keys owned by that resolved entry are released.
void release_resolved_entry_resources(const ResolvedImageEntry &entry)
{
    if (!entry.footprint.texture_key.empty()) {
        image_payload_release_key(entry.footprint.texture_key.c_str());
    }
    if (!entry.top.texture_key.empty()) {
        image_payload_release_key(entry.top.texture_key.c_str());
    }
    for (const std::string &frame_key : entry.animation_frame_keys) {
        if (!frame_key.empty()) {
            image_payload_release_key(frame_key.c_str());
        }
    }
}

// Input: no arguments.
// Output: no return value; all cached docs, resolved entries, payloads, and failure sets are cleared.
void clear_all_cached_payload_state(void)
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

} // namespace image_group_payload_internal
