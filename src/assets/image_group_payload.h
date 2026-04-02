#pragma once

#include "assets/image_group_entry.h"
#include "assets/xml.h"
#include "core/image.h"

#ifdef __cplusplus
#include <string>
#include <unordered_map>
#include <vector>

class ImageGroupPayload {
public:
    ImageGroupPayload(std::string key, std::string xml_path, xml_asset_source source);
    ~ImageGroupPayload();

    const std::string &key() const;
    const std::string &xml_path() const;
    xml_asset_source source() const;

    void set_image(const std::string &image_id, const std::string &image_key, const std::string &top_image_key = std::string());
    void set_image_animation(const std::string &image_id, const image_animation &animation, std::vector<std::string> frame_image_keys);
    const char *image_key_for(const char *image_id) const;
    const char *top_image_key_for(const char *image_id) const;
    const char *image_id_at_index(int index) const;
    const image *legacy_image_for(const char *image_id) const;
    const image *legacy_image_at_index(int index) const;
    const image *animation_frame_for(const char *image_id, int animation_offset) const;
    const image *default_legacy_image() const;
    const char *default_image_id() const;

private:
    std::string key_;
    std::string xml_path_;
    xml_asset_source source_ = XML_ASSET_SOURCE_AUTO;
    std::unordered_map<std::string, ImageGroupEntry> images_;
    std::vector<std::string> image_order_;
    std::string default_image_id_;
};

const ImageGroupPayload *image_group_payload_get(const char *path_key);
#endif

#ifdef __cplusplus
extern "C" {
#endif

int image_group_payload_load(const char *path_key);
const image *image_group_payload_get_image(const char *path_key, const char *image_id);
const image *image_group_payload_get_image_at_index(const char *path_key, int index);
const image *image_group_payload_get_animation_frame(const char *path_key, const char *image_id, int animation_offset);
const image *image_group_payload_get_default_image(const char *path_key);
const char *image_group_payload_get_default_image_id(const char *path_key);
const char *image_group_payload_get_image_id_at_index(const char *path_key, int index);
const char *image_group_payload_get_image_key(const char *path_key, const char *image_id);
void image_group_payload_clear_all(void);

#ifdef __cplusplus
}
#endif
