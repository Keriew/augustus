#ifndef WINDOW_ADVISOR_ENTERTAINMENT_H
#define WINDOW_ADVISOR_ENTERTAINMENT_H

#include "window/advisors.h"

#ifdef __cplusplus
extern "C" {
#endif

void window_entertainment_draw_games_text(int x, int y, int draw_button_text);

const advisor_window_type *window_advisor_entertainment(void);

#ifdef __cplusplus
}
#endif

#endif // WINDOW_ADVISOR_ENTERTAINMENT_H
