#pragma once

#include "assets/image_group_entry.h"
#include "assets/xml.h"

#include <string>
#include <unordered_map>
#include <vector>

class ImageGroupPayload {
public:
    ImageGroupPayload(std::string key, std::string xml_path, xml_asset_source source);

    const std::string &key() const;
    const std::string &xml_path() const;
    xml_asset_source source() const;

    void add_entry(std::string internal_key, std::string image_id, ImageGroupEntry entry);
    void set_default_entry(const std::string &internal_key);
    const ImageGroupEntry *entry_for(const char *image_id) const;
    const ImageGroupEntry *entry_at_index(int index) const;
    const ImageGroupEntry *default_entry() const;
    const char *default_image_id() const;
    int entry_count() const;

private:
    std::string key_;
    std::string xml_path_;
    xml_asset_source source_ = XML_ASSET_SOURCE_AUTO;
    std::unordered_map<std::string, ImageGroupEntry> entries_;
    std::unordered_map<std::string, std::string> named_entry_keys_;
    std::vector<std::string> ordered_entry_keys_;
    std::string default_image_id_;
    std::string default_entry_key_;
};

const ImageGroupPayload *image_group_payload_get(const char *path_key);
int image_group_payload_load(const char *path_key);

#ifdef __cplusplus
extern "C" {
#endif

void image_group_payload_clear_all(void);

#ifdef __cplusplus
}
#endif
