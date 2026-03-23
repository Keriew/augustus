#ifndef GAME_TICK_H
#define GAME_TICK_H

void game_tick_run(void);

void game_tick_cheat_year(void);

#ifdef ENABLE_MULTIPLAYER
#include <stdint.h>
int multiplayer_time_can_advance_tick(void);
void multiplayer_time_on_tick_advanced(void);
uint32_t multiplayer_time_get_authoritative_tick(void);
#endif

#endif // GAME_TICK_H
