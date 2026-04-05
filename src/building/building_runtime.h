#ifndef BUILDING_BUILDING_RUNTIME_H
#define BUILDING_BUILDING_RUNTIME_H

#include "building/building_type.h"

#include <cstddef>
#include <vector>

extern "C" {
#include "core/image.h"
}

class building_runtime {
public:
    building_runtime(::building *building, const building_type_registry_impl::BuildingType *definition)
        : data(*building)
        , building_(building)
        , definition_(definition)
    {
    }

    // Transitional public access to the legacy saved struct while behavior is still migrating into methods.
    ::building &data;

    void set_building_graphic();
    void spawn_figure();
    const image *resolve_graphic_image();
    const image *resolve_graphic_animation_frame();

    const ::building *building() const
    {
        return building_;
    }

    const building_type_registry_impl::BuildingType *definition() const
    {
        return definition_;
    }

private:
    void refresh_graphic_state();
    int uses_new_graphics() const;
    const building_type_registry_impl::GraphicsTarget *resolve_graphic_target() const;
    const char *resolve_named_group_image_id(const char *group_key, const char *image_id) const;
    const char *resolve_candidate_graphic_image_id(const char *group_key, const char *const *image_ids, size_t image_id_count) const;
    const char *resolve_default_group_image_id(const char *group_key) const;
    const char *resolve_type_specific_graphic_image_id(const char *group_key) const;
    const char *resolve_graphic_image_id(const building_type_registry_impl::GraphicsTarget *target) const;
    const image *resolve_named_group_image(const char *group_key, const char *image_id) const;
    const image *resolve_candidate_graphic_image(const char *group_key, const char *const *image_ids, size_t image_id_count) const;
    const image *resolve_default_group_image(const char *group_key) const;
    const image *resolve_type_specific_graphic_image(const char *group_key) const;
    int worker_percentage() const;
    void check_labor_problem();
    void generate_labor_seeker(int x, int y);
    void spawn_labor_seeker(int x, int y, int min_houses);
    void run_labor_phase(const building_type_registry_impl::LaborPolicy &labor_policy, const map_point &road);
    int has_figure_of_type(figure_type type);
    int has_figure_of_any(const std::vector<figure_type> &types);
    int resolve_road_access(building_type_registry_impl::RoadAccessMode mode, map_point *road) const;
    int evaluate_delay(const std::vector<building_type_registry_impl::DelayBand> &delay_bands) const;
    int evaluate_condition(building_type_registry_impl::SpawnCondition condition) const;
    int should_apply_graphic_for_timing(
        const building_type_registry_impl::SpawnDelayGroup &group,
        building_type_registry_impl::GraphicTiming timing) const;
    unsigned char get_spawn_delay_counter(size_t policy_index) const;
    void set_spawn_delay_counter(size_t policy_index, unsigned char value);
    void assign_figure_slot(building_type_registry_impl::FigureSlot slot, unsigned int figure_id);
    int create_spawned_figure(const building_type_registry_impl::SpawnPolicy &policy, const map_point &road);
    int try_spawn_policy(const building_type_registry_impl::SpawnPolicy &policy, const map_point &road);
    void spawn_service_roamer_group(const building_type_registry_impl::SpawnDelayGroup &group, size_t group_index);

    ::building *building_;
    const building_type_registry_impl::BuildingType *definition_;
    std::vector<unsigned char> spawn_delay_counters_;
};

#endif // BUILDING_BUILDING_RUNTIME_H
