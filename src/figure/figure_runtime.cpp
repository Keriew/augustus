#include "figure/figure_runtime_api.h"

#include "figure/figure_type_registry_internal.h"

extern "C" {
#include "building/building.h"
#include "core/image.h"
#include "figure/combat.h"
#include "figure/figure.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "map/building.h"
#include "map/road_access.h"
}

#include <memory>
#include <vector>

namespace {

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

        const figure_type_registry_impl::OwnerBinding &owner_binding = definition()->owner_binding();
        if (!owner_matches(owner, owner_binding) || !slot_matches(owner, owner_binding)) {
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

private:
    static int owner_matches(const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding)
    {
        if (!owner) {
            return 0;
        }

        switch (owner_binding.required_owner_state) {
            case figure_type_registry_impl::OwnerStateRequirement::Any:
                break;
            case figure_type_registry_impl::OwnerStateRequirement::InUse:
                if (owner->state != BUILDING_STATE_IN_USE) {
                    return 0;
                }
                break;
            case figure_type_registry_impl::OwnerStateRequirement::InUseOrMothballed:
                if (owner->state != BUILDING_STATE_IN_USE && owner->state != BUILDING_STATE_MOTHBALLED) {
                    return 0;
                }
                break;
        }

        if (owner_binding.required_building_type != BUILDING_ANY &&
            owner_binding.required_building_type != owner->type) {
            return 0;
        }
        return 1;
    }

    figure *slot_figure(const building *owner, figure_type_registry_impl::FigureSlot slot) const
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

    int slot_matches(const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding) const
    {
        if (owner_binding.slot == figure_type_registry_impl::FigureSlot::None) {
            return 1;
        }

        figure *tracked = slot_figure(owner, owner_binding.slot);
        figure *current = data_figure();
        return tracked && current && tracked->id == current->id && tracked->created_sequence == current->created_sequence;
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
