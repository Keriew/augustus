#ifndef BUILDING_BUILDING_RUNTIME_INTERNAL_H
#define BUILDING_BUILDING_RUNTIME_INTERNAL_H

#include "building/building_runtime.h"

#include <memory>

namespace building_runtime_impl {

extern std::vector<std::unique_ptr<building_runtime>> g_runtime_instances;

building_runtime *get_city_building(unsigned int id);
building_runtime *get_city_building(::building *building_data);
building_runtime *get_or_create_instance(::building *building_data);

}

#endif // BUILDING_BUILDING_RUNTIME_INTERNAL_H
