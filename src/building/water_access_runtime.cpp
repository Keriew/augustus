#include "building/water_access_runtime.h"

#include "assets/assets.h"
#include "building/building_type_registry_internal.h"

extern "C" {
#include "building/building.h"
#include "building/image.h"
#include "building/list.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/view.h"
#include "map/aqueduct.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/terrain.h"
#include "map/tiles.h"
#include "scenario/property.h"
}

#include <array>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr int kNoWaterImageOffset = 15;

uint8_t access_bit(building_type_registry_impl::WaterAccessType type)
{
    using namespace building_type_registry_impl;
    switch (type) {
        case WaterAccessType::Well:
            return 0x1;
        case WaterAccessType::Fountain:
            return 0x2;
        case WaterAccessType::Reservoir:
            return 0x4;
        case WaterAccessType::None:
        default:
            return 0;
    }
}

building_type normalize_provider_building_type(building_type type)
{
    return type == BUILDING_DRAGGABLE_RESERVOIR ? BUILDING_RESERVOIR : type;
}

const building_type_registry_impl::BuildingType *definition_for_provider(building_type type)
{
    type = normalize_provider_building_type(type);
    return type >= BUILDING_NONE && type < BUILDING_TYPE_MAX ?
        building_type_registry_impl::g_building_types[type].get() :
        nullptr;
}

const building_type_registry_impl::WaterAccessDefinition *provider_definition_for_building_type(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = definition_for_provider(type);
    return definition && definition->has_water_access_provider() ? &definition->water_access() : nullptr;
}

building_type_registry_impl::WaterAccessType provider_access_type_for_building_type(building_type type)
{
    if (type == BUILDING_AQUEDUCT) {
        return building_type_registry_impl::WaterAccessType::Reservoir;
    }
    const building_type_registry_impl::WaterAccessDefinition *definition = provider_definition_for_building_type(type);
    return definition ? definition->type() : building_type_registry_impl::WaterAccessType::None;
}

int provider_size_for_building_type(building_type type)
{
    return building_properties_for_type(normalize_provider_building_type(type))->size;
}

int provider_range_for_building_type(building_type type)
{
    const building_type_registry_impl::WaterAccessDefinition *definition = provider_definition_for_building_type(type);
    return definition ? definition->range() : 0;
}

building_type_registry_impl::WaterAccessRequirement provider_requirement_for_building_type(building_type type)
{
    const building_type_registry_impl::WaterAccessDefinition *definition = provider_definition_for_building_type(type);
    return definition ? definition->requirement() : building_type_registry_impl::WaterAccessRequirement::None;
}

const std::vector<building_type_registry_impl::WaterAccessNode> *provider_nodes_for_building_type(building_type type)
{
    const building_type_registry_impl::WaterAccessDefinition *definition = provider_definition_for_building_type(type);
    return definition ? &definition->nodes() : nullptr;
}

struct MaskSet {
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> access = {};
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> providers = {};
};

struct PreviewState {
    int active = 0;
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> highlight = {};
};

struct RuntimeState {
    MaskSet masks;
    PreviewState preview;
    int refreshed = 0;
} g_state;

struct PlannedProvider {
    building_type type = BUILDING_NONE;
    int grid_offset = 0;
};

struct ReservoirInstance {
    building_type type = BUILDING_RESERVOIR;
    int x = 0;
    int y = 0;
    int grid_offset = 0;
    int size = 3;
    uint8_t water_state = 0;
    uint8_t is_planned = 0;
};

struct SimulationInput {
    std::array<PlannedProvider, 2> planned_providers = {};
    int planned_provider_count = 0;
    int preview_aqueduct_grid_offset = 0;
    int include_constructing_aqueduct = 0;
};

struct SimulationResult {
    MaskSet masks;
    std::vector<ReservoirInstance> reservoirs;
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> wet_aqueduct = {};
};

std::array<uint8_t, GRID_SIZE * GRID_SIZE> *g_range_mask_target = nullptr;
uint8_t g_range_mask_bit = 0;

void mark_range_tile(int, int, int grid_offset)
{
    if (!g_range_mask_target || !map_grid_is_valid_offset(grid_offset)) {
        return;
    }
    (*g_range_mask_target)[grid_offset] |= g_range_mask_bit;
}

void clear_masks(MaskSet &masks)
{
    masks.access.fill(0);
    masks.providers.fill(0);
}

