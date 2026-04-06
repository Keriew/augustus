#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"

#include "assets/image_group_payload.h"
#include "building/building_runtime_graphics.h"
#include "core/crash_context.h"

extern "C" {
#include "building/building_runtime_api.h"
#include "building/count.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/movement.h"
#include "game/animation.h"
#include "game/resource.h"
#include "map/building_tiles.h"
#include "map/road_access.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "core/log.h"
}

#include <cstdio>
#include <string>
#include <unordered_set>

namespace {

std::unordered_set<std::string> g_logged_building_graphics_fallbacks;

void advance_runtime_monument_secondary_animation(building *b)
{
    if (b && b->type == BUILDING_GRAND_TEMPLE_CERES && b->monument.upgrades == 1) {
        b->monument.secondary_frame++;
        if (b->monument.secondary_frame > 4) {
            b->monument.secondary_frame = 0;
        }
    }
}

void make_building_context(char *buffer, size_t buffer_size, const building *b)
{
    if (b) {
        snprintf(
            buffer,
            buffer_size,
            "building_id=%u type=%d grid_offset=%d",
            b->id,
            static_cast<int>(b->type),
            b->grid_offset);
    } else {
        snprintf(buffer, buffer_size, "building=null");
    }
}

void log_building_scope_state(void *userdata)
{
    const building_runtime *runtime = static_cast<const building_runtime *>(userdata);
    const building *b = runtime ? runtime->building() : nullptr;
    const building_type_registry_impl::BuildingType *definition = runtime ? runtime->definition() : nullptr;
    if (!b) {
        return;
    }

    std::vector<const building_type_registry_impl::GraphicsTarget *> targets =
        definition ? definition->resolve_graphics_targets(*b) : std::vector<const building_type_registry_impl::GraphicsTarget *>();
    const building_type_registry_impl::GraphicsTarget *target = targets.empty() ? nullptr : targets.front();

    char details[256];
    snprintf(
        details,
        sizeof(details),
        "state=%d size=%d desirability=%d water=%d upgrade=%d target_path=%s target_image=%s",
        b->state,
        b->size,
        b->desirability,
        b->has_water_access,
        b->upgrade_level,
        target && target->has_path() ? target->path() : "",
        target && target->has_image() ? target->image() : "");
    log_info("Graphics building state", details, 0);
}

void log_building_graphics_fallback_once(
    const char *message,
    const building_runtime *runtime,
    const char *detail,
    const char *group_key = nullptr,
    const char *image_id = nullptr)
{
    const building *b = runtime ? runtime->building() : nullptr;
    const building_type_registry_impl::BuildingType *definition = runtime ? runtime->definition() : nullptr;

    char key_buffer[768];
    snprintf(
        key_buffer,
        sizeof(key_buffer),
        "%s|type=%d|path=%s|image=%s|detail=%s",
        message ? message : "",
        b ? static_cast<int>(b->type) : -1,
        group_key ? group_key : "",
        image_id ? image_id : "",
        detail ? detail : "");

    if (!g_logged_building_graphics_fallbacks.insert(key_buffer).second) {
        return;
    }

    error_context_report_error(message, detail);
}

}

namespace building_runtime_impl {

std::vector<std::unique_ptr<building_runtime>> g_runtime_instances;

building_runtime *get_city_building(unsigned int id)
{
    if (!id) {
        return nullptr;
    }
    return get_or_create_instance(building_get(id));
}

building_runtime *get_city_building(::building *building_data)
{
    return get_or_create_instance(building_data);
}

building_runtime *get_or_create_instance(::building *building_data)
{
    if (!building_data || !building_data->id) {
        return nullptr;
    }

    // For now runtime wrappers are materialized lazily, but they are owned here rather than by the type registry.
    if (g_runtime_instances.size() <= building_data->id) {
        g_runtime_instances.resize(building_data->id + 1);
    }

    // Every live building gets a runtime object, even before that type has migrated to XML-driven behavior.
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::g_building_types[building_data->type].get();

    std::unique_ptr<building_runtime> &slot = g_runtime_instances[building_data->id];
    if (!slot || slot->building() != building_data || slot->definition() != definition) {
        slot = std::make_unique<building_runtime>(building_data, definition);
    }
    return slot.get();
}

}

