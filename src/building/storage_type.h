#ifndef BUILDING_STORAGE_TYPE_H
#define BUILDING_STORAGE_TYPE_H

extern "C" {
#include "game/resource.h"
}

#include <string>
#include <vector>

namespace building_type_registry_impl {

class StorageType {
public:
    explicit StorageType(std::string path);

    const char *path() const;

    void add_resource(resource_type resource);
    const std::vector<resource_type> &resources() const;
    int handles_resource(resource_type resource) const;

    void set_capacity(int capacity);
    int capacity() const;

private:
    std::string path_;
    std::vector<resource_type> resources_;
    int capacity_ = 0;
};

} // namespace building_type_registry_impl

#endif // BUILDING_STORAGE_TYPE_H
