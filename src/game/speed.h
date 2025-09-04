#ifndef GAME_SPEED_H
#define GAME_SPEED_H

#define TOTAL_GAME_SPEEDS 13  
int game_speed_get_index(int speed);
int game_speed_get_speed(int index);
int game_speed_get_elapsed_ticks(void);

#endif // GAME_SPEED_H