void building_runtime::refresh_runtime_state()
{
    if (!building_ || !definition_ || !definition_->has_graphic()) {
        return;
    }

    if (definition_->water_access_mode() == building_type_registry_impl::WaterAccessMode::ReservoirRange) {
        building_->has_water_access =
            map_terrain_exists_tile_in_area_with_type(building_->x, building_->y, building_->size, TERRAIN_RESERVOIR_RANGE);
    }

    building_->upgrade_level = definition_->upgrade_level_for(*building_);
}

void building_runtime::clear_resolved_graphics()
{
    resolved_graphic_footprint_ = {};
    resolved_graphic_top_ = {};
    owns_graphics_ = 0;
    owns_graphic_animation_ = 0;
    resolved_animation_frame_slices_.clear();
}

int building_runtime::uses_new_graphics() const
{
    return building_ && definition_ && definition_->has_graphic();
}

int building_runtime::graphics_state_is_authoritative() const
{
    return uses_new_graphics();
}

std::vector<const building_type_registry_impl::GraphicsTarget *> building_runtime::resolve_graphic_targets() const
{
    if (!uses_new_graphics()) {
        return {};
    }
    return definition_->resolve_graphics_targets(*building_);
}

const ImageGroupEntry *building_runtime::resolve_graphic_entry(const building_type_registry_impl::GraphicsTarget *target) const
{
    if (!uses_new_graphics() || !target || !target->has_path()) {
        return nullptr;
    }

    if (!image_group_payload_load(target->path())) {
        log_building_graphics_fallback_once(
            "Building graphics group could not be resolved.",
            this,
            target->path(),
            target->path(),
            target->has_image() ? target->image() : nullptr);
        return nullptr;
    }

    const ImageGroupPayload *payload = image_group_payload_get(target->path());
    if (!payload) {
        log_building_graphics_fallback_once(
            "Building graphics group payload could not be resolved.",
            this,
            target->path(),
            target->path(),
            target->has_image() ? target->image() : nullptr);
        return nullptr;
    }

    const ImageGroupEntry *entry = target->has_image() ? payload->entry_for(target->image()) : payload->default_entry();
    if (!entry) {
        log_building_graphics_fallback_once(
            target->has_image() ? "Building graphics image id could not be resolved." :
                                  "Building graphics default entry could not be resolved.",
            this,
            target->has_image() ? target->image() : target->path(),
            target->path(),
            target->has_image() ? target->image() : payload->default_image_id());
        return nullptr;
    }

    return entry;
}