void ensure_runtime_refreshed();

int area_has_access(
    const std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    int x,
    int y,
    int size,
    uint8_t bit)
{
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            int grid_offset = map_grid_offset(x + dx, y + dy);
            if (map_grid_is_valid_offset(grid_offset) && (mask[grid_offset] & bit)) {
                return 1;
            }
        }
    }
    return 0;
}

void mark_building_footprint(
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    int x,
    int y,
    int size,
    uint8_t bit)
{
    for (int dy = 0; dy < size; dy++) {
        for (int dx = 0; dx < size; dx++) {
            int grid_offset = map_grid_offset(x + dx, y + dy);
            if (map_grid_is_valid_offset(grid_offset)) {
                mask[grid_offset] |= bit;
            }
        }
    }
}

void mark_range(
    std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask,
    int grid_offset,
    int size,
    int range,
    uint8_t bit)
{
    g_range_mask_target = &mask;
    g_range_mask_bit = bit;
    city_view_foreach_tile_in_range(grid_offset, size, range, mark_range_tile);
    g_range_mask_target = nullptr;
    g_range_mask_bit = 0;
}

void mark_provider_coverage(
    MaskSet &masks,
    building_type_registry_impl::WaterAccessType type,
    int x,
    int y,
    int grid_offset,
    int size,
    int range)
{
    const uint8_t bit = access_bit(type);
    if (!bit) {
        return;
    }
    if (range > 0) {
        mark_range(masks.access, grid_offset, size, range, bit);
    }
    mark_building_footprint(masks.providers, x, y, size, bit);
}

int is_aqueduct_tile_for_simulation(int grid_offset, const SimulationInput &input)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        return 1;
    }
    if (input.include_constructing_aqueduct && map_property_is_constructing(grid_offset)) {
        return 1;
    }
    return input.preview_aqueduct_grid_offset && input.preview_aqueduct_grid_offset == grid_offset;
}

int requirement_is_satisfied(
    building_type_registry_impl::WaterAccessRequirement requirement,
    int x,
    int y,
    int size,
    const std::array<uint8_t, GRID_SIZE * GRID_SIZE> &mask)
{
    switch (requirement) {
        case building_type_registry_impl::WaterAccessRequirement::ReservoirNetwork:
            return area_has_access(mask, x, y, size, access_bit(building_type_registry_impl::WaterAccessType::Reservoir));
        case building_type_registry_impl::WaterAccessRequirement::WaterSourceAny:
        case building_type_registry_impl::WaterAccessRequirement::WaterSourceFreshOnly:
            return map_terrain_exists_tile_in_area_with_type(x - 1, y - 1, size + 2, TERRAIN_WATER);
        case building_type_registry_impl::WaterAccessRequirement::None:
        default:
            return 1;
    }
}

int reservoir_has_node_at(const ReservoirInstance &reservoir, int grid_offset)
{
    const std::vector<building_type_registry_impl::WaterAccessNode> *nodes = provider_nodes_for_building_type(reservoir.type);
    if (!nodes) {
        return 0;
    }

    for (const building_type_registry_impl::WaterAccessNode &node : *nodes) {
        if (node.kind != building_type_registry_impl::WaterAccessNodeKind::AqueductConnection) {
            continue;
        }
        int node_offset = map_grid_offset(reservoir.x + node.x, reservoir.y + node.y);
        if (node_offset == grid_offset) {
            return 1;
        }
    }
    return 0;
}

void collect_reservoirs(SimulationResult &result, const SimulationInput &input)
{
    result.reservoirs.clear();
    for (building *b = building_first_of_type(BUILDING_RESERVOIR); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        ReservoirInstance reservoir;
        reservoir.type = BUILDING_RESERVOIR;
        reservoir.x = b->x;
        reservoir.y = b->y;
        reservoir.grid_offset = b->grid_offset;
        reservoir.size = b->size;
        result.reservoirs.push_back(reservoir);
    }

    for (int i = 0; i < input.planned_provider_count; i++) {
        const PlannedProvider &planned = input.planned_providers[i];
        if (normalize_provider_building_type(planned.type) != BUILDING_RESERVOIR || !planned.grid_offset) {
            continue;
        }

        ReservoirInstance reservoir;
        reservoir.type = BUILDING_RESERVOIR;
        reservoir.grid_offset = planned.grid_offset;
        reservoir.x = map_grid_offset_to_x(planned.grid_offset);
        reservoir.y = map_grid_offset_to_y(planned.grid_offset);
        reservoir.size = provider_size_for_building_type(BUILDING_RESERVOIR);
        reservoir.is_planned = 1;
        result.reservoirs.push_back(reservoir);
    }
}

