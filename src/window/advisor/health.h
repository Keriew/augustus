#ifndef WINDOW_ADVISOR_HEALTH_H
#define WINDOW_ADVISOR_HEALTH_H

#include "window/advisors.h"

#ifdef __cplusplus
extern "C" {
#endif

int window_advisor_health_get_rating_text_id(void);
const advisor_window_type *window_advisor_health(void);

#ifdef __cplusplus
}
#endif

#endif // WINDOW_ADVISOR_HEALTH_H
