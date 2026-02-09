#ifndef SCENARIO_LUA_HOOKS_H
#define SCENARIO_LUA_HOOKS_H

/** Called after script loads, at scenario start */
void scenario_lua_hook_on_load(void);

/** Called every game day (from advance_day in tick.c) */
void scenario_lua_hook_on_tick(void);

/** Called at start of each month (from advance_month in tick.c) */
void scenario_lua_hook_on_month(void);

/** Called at start of each year (from advance_year in tick.c) */
void scenario_lua_hook_on_year(void);

/** Called when a named event fires (from scenario event system) */
void scenario_lua_hook_on_event(const char *event_name);

/** Called when a building is placed by the player */
void scenario_lua_hook_on_building_placed(int type, int x, int y, int building_id);

/** Called when a building is destroyed */
void scenario_lua_hook_on_building_destroyed(int type, int x, int y, int building_id);

/** Called on scenario victory */
void scenario_lua_hook_on_victory(void);

/** Called on scenario defeat */
void scenario_lua_hook_on_defeat(void);

#endif // SCENARIO_LUA_HOOKS_H