void mark_dry_reservoirs_with_node_at(SimulationResult &result, int grid_offset)
{
    for (ReservoirInstance &reservoir : result.reservoirs) {
        if (!reservoir.water_state && reservoir_has_node_at(reservoir, grid_offset)) {
            reservoir.water_state = 2;
        }
    }
}

void fill_aqueducts_from_connection(int grid_offset, const SimulationInput &input, SimulationResult &result)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return;
    }

    static const int adjacent_offsets[] = { -GRID_SIZE, 1, GRID_SIZE, -1 };
    std::vector<int> queue;

    // Aqueduct traversal remains bespoke in this slice: the runtime only
    // decides which reservoir-declared node tiles may seed or receive flow.
    const auto visit_offset = [&](int offset, std::vector<int> &pending) {
        if (!map_grid_is_valid_offset(offset)) {
            return;
        }
        if (is_aqueduct_tile_for_simulation(offset, input)) {
            // A reservoir connection is made by the aqueduct tile occupying a
            // declared node, not by an empty node next to a wet aqueduct.
            mark_dry_reservoirs_with_node_at(result, offset);
            if (!result.wet_aqueduct[offset]) {
                result.wet_aqueduct[offset] = 1;
                pending.push_back(offset);
            }
        }
    };

    visit_offset(grid_offset, queue);
    while (!queue.empty()) {
        int current = queue.back();
        queue.pop_back();
        for (int adjacent_offset : adjacent_offsets) {
            visit_offset(current + adjacent_offset, queue);
        }
    }
}

void propagate_reservoir_network(const SimulationInput &input, SimulationResult &result)
{
    for (ReservoirInstance &reservoir : result.reservoirs) {
        if (requirement_is_satisfied(
                provider_requirement_for_building_type(reservoir.type),
                reservoir.x,
                reservoir.y,
                reservoir.size,
                result.masks.access)) {
            reservoir.water_state = 2;
        } else {
            reservoir.water_state = 0;
        }
    }

    int changed = 1;
    while (changed) {
        changed = 0;
        for (ReservoirInstance &reservoir : result.reservoirs) {
            if (reservoir.water_state != 2) {
                continue;
            }
            reservoir.water_state = 1;
            changed = 1;

            const std::vector<building_type_registry_impl::WaterAccessNode> *nodes = provider_nodes_for_building_type(reservoir.type);
            if (!nodes) {
                continue;
            }
            for (const building_type_registry_impl::WaterAccessNode &node : *nodes) {
                if (node.kind != building_type_registry_impl::WaterAccessNodeKind::AqueductConnection) {
                    continue;
                }
                fill_aqueducts_from_connection(
                    map_grid_offset(reservoir.x + node.x, reservoir.y + node.y),
                    input,
                    result);
            }
        }
    }
}

void apply_neptune_bonus(SimulationResult &result)
{
    if (!building_monument_gt_module_is_active(NEPTUNE_MODULE_2_CAPACITY_AND_WATER)) {
        return;
    }

    building *b = building_get(building_monument_get_neptune_gt());
    if (!b || !b->id) {
        return;
    }

    mark_provider_coverage(
        result.masks,
        building_type_registry_impl::WaterAccessType::Reservoir,
        b->x,
        b->y,
        b->grid_offset,
        7,
        water_access_runtime_range_for_building(BUILDING_RESERVOIR));
}

