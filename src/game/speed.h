#ifndef GAME_SPEED_H
#define GAME_SPEED_H

#ifdef __cplusplus
extern "C" {
#endif

#define TOTAL_GAME_SPEEDS 13
int game_speed_get_index(int speed);
int game_speed_get_speed(int index);
int game_speed_get_elapsed_ticks(void);

#ifdef __cplusplus
}
#endif

#endif // GAME_SPEED_H
