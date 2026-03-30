extern "C" {
#include "tool_mode.h"
}

#include <array>

namespace {

struct ToolModeDefinition {
    building_type resolved_type;
    key_modifier_type modifier;
};

class ModeSwitchTool {
public:
    ModeSwitchTool(
        building_type selection_type,
        std::array<building_type, 2> compatibility_aliases,
        std::array<ToolModeDefinition, 2> modes,
        building_type default_mode)
        : selection_type_(selection_type),
          compatibility_aliases_(compatibility_aliases),
          modes_(modes),
          default_mode_(default_mode)
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

    building_type resolve(building_type compatibility_alias_type, key_modifier_type modifiers) const
    {
        for (const ToolModeDefinition &mode : modes_) {
            if ((modifiers & mode.modifier) == mode.modifier) {
                return mode.resolved_type;
            }
        }
        if (handles_requested_type(compatibility_alias_type)) {
            return compatibility_alias_type;
        }
        return default_mode_;
    }

private:
    building_type selection_type_;
    std::array<building_type, 2> compatibility_aliases_;
    std::array<ToolModeDefinition, 2> modes_;
    building_type default_mode_;
};

const ModeSwitchTool CLEAR_TOOL(
    BUILDING_CLEAR_LAND,
    { { BUILDING_CLEAR_LAND, BUILDING_REPAIR_LAND } },
    { {
        ToolModeDefinition{ BUILDING_REPAIR_LAND, KEY_MOD_CTRL },
        ToolModeDefinition{ BUILDING_CLEAR_TREES, KEY_MOD_SHIFT },
    } },
    BUILDING_CLEAR_LAND);

const ModeSwitchTool *find_tool_for_requested_type(building_type requested_type)
{
    if (CLEAR_TOOL.handles_requested_type(requested_type)) {
        return &CLEAR_TOOL;
    }
    return nullptr;
}

const ModeSwitchTool *find_tool_for_selection(building_type selection_type)
{
    if (CLEAR_TOOL.handles_selection(selection_type)) {
        return &CLEAR_TOOL;
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
