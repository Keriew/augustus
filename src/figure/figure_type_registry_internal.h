#ifndef FIGURE_TYPE_REGISTRY_INTERNAL_H
#define FIGURE_TYPE_REGISTRY_INTERNAL_H

extern "C" {
#include "building/type.h"
#include "figure/type.h"
}

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace figure_type_registry_impl {

enum class NativeClassId {
    None,
    RoamingService
};

enum class FigureSlot {
    None,
    Primary,
    Secondary,
    Quaternary
};

enum class OwnerStateRequirement {
    Any,
    InUse,
    InUseOrMothballed
};

enum class ReturnMode {
    ReturnToOwnerRoad,
    DieAtLimit
};

struct OwnerBinding {
    FigureSlot slot = FigureSlot::None;
    building_type required_building_type = BUILDING_ANY;
    OwnerStateRequirement required_owner_state = OwnerStateRequirement::InUse;
};

struct MovementProfile {
    int terrain_usage = TERRAIN_USAGE_ANY;
    int roam_ticks = 1;
    int max_roam_length = 0;
    ReturnMode return_mode = ReturnMode::ReturnToOwnerRoad;
};

struct GraphicsPolicy {
    int image_group = 0;
    int max_image_offset = 12;
};

class FigureTypeDefinition {
public:
    FigureTypeDefinition(figure_type type, std::string attr);

    figure_type type() const;
    const char *attr() const;

    void set_native_class(NativeClassId native_class_id);
    NativeClassId native_class() const;

    void set_owner_binding(const OwnerBinding &owner_binding);
    const OwnerBinding &owner_binding() const;

    void set_movement_profile(const MovementProfile &movement_profile);
    const MovementProfile &movement_profile() const;

    void set_graphics_policy(const GraphicsPolicy &graphics_policy);
    const GraphicsPolicy &graphics_policy() const;

private:
    figure_type type_ = FIGURE_NONE;
    std::string attr_;
    NativeClassId native_class_id_ = NativeClassId::None;
    OwnerBinding owner_binding_;
    MovementProfile movement_profile_;
    GraphicsPolicy graphics_policy_;
};

extern std::array<std::unique_ptr<FigureTypeDefinition>, FIGURE_TYPE_MAX> g_figure_types;
extern std::string g_failure_reason;

int directory_exists(const char *path);
void set_failure_reason(const char *message, const char *detail = nullptr);
std::vector<std::string> build_candidate_definition_paths();
const FigureTypeDefinition *definition_for(figure_type type);

} // namespace figure_type_registry_impl

#endif // FIGURE_TYPE_REGISTRY_INTERNAL_H
