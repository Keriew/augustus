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

/** Called when an invasion starts */
void scenario_lua_hook_on_invasion_start(int type, int size);

/** Called when a figure dies in combat */
void scenario_lua_hook_on_figure_died(int type, int x, int y);

/** Called when a trade resource is sold to a warehouse */
void scenario_lua_hook_on_trade_completed(int route_id, int resource, int amount);

/** Called when a Caesar's request is fulfilled */
void scenario_lua_hook_on_request_fulfilled(int id);

/** Called when a Caesar's request is refused or ignored */
void scenario_lua_hook_on_request_failed(int id);

/** Called when a building catches fire */
void scenario_lua_hook_on_building_fire(int building_id, int type);

/** Called when people immigrate or emigrate. direction: 1=in, -1=out */
void scenario_lua_hook_on_migration(int count, int direction);

/** Called when a city rating changes. type: 0=favor, 1=peace, 2=prosperity */
void scenario_lua_hook_on_rating_changed(int type, int old_value, int new_value);

/** Called when a god becomes angry (wrath bolts increase) */
void scenario_lua_hook_on_god_angry(int god_id);

/** Called when combat damage is dealt */
void scenario_lua_hook_on_combat(int attacker_type, int defender_type);

#endif // SCENARIO_LUA_HOOKS_H
