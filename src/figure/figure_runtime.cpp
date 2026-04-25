#include "figure/figure_runtime_api.h"

#include "core/crash_context.h"
#include "figure/figure_type_registry_internal.h"
#include "map/road_service_history.h"

extern "C" {
#include "building/list.h"
#include "building/maintenance.h"
#include "building/building.h"
#include "city/figures.h"
#include "core/calc.h"
#include "core/image.h"
#include "figure/action.h"
#include "figure/combat.h"
#include "figure/enemy_army.h"
#include "figure/figure.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/time.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/road_access.h"
#include "map/terrain.h"
#include "sound/effect.h"
}

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace {

constexpr int kInfiniteDistance = 10000;
constexpr int kRecalculateEnemyLocationTicks = 30;

class NativeFigure {
public:
    NativeFigure(figure *data, const figure_type_registry_impl::FigureTypeDefinition *definition)
        : figure_(data)
        , definition_(definition)
    {
    }

    virtual ~NativeFigure() = default;

    void set_figure(figure *data)
    {
        figure_ = data;
    }

    const figure_type_registry_impl::FigureTypeDefinition *definition() const
    {
        return definition_;
    }

    virtual int execute() = 0;

protected:
    figure *data_figure() const
    {
        return figure_;
    }

private:
    figure *figure_ = nullptr;
    const figure_type_registry_impl::FigureTypeDefinition *definition_ = nullptr;
};

bool owner_state_matches(const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding)
{
    if (!owner) {
        return false;
    }

    switch (owner_binding.required_owner_state) {
        case figure_type_registry_impl::OwnerStateRequirement::Any:
            break;
        case figure_type_registry_impl::OwnerStateRequirement::InUse:
            if (owner->state != BUILDING_STATE_IN_USE) {
                return false;
            }
            break;
        case figure_type_registry_impl::OwnerStateRequirement::InUseOrMothballed:
            if (owner->state != BUILDING_STATE_IN_USE && owner->state != BUILDING_STATE_MOTHBALLED) {
                return false;
            }
            break;
    }

    if (owner_binding.required_building_type != BUILDING_ANY &&
        owner_binding.required_building_type != owner->type) {
        return false;
    }
    return true;
}

figure *slot_figure(const building *owner, figure_type_registry_impl::FigureSlot slot)
{
    if (!owner) {
        return nullptr;
    }

    unsigned int figure_id = 0;
    switch (slot) {
        case figure_type_registry_impl::FigureSlot::Primary:
            figure_id = owner->figure_id;
            break;
        case figure_type_registry_impl::FigureSlot::Secondary:
            figure_id = owner->figure_id2;
            break;
        case figure_type_registry_impl::FigureSlot::Quaternary:
            figure_id = owner->figure_id4;
            break;
        case figure_type_registry_impl::FigureSlot::None:
        default:
            return nullptr;
    }
    return figure_id ? figure_get(figure_id) : nullptr;
}

bool slot_matches(const figure *f, const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding)
{
    if (owner_binding.slot == figure_type_registry_impl::FigureSlot::None) {
        return true;
    }

    figure *tracked = slot_figure(owner, owner_binding.slot);
    return tracked && f && tracked->id == f->id && tracked->created_sequence == f->created_sequence;
}

bool owner_binding_matches(const figure *f, const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding)
{
    return owner_state_matches(owner, owner_binding) && slot_matches(f, owner, owner_binding);
}

void retire_unsupported_native_state(figure *f, const char *native_class)
{
    const ErrorContextScope scope("Native FigureType action", native_class);
    error_context_report_warning(
        "Native FigureType walker reached an unsupported action state.",
        "The legacy fallback for this walker has been retired; the invalid figure will be removed.");
    if (f) {
        f->state = FIGURE_STATE_DEAD;
    }
}

road_service_effect religion_service_effect_for_owner(const building *owner)
{
    if (!owner) {
        return ROAD_SERVICE_EFFECT_NONE;
    }

    switch (owner->type) {
        case BUILDING_SMALL_TEMPLE_CERES:
        case BUILDING_LARGE_TEMPLE_CERES:
        case BUILDING_GRAND_TEMPLE_CERES:
            return ROAD_SERVICE_EFFECT_RELIGION_CERES;
        case BUILDING_SMALL_TEMPLE_NEPTUNE:
        case BUILDING_LARGE_TEMPLE_NEPTUNE:
        case BUILDING_GRAND_TEMPLE_NEPTUNE:
            return ROAD_SERVICE_EFFECT_RELIGION_NEPTUNE;
        case BUILDING_SMALL_TEMPLE_MERCURY:
        case BUILDING_LARGE_TEMPLE_MERCURY:
        case BUILDING_GRAND_TEMPLE_MERCURY:
            return ROAD_SERVICE_EFFECT_RELIGION_MERCURY;
        case BUILDING_SMALL_TEMPLE_MARS:
        case BUILDING_LARGE_TEMPLE_MARS:
        case BUILDING_GRAND_TEMPLE_MARS:
            return ROAD_SERVICE_EFFECT_RELIGION_MARS;
        case BUILDING_SMALL_TEMPLE_VENUS:
        case BUILDING_LARGE_TEMPLE_VENUS:
        case BUILDING_GRAND_TEMPLE_VENUS:
            return ROAD_SERVICE_EFFECT_RELIGION_VENUS;
        case BUILDING_PANTHEON:
            return ROAD_SERVICE_EFFECT_RELIGION_PANTHEON;
        default:
            return ROAD_SERVICE_EFFECT_NONE;
    }
}

road_service_effect primary_service_effect_for_pathing(
    const figure_type_registry_impl::PathingPolicy &pathing,
    const figure *f)
{
    if (!pathing.effect_from_religion_owner) {
        return pathing.effect;
    }
    return religion_service_effect_for_owner(f ? building_get(f->building_id) : nullptr);
}

void record_religion_owner_service_effects(const figure *f)
{
    const building *owner = f ? building_get(f->building_id) : nullptr;
    const road_service_effect effect = religion_service_effect_for_owner(owner);
    if (effect == ROAD_SERVICE_EFFECT_NONE) {
        return;
    }

    if (effect == ROAD_SERVICE_EFFECT_RELIGION_PANTHEON) {
        map_road_service_history_record(ROAD_SERVICE_EFFECT_RELIGION_CERES, f->grid_offset);
        map_road_service_history_record(ROAD_SERVICE_EFFECT_RELIGION_NEPTUNE, f->grid_offset);
        map_road_service_history_record(ROAD_SERVICE_EFFECT_RELIGION_MERCURY, f->grid_offset);
        map_road_service_history_record(ROAD_SERVICE_EFFECT_RELIGION_MARS, f->grid_offset);
        map_road_service_history_record(ROAD_SERVICE_EFFECT_RELIGION_VENUS, f->grid_offset);
    }
    map_road_service_history_record(effect, f->grid_offset);
}

class RoamingServiceFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        building *owner = building_get(f->building_id);
        if (!owner || !owner->id) {
            f->state = FIGURE_STATE_DEAD;
            return 1;
        }

        if (f->type == FIGURE_PRIEST && f->destination_building_id) {
            return 0;
        }

        if (!owner_binding_matches(f, owner, definition()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = definition()->movement_profile();
        const figure_type_registry_impl::GraphicsPolicy &graphics = definition()->graphics_policy();

        f->terrain_usage = static_cast<unsigned char>(movement.terrain_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, graphics.max_image_offset);

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_125_ROAMING:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    if (movement.return_mode == figure_type_registry_impl::ReturnMode::DieAtLimit) {
                        f->state = FIGURE_STATE_DEAD;
                    } else {
                        int x_road = 0;
                        int y_road = 0;
                        if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                            f->action_state = FIGURE_ACTION_126_ROAMER_RETURNING;
                            f->destination_x = x_road;
                            f->destination_y = y_road;
                            figure_route_remove(f);
                            f->roam_length = 0;
                        } else {
                            f->state = FIGURE_STATE_DEAD;
                        }
                    }
                }
                if (f->state == FIGURE_STATE_ALIVE) {
                    figure_movement_roam_ticks(f, movement.roam_ticks);
                }
                break;
            case FIGURE_ACTION_126_ROAMER_RETURNING:
                if (movement.return_mode != figure_type_registry_impl::ReturnMode::ReturnToOwnerRoad) {
                    return 0;
                }
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                    f->direction == DIR_FIGURE_REROUTE ||
                    f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                return 0;
        }

        figure_image_update(f, image_group(graphics.image_group));
        return 1;
    }
};

class EngineerServiceFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        building *owner = building_get(f->building_id);
        if (!owner_binding_matches(f, owner, definition()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = definition()->movement_profile();
        f->terrain_usage = static_cast<unsigned char>(movement.terrain_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics_policy().max_image_offset);

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_60_ENGINEER_CREATED:
                f->is_ghost = 1;
                f->image_offset = 0;
                f->wait_ticks--;
                if (f->wait_ticks <= 0) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_61_ENGINEER_ENTERING_EXITING;
                        figure_movement_set_cross_country_destination(f, x_road, y_road);
                        f->roam_length = 0;
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                break;
            case FIGURE_ACTION_61_ENGINEER_ENTERING_EXITING:
                f->use_cross_country = 1;
                f->is_ghost = 1;
                if (figure_movement_move_ticks_cross_country(f, movement.roam_ticks) == 1) {
                    if (map_building_at(f->grid_offset) == f->building_id) {
                        f->state = FIGURE_STATE_DEAD;
                    } else {
                        f->action_state = FIGURE_ACTION_62_ENGINEER_ROAMING;
                        figure_movement_init_roaming(f);
                        f->roam_length = 0;
                    }
                }
                break;
            case FIGURE_ACTION_62_ENGINEER_ROAMING:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_63_ENGINEER_RETURNING;
                        f->destination_x = x_road;
                        f->destination_y = y_road;
                        figure_route_remove(f);
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                if (f->state == FIGURE_STATE_ALIVE) {
                    figure_movement_roam_ticks(f, movement.roam_ticks);
                }
                break;
            case FIGURE_ACTION_63_ENGINEER_RETURNING:
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    f->action_state = FIGURE_ACTION_61_ENGINEER_ENTERING_EXITING;
                    figure_movement_set_cross_country_destination(f, owner->x, owner->y);
                    f->roam_length = 0;
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                retire_unsupported_native_state(f, "engineer_service");
                break;
        }

        update_image(f);
        return 1;
    }

