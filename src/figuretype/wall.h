#ifndef FIGURETYPE_WALL_H
#define FIGURETYPE_WALL_H

#include "figure/figure.h"
#include "building/building.h"

void figure_ballista_action(figure *f);

void figure_tower_sentry_set_image(figure *f);

void figure_tower_sentry_action(figure *f);

void figure_tower_sentry_reroute(void);

void figure_kill_tower_sentries_at(int x, int y);

void figure_kill_tower_sentries_in_building(building *b);

void figure_watchman_action(figure *f);

void figure_watchtower_archer_action(figure *f);


#endif // FIGURETYPE_WALL_H
