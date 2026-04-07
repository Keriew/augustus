#ifndef BUILDING_BUILDING_RUNTIME_H
#define BUILDING_BUILDING_RUNTIME_H

#include "assets/image_group_entry.h"
#include "building/building_type.h"

extern "C" {
#include "figure/figure.h"
}

#include <cstddef>
#include <cstdint>
#include <vector>

class ImageGroupPayload;

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
    const RuntimeDrawSlice *graphic_footprint();
    const RuntimeDrawSlice *graphic_top();
    const RuntimeDrawSlice *graphic_animation();
    int owns_graphics();
    int owns_graphic_animation();

    const ::building *building() const
    {
        return building_;
    }

    const building_type_registry_impl::BuildingType *definition() const
    {
        return definition_;
    }

private:
    // Per-building-instance cached native graphics bindings.
    // Input: one live building instance plus its shared BuildingType definition.
    // Output: stable references to the selected base and animation image-group entries for that instance.
    struct CachedGraphicsBindings {
        const building_type_registry_impl::GraphicsTarget *selected_target = nullptr;
        const ImageGroupPayload *base_payload = nullptr;
        const ImageGroupEntry *base_entry = nullptr;
        const ImageGroupPayload *animation_payload = nullptr;
        const ImageGroupEntry *animation_entry = nullptr;
        RuntimeDrawSlice animation_slice;
        int owns_graphics = 0;
        int owns_graphic_animation = 0;
        int dirty = 1;
        int resolved = 0;
        std::uint64_t signature = 0;
    };

    void refresh_runtime_state();
    void clear_cached_graphics_bindings();
    void invalidate_graphics_cache();
    void rebuild_cached_graphics_bindings();
    void ensure_cached_graphics_bindings();
    void rebuild_cached_animation_slice();
    std::uint64_t graphics_state_signature() const;
    int mirror_animation_offset(const RuntimeAnimationTrack &track) const;
    int uses_new_graphics() const;
    int building_state_supports_native_graphics() const;
    const building_type_registry_impl::GraphicsTarget *resolve_graphic_target() const;
    int resolve_graphic_binding(
        const building_type_registry_impl::GraphicsTarget *target,
        const ImageGroupPayload *&payload,
        const ImageGroupEntry *&entry) const;
    const RuntimeAnimationTrack *cached_animation_track() const;
    int worker_percentage() const;
    int default_spawn_delay() const;
    void check_labor_problem();
    void generate_labor_seeker(int x, int y);
    void spawn_labor_seeker(int x, int y, int min_houses);
    void run_labor_phase(const building_type_registry_impl::LaborDefinition &labor, const map_point &road);
    int has_figure_of_type(figure_type type);
    int has_figure_of_any(const std::vector<figure_type> &types);
    unsigned int *figure_slot_storage(building_type_registry_impl::FigureSlot slot);
    int slot_has_live_figure(
        building_type_registry_impl::FigureSlot slot,
        figure_type primary_type,
        figure_type secondary_type = FIGURE_NONE);
    void send_supplier_to_destination(figure *supplier, int destination_building_id);
    int spawn_caravanserai_supplier(const map_point &road);
    int spawn_lighthouse_supplier(const map_point &road);
    void spawn_architect_guild();
    void spawn_caravanserai();
    void spawn_lighthouse();
    void spawn_watchtower();
    void spawn_armoury();
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
    CachedGraphicsBindings graphics_cache_;
};

#endif // BUILDING_BUILDING_RUNTIME_H