private:
    void update_image(figure *f) const
    {
        figure_image_update(f, image_group(definition()->graphics_policy().image_group));
    }
};

class PrefectServiceFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        building *owner = building_get(f->building_id);
        if (!owner_binding_matches(f, owner, definition()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = definition()->movement_profile();
        f->terrain_usage = static_cast<unsigned char>(movement.terrain_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics_policy().max_image_offset);

        if (!fight_enemy(f)) {
            fight_fire(f, false);
        }

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_70_PREFECT_CREATED:
                f->is_ghost = 1;
                f->image_offset = 0;
                f->wait_ticks--;
                if (f->wait_ticks <= 0) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_71_PREFECT_ENTERING_EXITING;
                        figure_movement_set_cross_country_destination(f, x_road, y_road);
                        f->roam_length = 0;
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                break;
            case FIGURE_ACTION_71_PREFECT_ENTERING_EXITING:
                f->use_cross_country = 1;
                f->is_ghost = 1;
                if (figure_movement_move_ticks_cross_country(f, movement.roam_ticks) == 1) {
                    if (map_building_at(f->grid_offset) == f->building_id) {
                        f->state = FIGURE_STATE_DEAD;
                    } else {
                        f->action_state = FIGURE_ACTION_72_PREFECT_ROAMING;
                        figure_movement_init_roaming(f);
                        f->roam_length = 0;
                    }
                }
                break;
            case FIGURE_ACTION_72_PREFECT_ROAMING:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_73_PREFECT_RETURNING;
                        f->destination_x = x_road;
                        f->destination_y = y_road;
                        figure_route_remove(f);
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                if (f->state == FIGURE_STATE_ALIVE) {
                    figure_movement_roam_ticks(f, movement.roam_ticks);
                }
                break;
            case FIGURE_ACTION_73_PREFECT_RETURNING:
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    f->action_state = FIGURE_ACTION_71_PREFECT_ENTERING_EXITING;
                    figure_movement_set_cross_country_destination(f, owner->x, owner->y);
                    f->roam_length = 0;
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
                f->terrain_usage = TERRAIN_USAGE_ANY;
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    f->action_state = FIGURE_ACTION_75_PREFECT_AT_FIRE;
                    figure_route_remove(f);
                    f->roam_length = 0;
                    f->wait_ticks = game_time_scale_legacy_day_ticks(50);
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS && !fight_fire(f, true) && !in_combat(f)) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_73_PREFECT_RETURNING;
                        f->destination_x = x_road;
                        f->destination_y = y_road;
                        f->wait_ticks = 0;
                        figure_route_remove(f);
                    }
                }
                break;
            case FIGURE_ACTION_75_PREFECT_AT_FIRE:
                extinguish_fire(f);
                break;
            case FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY:
                f->terrain_usage = TERRAIN_USAGE_ANY;
                if (!figure_target_is_alive(f) && !fight_enemy(f)) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_73_PREFECT_RETURNING;
                        f->destination_x = x_road;
                        f->destination_y = y_road;
                        figure_route_remove(f);
                        f->roam_length = 0;
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                figure_movement_move_ticks_with_percentage(f, movement.roam_ticks, 20);
                if (f->direction == DIR_FIGURE_AT_DESTINATION || f->wait_ticks++ > kRecalculateEnemyLocationTicks) {
                    figure *target = figure_get(f->target_figure_id);
                    f->destination_x = target->x;
                    f->destination_y = target->y;
                    f->wait_ticks = 0;
                    figure_route_remove(f);
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                retire_unsupported_native_state(f, "prefect_service");
                break;
        }

        update_image(f);
        return 1;
    }

