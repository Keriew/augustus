#ifndef GAME_TIME_H
#define GAME_TIME_H

#include "core/buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Game time.
 * One year has 12 months.
 */
#define GAME_TIME_MONTHS_PER_YEAR 12
#define GAME_TIME_LEGACY_DAYS_PER_MONTH 16
#define GAME_TIME_TICKS_PER_DAY (game_time_ticks_per_day())
#define GAME_TIME_DAYS_PER_MONTH (game_time_days_in_month(game_time_month()))

/**
 * Initializes game time to the given year with all other fields zero
 * @param year Year
 */
void game_time_init(int year);

/**
 * The current game year
 *
 */
int game_time_year(void);

/**
 * The current game month within the year
 */
int game_time_month(void);

/**
 * The current game day within the month
 */
int game_time_day(void);

/**
 * The current game tick within the day
 */
int game_time_tick(void);

int game_time_ticks_per_day(void);
int game_time_days_in_month(int month);
int game_time_days_in_current_month(void);
int game_time_days_per_year(void);
int game_time_ticks_per_month(int month);
int game_time_ticks_per_year(void);
int game_time_is_last_day_of_month(void);
int game_time_is_last_day_of_year(void);

/**
 * Increases tick
 * @return True if the tick overflows
 */
int game_time_advance_tick(void);

/**
 * Increase day
 * @return True if the day overflows
 */
int game_time_advance_day(void);

/**
 * Increase month
 * @return True if the month overflows
 */
int game_time_advance_month(void);

/**
 * Increase year
 */
void game_time_advance_year(void);

int game_time_total_months(void);
int game_time_total_years(void);

/**
 * Saves the game time
 * @param buf Buffer
 */
void game_time_save_state(buffer *buf);

/**
 * Loads the game time
 * @param buf Buffer
 */
void game_time_load_state(buffer *buf);

void game_time_load_basic_info(buffer *buf, int *month, int *year);

#ifdef __cplusplus
}
#endif

#endif // GAME_TIME_H