void build_water_masks(const SimulationInput &input, SimulationResult &result)
{
    clear_masks(result.masks);
    result.wet_aqueduct.fill(0);
    collect_reservoirs(result, input);
    propagate_reservoir_network(input, result);

    // Reservoir coverage must land first because fountain previews and live
    // activation both consume the already-resolved reservoir network map.
    for (const ReservoirInstance &reservoir : result.reservoirs) {
        // Ghost previews still need to show the footprint they would cover
        // even when the candidate reservoir is dry at the hovered position.
        if (!reservoir.water_state && !reservoir.is_planned) {
            continue;
        }
        mark_provider_coverage(
            result.masks,
            building_type_registry_impl::WaterAccessType::Reservoir,
            reservoir.x,
            reservoir.y,
            reservoir.grid_offset,
            reservoir.size,
            water_access_runtime_range_for_building(BUILDING_RESERVOIR));
    }

    apply_neptune_bonus(result);

    for (building *b = building_first_of_type(BUILDING_FOUNTAIN); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE || !b->num_workers) {
            continue;
        }
        if (!area_has_access(
                result.masks.access,
                b->x,
                b->y,
                b->size,
                access_bit(building_type_registry_impl::WaterAccessType::Reservoir))) {
            continue;
        }
        mark_provider_coverage(
            result.masks,
            building_type_registry_impl::WaterAccessType::Fountain,
            b->x,
            b->y,
            b->grid_offset,
            b->size,
            water_access_runtime_range_for_building(BUILDING_FOUNTAIN));
    }

    for (building *b = building_first_of_type(BUILDING_WELL); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        mark_provider_coverage(
            result.masks,
            building_type_registry_impl::WaterAccessType::Well,
            b->x,
            b->y,
            b->grid_offset,
            b->size,
            water_access_runtime_range_for_building(BUILDING_WELL));
    }

    for (int i = 0; i < input.planned_provider_count; i++) {
        const PlannedProvider &planned = input.planned_providers[i];
        if (!planned.grid_offset) {
            continue;
        }

        building_type type = normalize_provider_building_type(planned.type);
        const building_type_registry_impl::WaterAccessType access_type = provider_access_type_for_building_type(type);
        if (access_type == building_type_registry_impl::WaterAccessType::None ||
            access_type == building_type_registry_impl::WaterAccessType::Reservoir) {
            continue;
        }

        const int x = map_grid_offset_to_x(planned.grid_offset);
        const int y = map_grid_offset_to_y(planned.grid_offset);
        const int size = provider_size_for_building_type(type);
        if (!requirement_is_satisfied(provider_requirement_for_building_type(type), x, y, size, result.masks.access)) {
            continue;
        }

        mark_provider_coverage(
            result.masks,
            access_type,
            x,
            y,
            planned.grid_offset,
            size,
            water_access_runtime_range_for_building(type));
    }
}

void set_aqueduct_to_no_water(int grid_offset)
{
    map_aqueduct_set_water_access(grid_offset, 0);
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        map_image_set(grid_offset, map_tiles_highway_get_aqueduct_image(grid_offset));
        return;
    }

    int image_id = map_image_at(grid_offset);
    if (image_id < image_group(GROUP_BUILDING_AQUEDUCT_NO_WATER)) {
        map_image_set(grid_offset, image_id + kNoWaterImageOffset);
    }
}

void set_aqueduct_to_water(int grid_offset)
{
    map_aqueduct_set_water_access(grid_offset, 1);
    int image_id = map_image_at(grid_offset);
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        map_image_set(grid_offset, map_tiles_highway_get_aqueduct_image(grid_offset));
    } else if (image_id >= image_group(GROUP_BUILDING_AQUEDUCT_NO_WATER)) {
        map_image_set(grid_offset, image_id - kNoWaterImageOffset);
    }
}

void project_aqueduct_state(const SimulationResult &result)
{
    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (!map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
                continue;
            }
            set_aqueduct_to_no_water(grid_offset);
        }
    }

    grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT) && result.wet_aqueduct[grid_offset]) {
                set_aqueduct_to_water(grid_offset);
            }
        }
    }
}

void project_terrain_ranges(const SimulationResult &result)
{
    map_terrain_remove_all(TERRAIN_FOUNTAIN_RANGE | TERRAIN_RESERVOIR_RANGE);

    int grid_offset = map_data.start_offset;
    for (int y = 0; y < map_data.height; y++, grid_offset += map_data.border_size) {
        for (int x = 0; x < map_data.width; x++, grid_offset++) {
            if (result.masks.access[grid_offset] & access_bit(building_type_registry_impl::WaterAccessType::Reservoir)) {
                map_terrain_add(grid_offset, TERRAIN_RESERVOIR_RANGE);
            }
            if (result.masks.access[grid_offset] & access_bit(building_type_registry_impl::WaterAccessType::Fountain)) {
                map_terrain_add(grid_offset, TERRAIN_FOUNTAIN_RANGE);
            }
        }
    }
}

