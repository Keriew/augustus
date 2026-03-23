#ifndef SCENARIO_EMPIRE_H
#define SCENARIO_EMPIRE_H

#define SCENARIO_CUSTOM_EMPIRE 99

int scenario_empire_id(void);

int scenario_empire_is_expanded(void);

void scenario_empire_process_expansion(void);

#ifdef ENABLE_MULTIPLAYER
int scenario_empire_is_multiplayer_mode(void);
void scenario_empire_multiplayer_init(void);
void scenario_empire_set_multiplayer_mode(int enabled);
#endif

#endif // SCENARIO_EMPIRE_H
