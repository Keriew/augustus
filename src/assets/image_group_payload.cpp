#include "assets/image_group_payload.h"
#include "assets/image_group_payload_internal.h"

#include "core/crash_context.h"

#include <memory>
#include <utility>

ImageGroupPayload::ImageGroupPayload(std::string key, std::string xml_path, xml_asset_source source)
    : key_(std::move(key))
    , xml_path_(std::move(xml_path))
    , source_(source)
{
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

// Input: one internal selector key, the XML-visible id, and a fully materialized entry.
// Output: no return value; enumeration keeps every internal entry while named lookup keeps the first label match.
void ImageGroupPayload::add_entry(std::string internal_key, std::string image_id, ImageGroupEntry entry)
{
    if (default_image_id_.empty()) {
        default_image_id_ = image_id;
    }
    if (named_entry_keys_.find(image_id) == named_entry_keys_.end()) {
        named_entry_keys_.emplace(image_id, internal_key);
    }
    ordered_entry_keys_.push_back(internal_key);
    entries_.emplace(std::move(internal_key), std::move(entry));
}

void ImageGroupPayload::set_default_entry(const std::string &internal_key)
{
    default_entry_key_ = internal_key;
}

const ImageGroupEntry *ImageGroupPayload::entry_for(const char *image_id) const
{
    if (!image_id || !*image_id) {
        return nullptr;
    }
    auto named = named_entry_keys_.find(image_id);
    if (named == named_entry_keys_.end()) {
        return nullptr;
    }
    auto it = entries_.find(named->second);
    return it == entries_.end() ? nullptr : &it->second;
}

const ImageGroupEntry *ImageGroupPayload::entry_at_index(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= ordered_entry_keys_.size()) {
        return nullptr;
    }
    auto it = entries_.find(ordered_entry_keys_[static_cast<size_t>(index)]);
    return it == entries_.end() ? nullptr : &it->second;
}

const ImageGroupEntry *ImageGroupPayload::default_entry() const
{
    if (default_entry_key_.empty()) {
        return nullptr;
    }
    auto it = entries_.find(default_entry_key_);
    return it == entries_.end() ? nullptr : &it->second;
}

const char *ImageGroupPayload::default_image_id() const
{
    return default_image_id_.empty() ? nullptr : default_image_id_.c_str();
}

int ImageGroupPayload::entry_count() const
{
    return static_cast<int>(ordered_entry_keys_.size());
}

const ImageGroupPayload *image_group_payload_get(const char *path_key)
{
    const std::string normalized_key = image_group_payload_internal::normalize_path_key(path_key);
    auto it = image_group_payload_internal::g_group_payloads.find(normalized_key);
    return it == image_group_payload_internal::g_group_payloads.end() ? nullptr : it->second.get();
}

static ImageGroupEntry make_public_entry(
    const std::string &image_id,
    const image_group_payload_internal::ResolvedImageEntry &resolved)
{
    ImageGroupEntry entry(image_id);
    entry.set_base_slice(resolved.footprint.slice, resolved.is_isometric, resolved.tile_span);
    if (resolved.has_top) {
        entry.set_top_slice(resolved.top.slice);
    }
    entry.set_animation(resolved.animation);
    entry.set_split_pixels(resolved.split_pixels, resolved.split_width, resolved.split_height, resolved.top_height);
    return entry;
}

int image_group_payload_load(const char *path_key)
{
    const std::string normalized_key = image_group_payload_internal::normalize_path_key(path_key);
    if (normalized_key.empty()) {
        return 0;
    }

    CrashContextScope crash_scope("image_group.load", normalized_key.c_str());
    if (image_group_payload_get(normalized_key.c_str())) {
        return 1;
    }
    if (image_group_payload_internal::g_failed_group_payloads.find(normalized_key) !=
        image_group_payload_internal::g_failed_group_payloads.end()) {
        return 0;
    }
    if (image_group_payload_internal::g_loading_group_payloads.find(normalized_key) !=
        image_group_payload_internal::g_loading_group_payloads.end()) {
        crash_context_report_error("Detected recursive image group load", normalized_key.c_str());
        image_group_payload_internal::g_failed_group_payloads.insert(normalized_key);
        return 0;
    }

    image_group_payload_internal::g_loading_group_payloads.insert(normalized_key);

    const image_group_payload_internal::MergedImageGroup *merged =
        image_group_payload_internal::load_merged_group(normalized_key);
    if (!merged || merged->ordered_entries.empty()) {
        image_group_payload_internal::g_loading_group_payloads.erase(normalized_key);
        image_group_payload_internal::g_failed_group_payloads.insert(normalized_key);
        return 0;
    }

    std::unique_ptr<ImageGroupPayload> payload = std::make_unique<ImageGroupPayload>(
        merged->key,
        merged->xml_path,
        merged->source_chain.empty() ? XML_ASSET_SOURCE_AUTO : merged->source_chain.front());

    for (size_t i = 0; i < merged->ordered_entries.size(); i++) {
        const xml_asset_source source = merged->ordered_entries[i].first;
        const std::string &image_id = merged->ordered_entries[i].second;
        const image_group_payload_internal::ResolvedImageEntry *resolved =
            image_group_payload_internal::materialize_source_entry(normalized_key, source, image_id);
        if (!resolved || resolved->footprint.texture_key.empty()) {
            crash_context_report_error("Building graphics image id could not be resolved", image_id.c_str());
            image_group_payload_internal::g_loading_group_payloads.erase(normalized_key);
            image_group_payload_internal::g_failed_group_payloads.insert(normalized_key);
            return 0;
        }

        const std::string internal_key =
            image_group_payload_internal::make_selector_cache_key(normalized_key, source, image_id);
        payload->add_entry(internal_key, image_id, make_public_entry(image_id, *resolved));
        if (i == 0) {
            payload->set_default_entry(internal_key);
        }
    }

    image_group_payload_internal::g_loading_group_payloads.erase(normalized_key);
    image_group_payload_internal::g_failed_group_payloads.erase(normalized_key);
    image_group_payload_internal::g_group_payloads.emplace(normalized_key, std::move(payload));
    return 1;
}

extern "C" void image_group_payload_clear_all(void)
{
    image_group_payload_internal::clear_all_cached_payload_state();
}