void mark_well_access(building *well, int radius)
{
    int x_min = 0;
    int y_min = 0;
    int x_max = 0;
    int y_max = 0;
    map_grid_get_area(well->x, well->y, 1, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int building_id = map_building_at(map_grid_offset(xx, yy));
            if (building_id) {
                building_get(building_id)->has_well_access = 1;
            }
        }
    }
}

void mark_latrines_access(building *latrines, int radius)
{
    int x_min = 0;
    int y_min = 0;
    int x_max = 0;
    int y_max = 0;
    map_grid_get_area(latrines->x, latrines->y, 1, radius, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int building_id = map_building_at(map_grid_offset(xx, yy));
            if (building_id && latrines->num_workers > 0) {
                building_get(building_id)->has_latrines_access = 1;
            }
        }
    }
}

void project_building_state(const SimulationResult &result)
{
    for (building_type type = BUILDING_HOUSE_SMALL_TENT; type <= BUILDING_HOUSE_LUXURY_PALACE; type = static_cast<building_type>(type + 1)) {
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE || !b->house_size) {
                continue;
            }
            b->has_water_access = area_has_access(
                result.masks.access,
                b->x,
                b->y,
                b->size,
                access_bit(building_type_registry_impl::WaterAccessType::Fountain)) ? 1 : 0;
            b->has_well_access = 0;
            b->has_latrines_access = 0;
        }
    }

    for (building *b = building_first_of_type(BUILDING_CONCRETE_MAKER); b; b = b->next_of_type) {
        b->has_well_access = 0;
    }

    for (building *b = building_first_of_type(BUILDING_WELL); b; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE) {
            mark_well_access(b, water_access_runtime_range_for_building(BUILDING_WELL));
        }
    }

    for (building *b = building_first_of_type(BUILDING_LATRINES); b; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE) {
            mark_latrines_access(b, water_access_runtime_range_for_building(BUILDING_LATRINES));
        }
    }

    for (building *b = building_first_of_type(BUILDING_CONCRETE_MAKER); b; b = b->next_of_type) {
        b->has_water_access = 0;
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        if (area_has_access(
                result.masks.access,
                b->x,
                b->y,
                b->size,
                access_bit(building_type_registry_impl::WaterAccessType::Reservoir))) {
            b->has_water_access = 2;
        } else if (area_has_access(
                result.masks.access,
                b->x,
                b->y,
                b->size,
                access_bit(building_type_registry_impl::WaterAccessType::Fountain)) ||
                b->has_well_access) {
            b->has_water_access = 1;
        }
    }

    for (building *b = building_first_of_type(BUILDING_RESERVOIR); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        b->has_water_access = area_has_access(
            result.masks.providers,
            b->x,
            b->y,
            b->size,
            access_bit(building_type_registry_impl::WaterAccessType::Reservoir)) ? 1 : 0;
    }

    for (building *b = building_first_of_type(BUILDING_FOUNTAIN); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        if (b->desirability > 60) {
            b->upgrade_level = 3;
        } else if (b->desirability > 40) {
            b->upgrade_level = 2;
        } else if (b->desirability > 20) {
            b->upgrade_level = 1;
        } else {
            b->upgrade_level = 0;
        }

        b->has_water_access = (b->num_workers &&
            area_has_access(
                result.masks.access,
                b->x,
                b->y,
                b->size,
                access_bit(building_type_registry_impl::WaterAccessType::Reservoir))) ? 1 : 0;
        map_building_tiles_add(b->id, b->x, b->y, 1, building_image_get(b), TERRAIN_BUILDING);
    }

    static const building_type pond_types[] = { BUILDING_SMALL_POND, BUILDING_LARGE_POND };
    for (building_type pond_type : pond_types) {
        for (building *b = building_first_of_type(pond_type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE) {
                continue;
            }
            b->has_water_access = area_has_access(
                result.masks.access,
                b->x,
                b->y,
                b->size,
                access_bit(building_type_registry_impl::WaterAccessType::Reservoir)) ? 1 : 0;
            map_building_tiles_add(b->id, b->x, b->y, b->size, building_image_get(b), TERRAIN_BUILDING);
        }
    }

    for (building *b = building_first_of_type(BUILDING_LARGE_STATUE); b; b = b->next_of_type) {
        water_access_runtime_refresh_large_statue(b);
    }

    for (building *b = building_first_of_type(BUILDING_COLOSSEUM); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        b->has_water_access = area_has_access(
            result.masks.access,
            b->x,
            b->y,
            b->size,
            access_bit(building_type_registry_impl::WaterAccessType::Reservoir)) ? 1 : 0;
    }
}

