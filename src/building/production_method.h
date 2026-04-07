#ifndef BUILDING_PRODUCTION_METHOD_H
#define BUILDING_PRODUCTION_METHOD_H

extern "C" {
#include "game/resource.h"
}

#include <string>
#include <vector>

namespace building_type_registry_impl {

enum class ProductionMethodKind {
    None,
    Farm,
    Workshop
};

struct ProductionResourceAmount {
    resource_type resource = RESOURCE_NONE;
    int amount = 0;
};

class ProductionMethod {
public:
    explicit ProductionMethod(std::string path);

    const char *path() const;

    void set_kind(ProductionMethodKind kind);
    ProductionMethodKind kind() const;

    void set_output_resource(resource_type resource);
    resource_type output_resource() const;

    void add_input(ProductionResourceAmount input);
    const std::vector<ProductionResourceAmount> &inputs() const;

    int is_farm() const;
    int is_workshop() const;
    int refreshes_farm_image() const;
    int uses_blessing_multiplier() const;

private:
    std::string path_;
    ProductionMethodKind kind_ = ProductionMethodKind::None;
    resource_type output_resource_ = RESOURCE_NONE;
    std::vector<ProductionResourceAmount> inputs_;
};

} // namespace building_type_registry_impl

#endif // BUILDING_PRODUCTION_METHOD_H
