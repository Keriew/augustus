#include "editor.h"



static struct {
    empire_tool current_tool;
} data = {
    .current_tool = EMPIRE_TOOL_OUR_CITY
};

static void update_tool_bounds(void)
{
    if (data.current_tool > EMPIRE_TOOL_MAX) {
        data.current_tool = data.current_tool % (EMPIRE_TOOL_MAX + 1);
    }
}

int empire_editor_get_tool(void)
{
    return data.current_tool;
}

void empire_editor_set_tool(empire_tool tool)
{
    data.current_tool = tool;
}

void empire_editor_change_tool(int amount)
{
    data.current_tool += amount;
    update_tool_bounds();
}