void build_preview_highlight(
    building_type type,
    const SimulationResult &preview_result)
{
    g_state.preview.highlight.fill(0);
    g_state.preview.active = 0;

    const building_type_registry_impl::WaterAccessType provider_type = provider_access_type_for_building_type(type);
    const uint8_t bit = access_bit(provider_type);
    if (!bit) {
        return;
    }

    g_state.preview.active = 1;
    // Placement preview shows the union of existing coverage plus every tile
    // the current ghost would newly cover, matching the requested blue overlay.
    for (size_t i = 0; i < g_state.preview.highlight.size(); i++) {
        if ((g_state.masks.access[i] & bit) || (g_state.masks.providers[i] & bit) ||
            (preview_result.masks.access[i] & bit) || (preview_result.masks.providers[i] & bit)) {
            g_state.preview.highlight[i] = 1;
        }
    }
}

void ensure_runtime_refreshed()
{
    if (!g_state.refreshed) {
        water_access_runtime_refresh();
    }
}

} // namespace

extern "C" void water_access_runtime_reset(void)
{
    clear_masks(g_state.masks);
    g_state.preview = PreviewState();
    g_state.refreshed = 0;
}

extern "C" void water_access_runtime_refresh(void)
{
    SimulationInput input;
    SimulationResult result;
    build_water_masks(input, result);
    g_state.masks = result.masks;
    g_state.refreshed = 1;

    project_aqueduct_state(result);
    project_terrain_ranges(result);
    project_building_state(result);
}

extern "C" void water_access_runtime_refresh_large_statue(building *b)
{
    if (!b || b->type != BUILDING_LARGE_STATUE) {
        return;
    }

    ensure_runtime_refreshed();
    b->has_water_access = area_has_access(
        g_state.masks.access,
        b->x,
        b->y,
        b->size,
        access_bit(building_type_registry_impl::WaterAccessType::Reservoir)) ? 1 : 0;

    if (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_MOTHBALLED || b->state == BUILDING_STATE_CREATED) {
        map_building_tiles_add(b->id, b->x, b->y, b->size, building_image_get(b), TERRAIN_BUILDING);
    }
}

extern "C" int water_access_runtime_range_for_building(building_type type)
{
    switch (normalize_provider_building_type(type)) {
        case BUILDING_FOUNTAIN:
        {
            int radius = provider_range_for_building_type(BUILDING_FOUNTAIN);
            if (scenario_property_climate() == CLIMATE_DESERT) {
                radius--;
            }
            if (building_monument_working(BUILDING_GRAND_TEMPLE_NEPTUNE)) {
                radius++;
            }
            return radius;
        }
        case BUILDING_RESERVOIR:
        {
            int radius = provider_range_for_building_type(BUILDING_RESERVOIR);
            if (building_monument_working(BUILDING_GRAND_TEMPLE_NEPTUNE)) {
                radius += 2;
            }
            return radius;
        }
        case BUILDING_WELL:
        {
            int radius = provider_range_for_building_type(BUILDING_WELL);
            if (building_monument_working(BUILDING_GRAND_TEMPLE_NEPTUNE)) {
                radius++;
            }
            return radius;
        }
        case BUILDING_LATRINES:
            return 3;
        default:
            return provider_range_for_building_type(type);
    }
}

extern "C" int water_access_runtime_tile_has_access(int grid_offset, int access_type)
{
    ensure_runtime_refreshed();
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }

    uint8_t bit = 0;
    switch (access_type) {
        case WATER_ACCESS_RUNTIME_TYPE_WELL:
            bit = access_bit(building_type_registry_impl::WaterAccessType::Well);
            break;
        case WATER_ACCESS_RUNTIME_TYPE_FOUNTAIN:
            bit = access_bit(building_type_registry_impl::WaterAccessType::Fountain);
            break;
        case WATER_ACCESS_RUNTIME_TYPE_RESERVOIR:
            bit = access_bit(building_type_registry_impl::WaterAccessType::Reservoir);
            break;
        default:
            return 0;
    }

    return (g_state.masks.access[grid_offset] & bit) != 0;
}

