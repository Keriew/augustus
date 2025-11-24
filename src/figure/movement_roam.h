// Header Guard
#ifndef MOVEMENT_ROAM_H
#define MOVEMENT_ROAM_H

#define ROAM_DECISION_INTERVAL 5
#define ROAM_INITIAL_OFFSET 8
#define ROAM_TURN_CW 2
#define ROAM_TURN_CCW -2

void figure_movement_init_roaming(figure *f);
void figure_movement_roam_ticks(figure *f, int num_ticks);


#endif