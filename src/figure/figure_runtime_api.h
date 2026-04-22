#ifndef FIGURE_RUNTIME_API_H
#define FIGURE_RUNTIME_API_H

#include "figure/figure.h"

#ifdef __cplusplus
extern "C" {
#endif

void figure_runtime_reset(void);
void figure_runtime_initialize_city(void);
void figure_runtime_on_created(figure *f);
void figure_runtime_on_deleted(figure *f);
int figure_runtime_execute(figure *f);

#ifdef __cplusplus
}
#endif

#endif // FIGURE_RUNTIME_API_H