// Input: one runtime animation track plus the live building/grid state that owns it.
// Output: the 1-based frame index that should be drawn now, while mirroring legacy timing/gating without using legacy image payloads.
int building_runtime::mirror_animation_offset(const RuntimeAnimationTrack &track) const
{
    if (!building_ || track.num_frames <= 0) {
        return 0;
    }

    if (building_->type == BUILDING_FOUNTAIN && (building_->num_workers <= 0 || !building_->has_water_access)) {
        return 0;
    }
    if (building_->type == BUILDING_RESERVOIR && !building_->has_water_access) {
        return 0;
    }
    if (building_is_workshop(building_->type)) {
        if (building_->num_workers <= 0 || building_->strike_duration_days > 0 ||
            !building_industry_has_raw_materials_for_production(building_)) {
            return 0;
        }
    }
    if (building_->type == BUILDING_CONCRETE_MAKER) {
        if (!building_->has_water_access || building_->data.industry.progress == 0) {
            return 0;
        }
    }
    if ((building_->type == BUILDING_PREFECTURE || building_->type == BUILDING_ENGINEERS_POST) && building_->num_workers <= 0) {
        return 0;
    }
    if (building_->type == BUILDING_MARKET && building_->num_workers <= 0) {
        return 0;
    }
    if (building_->type == BUILDING_WAREHOUSE && building_->num_workers < model_get_building(building_->type)->laborers) {
        return 0;
    }
    if (building_->type == BUILDING_DOCK && building_->data.dock.num_ships <= 0) {
        map_sprite_animation_set(building_->grid_offset, 1);
        return 1;
    }
    if (building_->type == BUILDING_MARBLE_QUARRY && (building_->num_workers <= 0 || building_->strike_duration_days > 0)) {
        map_sprite_animation_set(building_->grid_offset, 1);
        return 1;
    } else if (building_is_raw_resource_producer(building_->type) && (building_->num_workers <= 0 || building_->strike_duration_days > 0)) {
        return 0;
    }
    if (building_->type == BUILDING_GLADIATOR_SCHOOL) {
        if (building_->num_workers <= 0) {
            map_sprite_animation_set(building_->grid_offset, 1);
            return 1;
        }
    } else if (building_->type >= BUILDING_THEATER && building_->type <= BUILDING_CHARIOT_MAKER &&
        building_->type != BUILDING_HIPPODROME && building_->num_workers <= 0) {
        return 0;
    }
    if (building_->type == BUILDING_GRANARY && building_->num_workers < model_get_building(building_->type)->laborers) {
        return 0;
    }
    if (building_monument_is_monument(building_) && (building_->type != BUILDING_ORACLE && building_->type != BUILDING_NYMPHAEUM &&
        (building_->num_workers <= 0 || building_->monument.phase != MONUMENT_FINISHED))) {
        return 0;
    }
    if (building_->type == BUILDING_CITY_MINT &&
        ((building_->output_resource_id == RESOURCE_DENARII &&
            building_->resources[RESOURCE_GOLD] < BUILDING_INDUSTRY_CITY_MINT_GOLD_PER_COIN) || building_->num_workers <= 0 ||
            (building_count_active(BUILDING_SENATE) == 0))) {
        return 0;
    }
    if ((building_->type == BUILDING_ARCHITECT_GUILD || building_->type == BUILDING_MESS_HALL || building_->type == BUILDING_ARENA)
        && building_->num_workers <= 0) {
        return 0;
    }
    if (building_->type == BUILDING_TAVERN && (building_->num_workers <= 0 || !building_->resources[RESOURCE_WINE])) {
        return 0;
    }
    if (building_->type == BUILDING_WATCHTOWER && (building_->num_workers <= 0 || !building_->figure_id4)) {
        return 0;
    }
    if (building_->type == BUILDING_LARGE_STATUE && !building_->has_water_access) {
        return 0;
    }
    if (building_->type == BUILDING_DEPOT && building_->num_workers <= 0) {
        return 0;
    }
    if (building_->type == BUILDING_ARMOURY && building_->num_workers <= 0) {
        return 0;
    }
    if (building_->type == BUILDING_AMPHITHEATER && building_->num_workers <= 0) {
        return 0;
    }

    if (!track.speed_id) {
        return map_sprite_animation_at(building_->grid_offset) & 0x7f;
    }
    if (!game_animation_should_advance(track.speed_id)) {
        return map_sprite_animation_at(building_->grid_offset) & 0x7f;
    }

    int new_sprite = 0;
    int is_reverse = 0;
    if (building_->type == BUILDING_WINE_WORKSHOP) {
        const int pct_done = calc_percentage(building_->data.industry.progress, building_industry_get_max_progress(building_));
        const int current_sprite = map_sprite_animation_at(building_->grid_offset);
        if (pct_done <= 0) {
            new_sprite = 0;
        } else if (pct_done < 4) {
            new_sprite = 1;
        } else if (pct_done < 8) {
            new_sprite = 2;
        } else if (pct_done < 12) {
            new_sprite = 3;
        } else if (pct_done < 96) {
            const int first_mid_sprite = 4;
            const int last_mid_sprite = track.num_frames < 8 ? track.num_frames : 8;
            if (current_sprite < first_mid_sprite) {
                new_sprite = first_mid_sprite;
            } else {
                new_sprite = current_sprite + 1;
                if (new_sprite > last_mid_sprite) {
                    new_sprite = first_mid_sprite;
                }
            }
        } else {
            const int first_late_sprite = track.num_frames < 9 ? track.num_frames : 9;
            if (current_sprite < first_late_sprite) {
                new_sprite = first_late_sprite;
            } else {
                new_sprite = current_sprite + 1;
                if (new_sprite > track.num_frames) {
                    new_sprite = track.num_frames;
                }
            }
        }
    } else if (track.can_reverse) {
        if (map_sprite_animation_at(building_->grid_offset) & 0x80) {
            is_reverse = 1;
        }
        const int current_sprite = map_sprite_animation_at(building_->grid_offset) & 0x7f;
        if (is_reverse) {
            new_sprite = current_sprite - 1;
            if (new_sprite < 1) {
                new_sprite = 1;
                is_reverse = 0;
            }
        } else {
            new_sprite = current_sprite + 1;
            if (new_sprite > track.num_frames) {
                new_sprite = track.num_frames;
                is_reverse = 1;
            }
        }
    } else {
        new_sprite = map_sprite_animation_at(building_->grid_offset) + 1;
        if (new_sprite > track.num_frames) {
            advance_runtime_monument_secondary_animation(building_);
            new_sprite = 1;
        }
    }

    map_sprite_animation_set(building_->grid_offset, is_reverse ? new_sprite | 0x80 : new_sprite);
    return new_sprite;
}

