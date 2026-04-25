#ifndef FIGURETYPE_WATER_H
#define FIGURETYPE_WATER_H

#include "figure/figure.h"

#ifdef __cplusplus
extern "C" {
#endif

void figure_create_flotsam(void);

void figure_flotsam_action(figure *f);

void figure_shipwreck_action(figure *f);

void figure_fishing_boat_action(figure *f);

void figure_sink_all_ships(void);
void figure_sink_half_ships(void);

#ifdef __cplusplus
}
#endif

#endif // FIGURETYPE_WATER_H
