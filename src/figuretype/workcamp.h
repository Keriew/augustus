#ifndef FIGURETYPE_WORKCAMP_H
#define FIGURETYPE_WORKCAMP_H

#include "figure/figure.h"

#define CARTLOADS_PER_MONUMENT_DELIVERY 4

#ifdef __cplusplus
extern "C" {
#endif

void figure_workcamp_worker_action(figure *f);

void figure_workcamp_slave_action(figure *f);

void figure_workcamp_architect_action(figure *f);

#ifdef __cplusplus
}
#endif

#endif // FIGURETYPE_WORKCAMP_H