// Input: one resolved native graphics entry that may own animation frames.
// Output: no return value; the currently active frame slice is appended to the resolved runtime draw list when the track is active.
void building_runtime::append_graphic_animation_layer(const ImageGroupEntry &entry)
{
    owns_graphic_animation_ = entry.has_animation() ? 1 : 0;
    if (!entry.has_animation()) {
        return;
    }

    const RuntimeAnimationTrack &track = entry.animation();
    const int animation_offset = mirror_animation_offset(track);
    if (animation_offset <= 0) {
        return;
    }

    const size_t frame_index = static_cast<size_t>(animation_offset - 1);
    if (frame_index >= track.frames.size()) {
        return;
    }

    RuntimeDrawSlice frame_slice = track.frames[frame_index];
    frame_slice.draw_offset_x += track.sprite_offset_x;
    frame_slice.draw_offset_y += track.sprite_offset_y;
    if (const RuntimeDrawSlice *top_slice = entry.top()) {
        frame_slice.draw_offset_y += top_slice->draw_offset_y;
    }
    resolved_animation_frame_slices_.push_back(frame_slice);
}

void building_runtime::resolve_graphics_state()
{
    clear_resolved_graphics();
    if (!uses_new_graphics()) {
        return;
    }

    owns_graphics_ = graphics_state_is_authoritative();
    owns_graphic_animation_ = 0;
    refresh_runtime_state();

    std::vector<const building_type_registry_impl::GraphicsTarget *> matched_targets = resolve_graphic_targets();
    if (matched_targets.empty()) {
        log_building_graphics_fallback_once(
            "Building graphics target could not be resolved.",
            this,
            definition_ ? definition_->attr() : "");
        owns_graphics_ = 0;
        owns_graphic_animation_ = 0;
        return;
    }

    const building_type_registry_impl::GraphicsTarget *selected_base_target = nullptr;
    const ImageGroupEntry *selected_base_entry = nullptr;
    std::vector<const ImageGroupEntry *> animation_entries;

    for (const building_type_registry_impl::GraphicsTarget *target : matched_targets) {
        if (!target || !target->has_path()) {
            continue;
        }

        const ImageGroupEntry *entry = resolve_graphic_entry(target);
        if (!entry) {
            continue;
        }

        if (!selected_base_entry && entry->footprint()) {
            selected_base_target = target;
            selected_base_entry = entry;
        }

        if (entry->has_animation()) {
            animation_entries.push_back(entry);
        }
    }

    if (!selected_base_entry) {
        const building_type_registry_impl::GraphicsTarget &default_target = definition_->graphics().default_target();
        if (default_target.has_path()) {
            const ImageGroupEntry *default_entry = resolve_graphic_entry(&default_target);
            if (default_entry && default_entry->footprint()) {
                selected_base_target = &default_target;
                selected_base_entry = default_entry;
            }
        }
    }

    if (!selected_base_entry) {
        log_building_graphics_fallback_once(
            "Building graphics entry is missing a footprint slice. Falling back to legacy rendering.",
            this,
            definition_ ? definition_->attr() : "",
            selected_base_target && selected_base_target->has_path() ? selected_base_target->path() : nullptr,
            selected_base_target && selected_base_target->has_image() ? selected_base_target->image() : nullptr);
        owns_graphics_ = 0;
        owns_graphic_animation_ = 0;
        return;
    }

    if (const RuntimeDrawSlice *footprint = selected_base_entry->footprint()) {
        resolved_graphic_footprint_ = *footprint;
    }
    if (const RuntimeDrawSlice *top = selected_base_entry->top()) {
        resolved_graphic_top_ = *top;
    }

    for (const ImageGroupEntry *animation_entry : animation_entries) {
        if (!animation_entry) {
            continue;
        }
        append_graphic_animation_layer(*animation_entry);
    }

    if (!resolved_graphic_footprint_.is_valid() && !resolved_graphic_top_.is_valid() && resolved_animation_frame_slices_.empty()) {
        owns_graphics_ = 0;
        owns_graphic_animation_ = 0;
    }
}

