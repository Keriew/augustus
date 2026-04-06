#include "assets/image_group_payload_internal.h"

#include "core/crash_context.h"

#include <unordered_set>

namespace image_group_payload_internal {

// Input: a group key.
// Output: the merged entry-id namespace across the source chain for that group, or null on failure.
const MergedImageGroup *load_merged_group(const std::string &group_key)
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

    for (int i = 0; i < source_count; i++) {
        const ImageGroupDoc *doc = load_group_doc(group_key, sources[i]);
        if (!doc) {
            g_failed_merged_groups.insert(group_key);
            return nullptr;
        }
        if (merged->xml_path.empty()) {
            merged->xml_path = doc->xml_path;
        }
        merged->source_chain.push_back(doc->source);

        for (const std::string &image_id : doc->ordered_ids) {
            merged->ordered_entries.emplace_back(doc->source, image_id);
        }
    }

    const MergedImageGroup *merged_ptr = merged.get();
    g_merged_groups.emplace(group_key, std::move(merged));
    return merged_ptr;
}

static const ImageGroupDoc *find_group_doc_for_preferred_source(
    const MergedImageGroup &merged,
    xml_asset_source preferred_source,
    const std::string &image_id)
{
    size_t start_index = 0;
    for (size_t i = 0; i < merged.source_chain.size(); i++) {
        if (merged.source_chain[i] == preferred_source) {
            start_index = i;
            break;
        }
    }

    for (size_t i = start_index; i < merged.source_chain.size(); i++) {
        const ImageGroupDoc *doc = load_group_doc(merged.key, merged.source_chain[i]);
        if (doc && doc->entries.find(image_id) != doc->entries.end()) {
            return doc;
        }
    }
    return nullptr;
}

// Input: a merged group key, the requesting source context, and one local XML label.
// Output: the resolved entry for the first matching source at or below the requesting source.
const ResolvedImageEntry *materialize_merged_entry(const std::string &group_key, xml_asset_source preferred_source, const std::string &image_id)
{
    const MergedImageGroup *merged = load_merged_group(group_key);
    if (!merged) {
        return nullptr;
    }

    const ImageGroupDoc *doc = find_group_doc_for_preferred_source(*merged, preferred_source, image_id);
    if (!doc) {
        crash_context_report_error("Unable to resolve merged image id", image_id.c_str());
        return nullptr;
    }
    return materialize_source_entry(group_key, doc->source, image_id);
}

} // namespace image_group_payload_internal