private:
    static int get_enemy_distance(figure *f, int x, int y)
    {
        if (f->type == FIGURE_RIOTER || f->type == FIGURE_ENEMY54_GLADIATOR) {
            return calc_maximum_distance(x, y, f->x, f->y);
        } else if (f->type == FIGURE_CRIMINAL_LOOTER || f->type == FIGURE_CRIMINAL_ROBBER) {
            return 3 * calc_maximum_distance(x, y, f->x, f->y);
        } else if (f->type == FIGURE_INDIGENOUS_NATIVE && f->action_state == FIGURE_ACTION_159_NATIVE_ATTACKING) {
            return calc_maximum_distance(x, y, f->x, f->y);
        } else if (figure_is_enemy(f)) {
            return 3 * calc_maximum_distance(x, y, f->x, f->y);
        } else if (f->type == FIGURE_WOLF) {
            return 4 * calc_maximum_distance(x, y, f->x, f->y);
        }
        return kInfiniteDistance;
    }

    static int get_nearest_enemy(int x, int y, int *distance)
    {
        int min_enemy_id = 0;
        int min_dist = kInfiniteDistance;
        for (unsigned int i = 1; i < figure_count(); i++) {
            figure *f = figure_get(i);
            if (figure_is_dead(f)) {
                continue;
            }
            int dist = get_enemy_distance(f, x, y);
            if (dist != kInfiniteDistance && f->targeted_by_figure_id) {
                figure *pursuiter = figure_get(f->targeted_by_figure_id);
                if (get_enemy_distance(f, pursuiter->x, pursuiter->y) < dist * 2) {
                    continue;
                }
            }
            if (dist < min_dist) {
                min_dist = dist;
                min_enemy_id = i;
            }
        }
        *distance = min_dist;
        return min_enemy_id;
    }

    static bool fight_enemy(figure *f)
    {
        if (!city_figures_has_security_breach() && enemy_army_total_enemy_formations() <= 0) {
            return false;
        }
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
            case FIGURE_ACTION_149_CORPSE:
            case FIGURE_ACTION_70_PREFECT_CREATED:
            case FIGURE_ACTION_71_PREFECT_ENTERING_EXITING:
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
            case FIGURE_ACTION_75_PREFECT_AT_FIRE:
            case FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY:
            case FIGURE_ACTION_77_PREFECT_AT_ENEMY:
                return false;
        }
        f->wait_ticks_next_target++;
        if (f->wait_ticks_next_target < 10) {
            return false;
        }
        int distance = 0;
        int enemy_id = get_nearest_enemy(f->x, f->y, &distance);
        if (enemy_id > 0 && distance <= 30) {
            figure *enemy = figure_get(enemy_id);
            if (enemy->targeted_by_figure_id) {
                figure_get(enemy->targeted_by_figure_id)->target_figure_id = 0;
            }
            f->wait_ticks = 0;
            f->action_state = FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY;
            f->destination_x = enemy->x;
            f->destination_y = enemy->y;
            f->target_figure_id = enemy_id;
            enemy->targeted_by_figure_id = f->id;
            f->target_figure_created_sequence = enemy->created_sequence;
            figure_route_remove(f);
            return true;
        }
        f->wait_ticks_next_target = 0;
        return false;
    }

    static bool fight_fire(figure *f, bool force)
    {
        if (building_list_burning_size() <= 0) {
            return false;
        }
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
            case FIGURE_ACTION_149_CORPSE:
            case FIGURE_ACTION_70_PREFECT_CREATED:
            case FIGURE_ACTION_71_PREFECT_ENTERING_EXITING:
            case FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY:
            case FIGURE_ACTION_77_PREFECT_AT_ENEMY:
                return false;
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
            case FIGURE_ACTION_75_PREFECT_AT_FIRE: {
                if (!force) {
                    return false;
                }
                building *burn = building_get(f->destination_building_id);
                if ((burn->state == BUILDING_STATE_IN_USE || burn->state == BUILDING_STATE_MOTHBALLED) &&
                    burn->type == BUILDING_BURNING_RUIN) {
                    return true;
                }
                break;
            }
        }
        f->wait_ticks_missile++;
        if (f->wait_ticks_missile < 20 && !force) {
            return false;
        }
        int distance = 0;
        int ruin_id = building_maintenance_get_closest_burning_ruin(f->x, f->y, &distance);
        if (ruin_id > 0 && distance <= 25) {
            building *ruin = building_get(ruin_id);
            f->wait_ticks_missile = 0;
            f->action_state = FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE;
            f->wait_ticks = 0;
            f->destination_x = ruin->road_access_x;
            f->destination_y = ruin->road_access_y;
            f->destination_building_id = ruin_id;
            figure_route_remove(f);
            ruin->figure_id4 = f->id;
            return true;
        }
        return false;
    }

    static void extinguish_fire(figure *f)
    {
        building *burn = building_get(f->destination_building_id);
        int distance = calc_maximum_distance(f->x, f->y, burn->x, burn->y);
        if ((burn->state == BUILDING_STATE_IN_USE || burn->state == BUILDING_STATE_MOTHBALLED)
            && burn->type == BUILDING_BURNING_RUIN && distance < 2) {
            burn->fire_duration = 32;
            sound_effect_play(SOUND_EFFECT_FIRE_SPLASH);
        } else {
            f->wait_ticks = 1;
        }
        f->attack_direction = calc_general_direction(f->x, f->y, burn->x, burn->y);
        if (f->attack_direction >= 8) {
            f->attack_direction = 0;
        }
        f->wait_ticks--;
        if (f->wait_ticks <= 0) {
            if (!fight_fire(f, true)) {
                building *owner = building_get(f->building_id);
                int x_road = 0;
                int y_road = 0;
                if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                    f->action_state = FIGURE_ACTION_73_PREFECT_RETURNING;
                    f->destination_x = x_road;
                    f->destination_y = y_road;
                    figure_route_remove(f);
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
        }
    }

    static bool in_combat(figure *f)
    {
        return f->action_state == FIGURE_ACTION_150_ATTACK || f->action_state == FIGURE_ACTION_149_CORPSE;
    }

    void update_image(figure *f) const
    {
        int dir = 0;
        if (f->action_state == FIGURE_ACTION_75_PREFECT_AT_FIRE ||
            f->action_state == FIGURE_ACTION_150_ATTACK) {
            dir = f->attack_direction;
        } else if (f->direction < 8) {
            dir = f->direction;
        } else {
            dir = f->previous_tile_direction;
        }
        dir = figure_image_normalize_direction(dir);

        switch (f->action_state) {
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
                f->image_id = image_group(GROUP_FIGURE_PREFECT_WITH_BUCKET) +
                    dir + 8 * f->image_offset;
                break;
            case FIGURE_ACTION_75_PREFECT_AT_FIRE:
                f->image_id = image_group(GROUP_FIGURE_PREFECT_WITH_BUCKET) +
                    dir + 96 + 8 * (f->image_offset / 2);
                break;
            case FIGURE_ACTION_150_ATTACK:
                if (f->attack_image_offset >= 12) {
                    f->image_id = image_group(definition()->graphics_policy().image_group) +
                        104 + dir + 8 * ((f->attack_image_offset - 12) / 2);
                } else {
                    f->image_id = image_group(definition()->graphics_policy().image_group) + 104 + dir;
                }
                break;
            case FIGURE_ACTION_149_CORPSE:
                f->image_id = image_group(definition()->graphics_policy().image_group) +
                    96 + figure_image_corpse_offset(f);
                break;
            default:
                f->image_id = image_group(definition()->graphics_policy().image_group) +
                    dir + 8 * f->image_offset;
                break;
        }
    }
};