// Input: the runtime building wrapper for one live building.
// Output: the current native footprint slice for renderer-owned building graphics, or null when this building stays on the legacy path.
const RuntimeDrawSlice *building_runtime::graphic_footprint()
{
    if (!uses_new_graphics()) {
        return nullptr;
    }

    char context[256];
    make_building_context(context, sizeof(context), building_);
    CrashContextScope crash_scope(
        "building_runtime.resolve_base_image",
        context,
        log_building_scope_state,
        this);
    resolve_graphics_state();
    return resolved_graphic_footprint_.is_valid() ? &resolved_graphic_footprint_ : nullptr;
}

// Input: the runtime building wrapper for one live building.
// Output: the current native top slice for renderer-owned building graphics, or null when no separate top exists.
const RuntimeDrawSlice *building_runtime::graphic_top()
{
    if (!uses_new_graphics()) {
        return nullptr;
    }

    char context[256];
    make_building_context(context, sizeof(context), building_);
    CrashContextScope crash_scope(
        "building_runtime.resolve_top_image",
        context,
        log_building_scope_state,
        this);
    resolve_graphics_state();
    return resolved_graphic_top_.is_valid() ? &resolved_graphic_top_ : nullptr;
}

void building_runtime::set_building_graphic()
{
    if (!building_ || !definition_ || !definition_->has_graphic() || building_->state != BUILDING_STATE_IN_USE) {
        return;
    }

    refresh_runtime_state();
    map_building_tiles_add(building_->id, building_->x, building_->y, building_->size, building_image_get(building_), TERRAIN_BUILDING);
}

int building_runtime::worker_percentage() const
{
    return calc_percentage(building_->num_workers, model_get_building(building_->type)->laborers);
}

void building_runtime::check_labor_problem()
{
    if (building_->houses_covered <= 0) {
        building_->show_on_problem_overlay = 2;
    }
}

