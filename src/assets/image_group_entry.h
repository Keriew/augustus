#pragma once

#include "core/image.h"

#ifdef __cplusplus
#include <string>

class ImageGroupEntry {
public:
    ImageGroupEntry() = default;
    ImageGroupEntry(std::string id, std::string image_key, std::string top_image_key = std::string());

    const std::string &id() const;
    const std::string &image_key() const;
    const std::string &top_image_key() const;
    const image *legacy_image() const;

    void set_keys(std::string image_key, std::string top_image_key = std::string());

private:
    std::string id_;
    std::string image_key_;
    std::string top_image_key_;
};
#endif
