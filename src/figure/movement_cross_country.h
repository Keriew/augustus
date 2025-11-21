// Header Guard
#ifndef MOVEMENT_CROSS_COUNTRY_H
#define MOVEMENT_CROSS_COUNTRY_H

#define CC_MAJOR_X 1
#define CC_MAJOR_Y 2

void figure_movement_set_cross_country_direction(
    figure *f, int x_src, int y_src, int x_dst, int y_dst, int is_missile);
void figure_movement_set_cross_country_destination(figure *f, int x_dst, int y_dst);
int figure_movement_cross_country(figure *f, int num_ticks);
int figure_movement_can_launch_cross_country_missile(int x_src, int y_src, int x_dst, int y_dst);

#endif