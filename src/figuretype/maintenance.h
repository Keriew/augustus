#ifndef FIGURETYPE_MAINTENANCE_H
#define FIGURETYPE_MAINTENANCE_H

#include "figure/figure.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Native FigureType runtime owns Engineer behavior; this is only an action-table guard. */
void figure_engineer_action(figure *f);

/* Native FigureType runtime owns Prefect behavior; this is only an action-table guard. */
void figure_prefect_action(figure *f);

void figure_worker_action(figure *f);

#ifdef __cplusplus
}
#endif

#endif // FIGURETYPE_MAINTENANCE_H
