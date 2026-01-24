#ifndef EMPIRE_EDITOR_H
#define EMPIRE_EDITOR_H

#include "input/mouse.h"
#include "input/hotkey.h"

typedef enum {
    EMPIRE_TOOL_MIN = 0,
    EMPIRE_TOOL_OUR_CITY = 0,
    EMPIRE_TOOL_TRADE_CITY = 1,
    EMPIRE_TOOL_ROMAN_CITY = 2,
    EMPIRE_TOOL_VULNERABLE_CITY = 3,
    EMPIRE_TOOL_FUTURE_TRADE_CITY = 4,
    EMPIRE_TOOL_DISTANT_CITY = 5,
    EMPIRE_TOOL_BORDER = 6,
    EMPIRE_TOOL_BATTLE = 7,
    EMPIRE_TOOL_DISTANT_BABARIAN = 8,
    EMPIRE_TOOL_DISTANT_LEGION = 9,
    EMPIRE_TOOL_MAX = 10
} empire_tool;

int empire_editor_get_tool(void);
void empire_editor_set_tool(empire_tool value);
void empire_editor_change_tool(int amount);

int empire_editor_delete_object(int obj_id);

int empire_editor_handle_placement(const mouse *m, const hotkeys *h);

#endif // EMPIRE_EDITOR_H
