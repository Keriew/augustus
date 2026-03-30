extern "C" {
#include "tool_mode.h"
#include "city/view.h"
#include "core/direction.h"
}

#include <array>

namespace {

enum class ToolTargetBehavior {
    DragArea,
    SingleTarget
};

struct ToolModeDefinition {
    building_type resolved_type;
    key_modifier_type modifier;
    int footprint_size;
    ToolTargetBehavior target_behavior;
};

class ModeSwitchTool {
public:
    ModeSwitchTool(
        building_type selection_type,
        bool is_drag_tool,
        std::array<building_type, 3> compatibility_aliases,
        ToolModeDefinition default_mode,
        std::array<ToolModeDefinition, 2> modes)
        : selection_type_(selection_type),
          is_drag_tool_(is_drag_tool),
          compatibility_aliases_(compatibility_aliases),
          default_mode_(default_mode),
          modes_(modes)
    {
    }

    bool handles_requested_type(building_type requested_type) const
    {
        if (requested_type == selection_type_) {
            return true;
        }
        for (building_type alias : compatibility_aliases_) {
            if (alias == requested_type) {
                return true;
            }
        }
        return false;
    }

    bool handles_selection(building_type selection_type) const
    {
        return selection_type_ == selection_type;
    }

    building_type selection_type() const
    {
        return selection_type_;
    }

    bool is_drag_tool() const
    {
        return is_drag_tool_;
    }

    building_type resolve(building_type compatibility_alias_type, key_modifier_type modifiers) const
    {
        return resolve_definition(compatibility_alias_type, modifiers).resolved_type;
    }

    void resolve_drag_points(
        building_type compatibility_alias_type,
        key_modifier_type modifiers,
        int raw_start_x,
        int raw_start_y,
        int raw_end_x,
        int raw_end_y,
        int *start_x,
        int *start_y,
        int *end_x,
        int *end_y) const
    {
        const ToolModeDefinition &mode = resolve_definition(compatibility_alias_type, modifiers);
        if (mode.target_behavior == ToolTargetBehavior::SingleTarget) {
            *start_x = raw_end_x;
            *start_y = raw_end_y;
            *end_x = raw_end_x;
            *end_y = raw_end_y;
            return;
        }

        *start_x = raw_start_x;
        *start_y = raw_start_y;
        *end_x = raw_end_x;
        *end_y = raw_end_y;
        apply_footprint_offset(start_x, start_y, mode.footprint_size);
        apply_footprint_offset(end_x, end_y, mode.footprint_size);
    }

private:
    const ToolModeDefinition *find_definition_for_type(building_type type) const
    {
        if (default_mode_.resolved_type == type) {
            return &default_mode_;
        }
        for (const ToolModeDefinition &mode : modes_) {
            if (mode.resolved_type == type) {
                return &mode;
            }
        }
        return nullptr;
    }

    const ToolModeDefinition &resolve_definition(building_type compatibility_alias_type, key_modifier_type modifiers) const
    {
        for (const ToolModeDefinition &mode : modes_) {
            if ((modifiers & mode.modifier) == mode.modifier) {
                return mode;
            }
        }
        if (handles_requested_type(compatibility_alias_type)) {
            const ToolModeDefinition *definition = find_definition_for_type(compatibility_alias_type);
            if (definition) {
                return *definition;
            }
        }
        return default_mode_;
    }

    static void apply_footprint_offset(int *x, int *y, int footprint_size)
    {
        if (footprint_size <= 1) {
            return;
        }

        switch (city_view_orientation()) {
            default:
                break;
            case DIR_2_RIGHT:
                *x = *x - footprint_size + 1;
                break;
            case DIR_4_BOTTOM:
                *x = *x - footprint_size + 1;
                *y = *y - footprint_size + 1;
                break;
            case DIR_6_LEFT:
                *y = *y - footprint_size + 1;
                break;
        }
    }