struct RuntimeEntry {
    figure *data = nullptr;
    unsigned short created_sequence = 0;
    const figure_type_registry_impl::FigureTypeDefinition *definition = nullptr;
    std::unique_ptr<NativeFigure> controller;
};

std::vector<RuntimeEntry> g_runtime_entries;

RuntimeEntry *entry_for_id(unsigned int id)
{
    if (!id) {
        return nullptr;
    }
    if (g_runtime_entries.size() <= id) {
        g_runtime_entries.resize(id + 1);
    }
    return &g_runtime_entries[id];
}

void clear_entry(unsigned int id)
{
    if (!id || g_runtime_entries.size() <= id) {
        return;
    }
    g_runtime_entries[id] = RuntimeEntry();
}

int entry_matches_figure(const RuntimeEntry &entry, const figure *f)
{
    return entry.data == f && entry.created_sequence == f->created_sequence;
}

std::unique_ptr<NativeFigure> make_controller(
    figure *f,
    const figure_type_registry_impl::FigureTypeDefinition *definition)
{
    if (!f || !definition) {
        return nullptr;
    }

    switch (definition->native_class()) {
        case figure_type_registry_impl::NativeClassId::RoamingService:
            return std::make_unique<RoamingServiceFigure>(f, definition);
        case figure_type_registry_impl::NativeClassId::EngineerService:
            return std::make_unique<EngineerServiceFigure>(f, definition);
        case figure_type_registry_impl::NativeClassId::PrefectService:
            return std::make_unique<PrefectServiceFigure>(f, definition);
        case figure_type_registry_impl::NativeClassId::None:
        default:
            return nullptr;
    }
}

