#ifndef BUILDING_TOOL_MODE_H
#define BUILDING_TOOL_MODE_H

#include "building/type.h"
#include "input/keys.h"

#ifdef __cplusplus
extern "C" {
#endif

int building_tool_mode_handles_requested_type(building_type requested_type);

int building_tool_mode_handles_selection(building_type selection_type);

building_type building_tool_mode_selection_type(building_type requested_type);

building_type building_tool_mode_resolve(
    building_type selection_type,
    building_type compatibility_alias_type,
    key_modifier_type modifiers);

#ifdef __cplusplus
}
#endif

#endif // BUILDING_TOOL_MODE_H