    building_type selection_type_;
    bool is_drag_tool_;
    std::array<building_type, 3> compatibility_aliases_;
    ToolModeDefinition default_mode_;
    std::array<ToolModeDefinition, 2> modes_;
};

const ModeSwitchTool CLEAR_TOOL(
    BUILDING_CLEAR_LAND,
    true,
    { { BUILDING_CLEAR_LAND, BUILDING_REPAIR_LAND, BUILDING_CLEAR_TREES } },
    ToolModeDefinition{ BUILDING_CLEAR_LAND, KEY_MOD_NONE, 1, ToolTargetBehavior::DragArea },
    { {
        ToolModeDefinition{ BUILDING_REPAIR_LAND, KEY_MOD_CTRL, 1, ToolTargetBehavior::DragArea },
        ToolModeDefinition{ BUILDING_CLEAR_TREES, KEY_MOD_SHIFT, 1, ToolTargetBehavior::DragArea },
    } });

const ModeSwitchTool ROAD_TOOL(
    BUILDING_ROAD,
    true,
    { { BUILDING_ROAD, BUILDING_HIGHWAY, BUILDING_ROADBLOCK } },
    ToolModeDefinition{ BUILDING_ROAD, KEY_MOD_NONE, 1, ToolTargetBehavior::DragArea },
    { {
        ToolModeDefinition{ BUILDING_ROADBLOCK, KEY_MOD_CTRL, 1, ToolTargetBehavior::SingleTarget },
        ToolModeDefinition{ BUILDING_HIGHWAY, KEY_MOD_SHIFT, 2, ToolTargetBehavior::DragArea },
    } });

const std::array<const ModeSwitchTool *, 2> MODE_SWITCH_TOOLS = { { &CLEAR_TOOL, &ROAD_TOOL } };

const ModeSwitchTool *find_tool_for_requested_type(building_type requested_type)
{
    for (const ModeSwitchTool *tool : MODE_SWITCH_TOOLS) {
        if (tool->handles_requested_type(requested_type)) {
            return tool;
        }
    }
    return nullptr;
}

const ModeSwitchTool *find_tool_for_selection(building_type selection_type)
{
    for (const ModeSwitchTool *tool : MODE_SWITCH_TOOLS) {
        if (tool->handles_selection(selection_type)) {
            return tool;
        }
    }
    return nullptr;
}

} // namespace

extern "C" int building_tool_mode_handles_requested_type(building_type requested_type)
{
    return find_tool_for_requested_type(requested_type) != nullptr;
}

extern "C" int building_tool_mode_handles_selection(building_type selection_type)
{
    return find_tool_for_selection(selection_type) != nullptr;
}

extern "C" building_type building_tool_mode_selection_type(building_type requested_type)
{
    const ModeSwitchTool *tool = find_tool_for_requested_type(requested_type);
    if (!tool) {
        return requested_type;
    }
    return tool->selection_type();
}

extern "C" int building_tool_mode_is_drag_tool(building_type selection_type)
{
    const ModeSwitchTool *tool = find_tool_for_selection(selection_type);
    return tool && tool->is_drag_tool();
}

extern "C" building_type building_tool_mode_resolve(
    building_type selection_type,
    building_type compatibility_alias_type,
    key_modifier_type modifiers)
{
    const ModeSwitchTool *tool = find_tool_for_selection(selection_type);
    if (!tool) {
        return selection_type;
    }
    return tool->resolve(compatibility_alias_type, modifiers);
}

extern "C" void building_tool_mode_resolve_drag_points(
    building_type selection_type,
    building_type compatibility_alias_type,
    key_modifier_type modifiers,
    int raw_start_x,
    int raw_start_y,
    int raw_end_x,
    int raw_end_y,
    int *start_x,
    int *start_y,
    int *end_x,
    int *end_y)
{
    const ModeSwitchTool *tool = find_tool_for_selection(selection_type);
    if (!tool) {
        *start_x = raw_start_x;
        *start_y = raw_start_y;
        *end_x = raw_end_x;
        *end_y = raw_end_y;
        return;
    }
    tool->resolve_drag_points(
        compatibility_alias_type,
        modifiers,
        raw_start_x,
        raw_start_y,
        raw_end_x,
        raw_end_y,
        start_x,
        start_y,
        end_x,
        end_y);
}
