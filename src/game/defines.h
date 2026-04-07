#ifndef GAME_DEFINES_H
#define GAME_DEFINES_H

#ifdef __cplusplus
extern "C" {
#endif

int game_defines_load(void);
const char *game_defines_get_failure_reason(void);

int game_defines_ticks_per_day(void);
int game_defines_days_in_month(int month);
int game_defines_days_per_year(void);
int game_defines_ticks_per_month(int month);
int game_defines_ticks_per_year(void);
int game_defines_is_last_day_of_month(int month, int day);
int game_defines_is_last_day_of_year(int month, int day);

int game_defines_mortality_percentage(int health_bucket, int age_decennium);

#ifdef __cplusplus
}
#endif

#endif // GAME_DEFINES_H
