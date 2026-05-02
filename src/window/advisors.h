#ifndef WINDOW_ADVISORS_H
#define WINDOW_ADVISORS_H

#include "city/constants.h"
#include "graphics/tooltip.h"
#include "input/mouse.h"

typedef struct {
    int text_id;
    int translation_key;
    const uint8_t *precomposed_text;
} advisor_tooltip_result;

typedef struct {
    /**
     * @return height of the advisor in blocks of 16px
     */
    int (*draw_background)(void);
    void (*draw_foreground)(void);
    int (*handle_mouse)(const mouse *m);
    void (*get_tooltip_text)(advisor_tooltip_result *r);
} advisor_window_type;

void window_advisors_set_advisor(advisor_type advisor);

advisor_type window_advisors_get_advisor(void);

void window_advisors_draw_dialog_background(void);

void window_advisors_show(void);

void window_advisors_show_checked(void);

int window_advisors_show_advisor(advisor_type advisor);

int window_advisors_get_tavern_coverage(void);
int window_advisors_get_theater_coverage(void);
int window_advisors_get_amphitheater_coverage(void);
int window_advisors_get_arena_coverage(void);
int window_advisors_get_colosseum_coverage(void);
int window_advisors_get_hippodrome_coverage(void);

int window_advisors_get_bathhouse_coverage(void);
int window_advisors_get_barber_coverage(void);
int window_advisors_get_clinic_coverage(void);
int window_advisors_get_hospital_coverage(void);

int window_advisors_get_school_coverage(void);
int window_advisors_get_library_coverage(void);
int window_advisors_get_academy_coverage(void);

#endif // WINDOW_ADVISORS_H