extern "C" int water_access_runtime_building_area_has_access(const building *b, int access_type)
{
    if (!b) {
        return 0;
    }
    ensure_runtime_refreshed();

    uint8_t bit = 0;
    switch (access_type) {
        case WATER_ACCESS_RUNTIME_TYPE_WELL:
            bit = access_bit(building_type_registry_impl::WaterAccessType::Well);
            break;
        case WATER_ACCESS_RUNTIME_TYPE_FOUNTAIN:
            bit = access_bit(building_type_registry_impl::WaterAccessType::Fountain);
            break;
        case WATER_ACCESS_RUNTIME_TYPE_RESERVOIR:
            bit = access_bit(building_type_registry_impl::WaterAccessType::Reservoir);
            break;
        default:
            return 0;
    }
    return area_has_access(g_state.masks.access, b->x, b->y, b->size, bit);
}

extern "C" int water_access_runtime_reservoir_has_network_access(int grid_offset)
{
    ensure_runtime_refreshed();
    const std::vector<building_type_registry_impl::WaterAccessNode> *nodes = provider_nodes_for_building_type(BUILDING_RESERVOIR);
    if (!nodes) {
        return 0;
    }

    const int x = map_grid_offset_to_x(grid_offset);
    const int y = map_grid_offset_to_y(grid_offset);
    for (const building_type_registry_impl::WaterAccessNode &node : *nodes) {
        if (node.kind != building_type_registry_impl::WaterAccessNodeKind::AqueductConnection) {
            continue;
        }
        const int node_offset = map_grid_offset(x + node.x, y + node.y);
        if (map_grid_is_valid_offset(node_offset) && map_aqueduct_has_water_access_at(node_offset)) {
            return 1;
        }
        for (building *reservoir = building_first_of_type(BUILDING_RESERVOIR); reservoir; reservoir = reservoir->next_of_type) {
            if (reservoir->state != BUILDING_STATE_IN_USE || !reservoir->has_water_access) {
                continue;
            }
            ReservoirInstance instance;
            instance.type = BUILDING_RESERVOIR;
            instance.x = reservoir->x;
            instance.y = reservoir->y;
            instance.grid_offset = reservoir->grid_offset;
            instance.size = reservoir->size;
            if (reservoir_has_node_at(instance, node_offset)) {
                return 1;
            }
        }
    }
    return 0;
}

extern "C" void water_access_runtime_begin_preview(building_type type, int primary_grid_offset, int secondary_grid_offset)
{
    ensure_runtime_refreshed();

    SimulationInput input;
    if (type == BUILDING_AQUEDUCT) {
        input.preview_aqueduct_grid_offset = primary_grid_offset;
        input.include_constructing_aqueduct = 1;
    } else {
        if (primary_grid_offset) {
            PlannedProvider &provider = input.planned_providers[input.planned_provider_count++];
            provider.type = type;
            provider.grid_offset = primary_grid_offset;
        }
        if (secondary_grid_offset && secondary_grid_offset != primary_grid_offset && input.planned_provider_count < 2) {
            PlannedProvider &provider = input.planned_providers[input.planned_provider_count++];
            provider.type = type;
            provider.grid_offset = secondary_grid_offset;
        }
        if (type == BUILDING_DRAGGABLE_RESERVOIR) {
            input.include_constructing_aqueduct = 1;
        }
    }

    SimulationResult preview_result;
    build_water_masks(input, preview_result);
    build_preview_highlight(type, preview_result);
}

extern "C" void water_access_runtime_end_preview(void)
{
    g_state.preview = PreviewState();
}

extern "C" int water_access_runtime_tile_has_preview_highlight(int grid_offset)
{
    return g_state.preview.active && map_grid_is_valid_offset(grid_offset) && g_state.preview.highlight[grid_offset];
}

extern "C" int water_access_runtime_should_draw_overlay_at(int grid_offset)
{
    if (!map_grid_is_valid_offset(grid_offset)) {
        return 0;
    }
    if (map_terrain_is(grid_offset, TERRAIN_ELEVATION | TERRAIN_ACCESS_RAMP | TERRAIN_AQUEDUCT)) {
        return 0;
    }
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        const building *b = building_get(map_building_at(grid_offset));
        if (b && (b->type == BUILDING_WELL || b->type == BUILDING_FOUNTAIN || b->type == BUILDING_RESERVOIR ||
                (b->type == BUILDING_GRAND_TEMPLE_NEPTUNE && building_monument_gt_module_is_active(NEPTUNE_MODULE_2_CAPACITY_AND_WATER)))) {
            return 0;
        }
    }
    return 1;
}