void building_runtime::generate_labor_seeker(int x, int y)
{
    if (city_population() <= 0) {
        return;
    }
    if (config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        if (building_->distance_from_entry) {
            building_->houses_covered = 100;
        } else {
            building_->houses_covered = 0;
        }
        return;
    }
    if (building_->figure_id2) {
        figure *existing = figure_get(building_->figure_id2);
        if (!existing->state || existing->type != FIGURE_LABOR_SEEKER || existing->building_id != building_->id) {
            building_->figure_id2 = 0;
        }
        return;
    }

    figure *labor_seeker = figure_create(FIGURE_LABOR_SEEKER, x, y, DIR_0_TOP);
    labor_seeker->action_state = FIGURE_ACTION_125_ROAMING;
    labor_seeker->building_id = building_->id;
    building_->figure_id2 = labor_seeker->id;
    figure_movement_init_roaming(labor_seeker);
}

void building_runtime::spawn_labor_seeker(int x, int y, int min_houses)
{
    if (config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        if (building_->distance_from_entry) {
            building_->houses_covered = 2 * min_houses;
        } else {
            building_->houses_covered = 0;
        }
    } else if (building_->houses_covered <= min_houses) {
        generate_labor_seeker(x, y);
    }
}

void building_runtime::run_labor_phase(const building_type_registry_impl::LaborPolicy &labor_policy, const map_point &road)
{
    switch (labor_policy.labor_seeker_mode) {
        case building_type_registry_impl::LaborSeekerMode::SpawnIfBelow:
            spawn_labor_seeker(road.x, road.y, labor_policy.labor_min_houses);
            break;
        case building_type_registry_impl::LaborSeekerMode::GenerateIfBelow:
            if (building_->houses_covered <= labor_policy.labor_min_houses) {
                generate_labor_seeker(road.x, road.y);
            }
            break;
        case building_type_registry_impl::LaborSeekerMode::None:
        default:
            break;
    }
}

int building_runtime::has_figure_of_type(figure_type type)
{
    if (building_->figure_id <= 0) {
        return 0;
    }
    figure *existing = figure_get(building_->figure_id);
    if (existing->state && existing->building_id == building_->id && existing->type == type) {
        return 1;
    }
    building_->figure_id = 0;
    return 0;
}

int building_runtime::has_figure_of_any(const std::vector<figure_type> &types)
{
    for (figure_type type : types) {
        if (has_figure_of_type(type)) {
            return 1;
        }
    }
    return 0;
}

int building_runtime::resolve_road_access(building_type_registry_impl::RoadAccessMode mode, map_point *road) const
{
    switch (mode) {
        case building_type_registry_impl::RoadAccessMode::Normal:
            return map_has_road_access(building_->x, building_->y, building_->size, road);
        case building_type_registry_impl::RoadAccessMode::None:
        default:
            return 0;
    }
}

int building_runtime::evaluate_delay(const std::vector<building_type_registry_impl::DelayBand> &delay_bands) const
{
    int pct_workers = worker_percentage();
    for (const building_type_registry_impl::DelayBand &delay_band : delay_bands) {
        if (pct_workers >= delay_band.min_worker_percentage) {
            return delay_band.delay;
        }
    }
    return 0;
}

int building_runtime::evaluate_condition(building_type_registry_impl::SpawnCondition condition) const
{
    switch (condition) {
        case building_type_registry_impl::SpawnCondition::Always:
            return 1;
        case building_type_registry_impl::SpawnCondition::Days1Positive:
            return building_->data.entertainment.days1 > 0;
        case building_type_registry_impl::SpawnCondition::Days1NotPositive:
            return building_->data.entertainment.days1 <= 0;
        case building_type_registry_impl::SpawnCondition::Days2Positive:
            return building_->data.entertainment.days2 > 0;
        case building_type_registry_impl::SpawnCondition::Days1OrDays2Positive:
            return building_->data.entertainment.days1 > 0 || building_->data.entertainment.days2 > 0;
        default:
            return 0;
    }
}