RuntimeEntry *bind_entry(figure *f)
{
    if (!f || !f->id) {
        return nullptr;
    }

    RuntimeEntry *entry = entry_for_id(f->id);
    if (!entry) {
        return nullptr;
    }

    const figure_type_registry_impl::FigureTypeDefinition *definition =
        figure_type_registry_impl::definition_for(static_cast<figure_type>(f->type));
    if (!definition) {
        *entry = RuntimeEntry();
        return nullptr;
    }

    if (!entry_matches_figure(*entry, f) || entry->definition != definition || !entry->controller) {
        entry->data = f;
        entry->created_sequence = f->created_sequence;
        entry->definition = definition;
        entry->controller = make_controller(f, definition);
    } else {
        entry->data = f;
        entry->controller->set_figure(f);
    }

    if (!entry->controller) {
        *entry = RuntimeEntry();
        return nullptr;
    }
    return entry;
}

} // namespace

extern "C" void figure_runtime_reset(void)
{
    g_runtime_entries.clear();
}

extern "C" void figure_runtime_initialize_city(void)
{
    figure_runtime_reset();

    for (unsigned int i = 1; i < figure_count(); i++) {
        figure *f = figure_get(i);
        if (!f || !f->state) {
            continue;
        }
        bind_entry(f);
    }
}

