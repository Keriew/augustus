#ifndef FIGURETYPE_MIGRANT_H
#define FIGURETYPE_MIGRANT_H

#include "figure/figure.h"

typedef struct building building;

#ifdef __cplusplus
extern "C" {
#endif

void figure_create_immigrant(building *house, int num_people);

void figure_create_emigrant(building *house, int num_people);

figure *figure_create_homeless(building *house, int num_people);

void figure_immigrant_action(figure *f);

void figure_emigrant_action(figure *f);

void figure_homeless_action(figure *f);

#ifdef __cplusplus
}
#endif

#endif // FIGURETYPE_MIGRANT_H