int building_runtime::should_apply_graphic_for_timing(
    const building_type_registry_impl::SpawnDelayGroup &group,
    building_type_registry_impl::GraphicTiming timing) const
{
    for (const building_type_registry_impl::SpawnPolicy &policy : group.policies) {
        if (policy.graphic_timing == timing && evaluate_condition(policy.condition)) {
            return 1;
        }
    }
    return 0;
}

unsigned char building_runtime::get_spawn_delay_counter(size_t policy_index) const
{
    if (policy_index == 0) {
        return building_->figure_spawn_delay;
    }
    if (spawn_delay_counters_.size() <= policy_index) {
        return 0;
    }
    return spawn_delay_counters_[policy_index];
}

void building_runtime::set_spawn_delay_counter(size_t policy_index, unsigned char value)
{
    if (policy_index == 0) {
        building_->figure_spawn_delay = value;
        return;
    }
    if (spawn_delay_counters_.size() <= policy_index) {
        spawn_delay_counters_.resize(policy_index + 1, 0);
    }
    spawn_delay_counters_[policy_index] = value;
}

void building_runtime::assign_figure_slot(building_type_registry_impl::FigureSlot slot, unsigned int figure_id)
{
    switch (slot) {
        case building_type_registry_impl::FigureSlot::Primary:
            building_->figure_id = figure_id;
            break;
        case building_type_registry_impl::FigureSlot::Secondary:
            building_->figure_id2 = figure_id;
            break;
        case building_type_registry_impl::FigureSlot::Quaternary:
            building_->figure_id4 = figure_id;
            break;
        case building_type_registry_impl::FigureSlot::None:
        default:
            break;
    }
}

int building_runtime::create_spawned_figure(const building_type_registry_impl::SpawnPolicy &policy, const map_point &road)
{
    int spawned_any = 0;
    int spawn_count = policy.spawn_count > 0 ? policy.spawn_count : 1;
    for (int i = 0; i < spawn_count; i++) {
        figure *spawned = figure_create(policy.spawn_figure, road.x, road.y, static_cast<direction_type>(policy.spawn_direction));
        if (!spawned) {
            continue;
        }
        spawned->action_state = policy.action_state;
        spawned->building_id = building_->id;
        // A multi-spawn policy still only owns one legacy tracked slot today; later spawns remain untracked for now.
        if (!spawned_any) {
            assign_figure_slot(policy.figure_slot, spawned->id);
        }
        if (policy.init_roaming) {
            figure_movement_init_roaming(spawned);
        }
        spawned_any = 1;
    }
    return spawned_any;
}

int building_runtime::try_spawn_policy(const building_type_registry_impl::SpawnPolicy &policy, const map_point &road)
{
    if (!evaluate_condition(policy.condition)) {
        return 0;
    }

    if (policy.mark_problem_if_no_water && !building_->has_water_access) {
        building_->show_on_problem_overlay = 2;
    }

    if (policy.require_water_access && !building_->has_water_access) {
        return 0;
    }

    if (policy.graphic_timing == building_type_registry_impl::GraphicTiming::BeforeSuccessfulSpawn) {
        set_building_graphic();
    }
    return create_spawned_figure(policy, road);
}

