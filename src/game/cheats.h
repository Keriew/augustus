#ifndef GAME_CHEATS_H
#define GAME_CHEATS_H

#include <stdint.h>

#define MAX_COMMAND_SIZE 64

void game_cheat_activate(void);

int game_cheat_tooltip_enabled(void);

void game_cheat_money(void);

void game_cheat_victory(void);

/**
 * Does nothing, just set breakpoint in implementation and call it with alt + b
 */
void game_cheat_breakpoint(void);

void game_cheat_console(void);

void game_cheat_parse_command(uint8_t *command);

int game_cheat_extra_legions(void);

int game_cheat_disabled_legions_consumption(void);

int game_cheat_disabled_invasions(void);
#endif // GAME_CHEATS_H
