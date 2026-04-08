#include "building/storage_type.h"

#include <utility>

namespace building_type_registry_impl {

StorageType::StorageType(std::string path)
    : path_(std::move(path))
{
}

const char *StorageType::path() const
{
    return path_.c_str();
}

void StorageType::add_resource(resource_type resource)
{
    resources_.push_back(resource);
}

const std::vector<resource_type> &StorageType::resources() const
{
    return resources_;
}

int StorageType::handles_resource(resource_type resource) const
{
    for (resource_type current : resources_) {
        if (current == resource) {
            return 1;
        }
    }
    return 0;
}

void StorageType::set_capacity(int capacity)
{
    capacity_ = capacity;
}

int StorageType::capacity() const
{
    return capacity_;
}

} // namespace building_type_registry_impl
