#ifndef BUILDING_STORAGE_TYPE_REGISTRY_H
#define BUILDING_STORAGE_TYPE_REGISTRY_H

#include "building/storage_type.h"

namespace building_type_registry_impl {

const StorageType *find_storage_type_definition(const char *path);

}

#ifdef __cplusplus
extern "C" {
#endif

const char *storage_type_registry_get_storage_type_path(void);
int storage_type_registry_load(void);

#ifdef __cplusplus
}
#endif

#endif // BUILDING_STORAGE_TYPE_REGISTRY_H