void building_runtime::spawn_service_roamer_group(const building_type_registry_impl::SpawnDelayGroup &group, size_t group_index)
{
    check_labor_problem();
    if (should_apply_graphic_for_timing(group, building_type_registry_impl::GraphicTiming::OnSpawnEntry)) {
        set_building_graphic();
    }

    if (group.guard_timing == building_type_registry_impl::GuardTiming::BeforeRoadAccess &&
        !group.existing_figures.empty() && has_figure_of_any(group.existing_figures)) {
        return;
    }

    map_point road;
    if (!resolve_road_access(group.road_access_mode, &road)) {
        return;
    }

    if (definition_ && definition_->has_labor_policy()) {
        run_labor_phase(definition_->labor_policy(), road);
    }

    if (group.guard_timing == building_type_registry_impl::GuardTiming::AfterLaborSeeker &&
        !group.existing_figures.empty() && has_figure_of_any(group.existing_figures)) {
        return;
    }

    if (should_apply_graphic_for_timing(group, building_type_registry_impl::GraphicTiming::BeforeDelayCheck)) {
        set_building_graphic();
    }

    int spawn_delay = evaluate_delay(group.delay_bands);
    if (!spawn_delay) {
        return;
    }

    unsigned char delay_counter = get_spawn_delay_counter(group_index);
    delay_counter++;
    if (delay_counter <= spawn_delay) {
        set_spawn_delay_counter(group_index, delay_counter);
        return;
    }

    set_spawn_delay_counter(group_index, 0);
    for (const building_type_registry_impl::SpawnPolicy &policy : group.policies) {
        if (policy.mode != building_type_registry_impl::SpawnMode::ServiceRoamer) {
            continue;
        }
        if (try_spawn_policy(policy, road) && policy.block_on_success) {
            break;
        }
    }
}

void building_runtime::spawn_figure()
{
    if (!building_ || !definition_ || building_->state != BUILDING_STATE_IN_USE) {
        return;
    }

    refresh_runtime_state();

    const std::vector<building_type_registry_impl::SpawnDelayGroup> &spawn_groups = definition_->spawn_groups();
    // Groups own the shared delay/guard phase, then policies inside them can either cooperate or block one another.
    for (size_t i = 0; i < spawn_groups.size(); i++) {
        const building_type_registry_impl::SpawnDelayGroup &group = spawn_groups[i];
        if (!group.policies.empty() && group.policies.front().mode == building_type_registry_impl::SpawnMode::ServiceRoamer) {
            spawn_service_roamer_group(group, i);
        }
    }
}

int building_runtime::owns_graphics()
{
    if (!uses_new_graphics()) {
        return 0;
    }
    resolve_graphics_state();
    return owns_graphics_;
}

int building_runtime::owns_graphic_animation()
{
    if (!uses_new_graphics()) {
        return 0;
    }
    resolve_graphics_state();
    return owns_graphic_animation_;
}

int building_runtime::graphic_animation_layer_count()
{
    if (!uses_new_graphics()) {
        return 0;
    }
    resolve_graphics_state();
    return static_cast<int>(resolved_animation_frame_slices_.size());
}

const RuntimeDrawSlice *building_runtime::graphic_animation_layer_frame(int index)
{
    if (!uses_new_graphics()) {
        return nullptr;
    }
    resolve_graphics_state();
    if (index < 0 || static_cast<size_t>(index) >= resolved_animation_frame_slices_.size()) {
        return nullptr;
    }
    return &resolved_animation_frame_slices_[static_cast<size_t>(index)];
}

extern "C" void building_runtime_reset(void)
{
    building_runtime_impl::g_runtime_instances.clear();
    g_logged_building_graphics_fallbacks.clear();
}

extern "C" void building_runtime_apply_graphic(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        instance->set_building_graphic();
    }
}

extern "C" void building_runtime_spawn_figure(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        instance->spawn_figure();
    }
}

const RuntimeDrawSlice *building_runtime_get_graphic_footprint_slice(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->graphic_footprint();
    }
    return nullptr;
}

const RuntimeDrawSlice *building_runtime_get_graphic_top_slice(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->graphic_top();
    }
    return nullptr;
}

int building_runtime_owns_graphics(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->owns_graphics();
    }
    return 0;
}

int building_runtime_owns_graphic_animation(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->owns_graphic_animation();
    }
    return 0;
}

int building_runtime_get_graphic_animation_layer_count(building *b)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->graphic_animation_layer_count();
    }
    return 0;
}

const RuntimeDrawSlice *building_runtime_get_graphic_animation_layer_frame(building *b, int layer_index)
{
    if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
        return instance->graphic_animation_layer_frame(layer_index);
    }
    return nullptr;
}