extern "C" void figure_runtime_on_created(figure *f)
{
    if (!f || !f->id) {
        return;
    }

    RuntimeEntry *entry = entry_for_id(f->id);
    if (!entry) {
        return;
    }
    *entry = RuntimeEntry();
    bind_entry(f);
}

extern "C" void figure_runtime_on_deleted(figure *f)
{
    if (!f) {
        return;
    }
    clear_entry(f->id);
}

extern "C" int figure_runtime_execute(figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    if (!entry || !entry->controller) {
        return 0;
    }
    return entry->controller->execute();
}

extern "C" int figure_runtime_choose_roaming_direction(
    figure *f,
    const int *road_tiles,
    int came_from_direction,
    int vanilla_direction)
{
    // Keep vanilla ordering as the tie-breaker. Smart service pathing only
    // changes real intersections where more than one forward road is available.
    if (!f || !road_tiles || vanilla_direction < 0 || vanilla_direction >= 8) {
        return vanilla_direction;
    }

    RuntimeEntry *entry = bind_entry(f);
    if (!entry || !entry->definition) {
        return vanilla_direction;
    }

    const figure_type_registry_impl::PathingPolicy &pathing = entry->definition->pathing_policy();
    const road_service_effect effect = primary_service_effect_for_pathing(pathing, f);
    if (pathing.mode != figure_type_registry_impl::PathingMode::SmartService ||
        effect == ROAD_SERVICE_EFFECT_NONE) {
        return vanilla_direction;
    }

    int candidate_count = 0;
    uint32_t best_stamp = std::numeric_limits<uint32_t>::max();
    int best_direction = vanilla_direction;
    for (int direction = 0; direction < 8; direction += 2) {
        if (!road_tiles[direction] || direction == came_from_direction) {
            continue;
        }

        candidate_count++;
        const int target_grid_offset = f->grid_offset + map_grid_direction_delta(direction);
        const uint32_t stamp = map_road_service_history_get(effect, target_grid_offset);
        if (stamp < best_stamp) {
            best_stamp = stamp;
            best_direction = direction;
        } else if (stamp == best_stamp && direction == vanilla_direction) {
            best_direction = direction;
        }
    }

    return candidate_count > 1 ? best_direction : vanilla_direction;
}

extern "C" void figure_runtime_record_road_service_visit(figure *f)
{
    // Recording is pathing telemetry only. Coverage/risk systems still use the
    // existing service callbacks; this history only informs future road choices.
    if (!f || !map_terrain_is(f->grid_offset, TERRAIN_ROAD)) {
        return;
    }

    RuntimeEntry *entry = bind_entry(f);
    if (!entry || !entry->definition) {
        return;
    }

    const figure_type_registry_impl::PathingPolicy &pathing = entry->definition->pathing_policy();
    const road_service_effect effect = primary_service_effect_for_pathing(pathing, f);
    if (pathing.mode != figure_type_registry_impl::PathingMode::SmartService ||
        effect == ROAD_SERVICE_EFFECT_NONE) {
        return;
    }

    if (pathing.effect_from_religion_owner) {
        record_religion_owner_service_effects(f);
    } else {
        map_road_service_history_record(effect, f->grid_offset);
    }
}
