#include "game/time.h"

extern "C" {
#include "game/defines.h"
}

namespace {

struct GameTimeData {
    int tick = 0;
    int day = 0;
    int month = 0;
    int year = 0;
    int total_days = 0;
} g_data;

static int calculate_total_months_from_days(int total_days)
{
    const int days_per_year = game_time_days_per_year();
    if (days_per_year <= 0) {
        return 0;
    }

    int total_months = (total_days / days_per_year) * GAME_TIME_MONTHS_PER_YEAR;
    int remainder_days = total_days % days_per_year;
    for (int month = 0; month < GAME_TIME_MONTHS_PER_YEAR; month++) {
        const int days_in_month = game_time_days_in_month(month);
        if (remainder_days < days_in_month) {
            break;
        }
        remainder_days -= days_in_month;
        total_months++;
    }

    return total_months;
}

static void clamp_loaded_time_state()
{
    if (g_data.month < 0) {
        g_data.month = 0;
    } else if (g_data.month >= GAME_TIME_MONTHS_PER_YEAR) {
        g_data.month = GAME_TIME_MONTHS_PER_YEAR - 1;
    }

    const int ticks_per_day = game_time_ticks_per_day();
    if (g_data.tick < 0) {
        g_data.tick = 0;
    } else if (ticks_per_day > 0 && g_data.tick >= ticks_per_day) {
        g_data.tick = ticks_per_day - 1;
    }

    const int days_in_month = game_time_days_in_month(g_data.month);
    if (g_data.day < 0) {
        g_data.day = 0;
    } else if (days_in_month > 0 && g_data.day >= days_in_month) {
        g_data.day = days_in_month - 1;
    }

    if (g_data.total_days < 0) {
        g_data.total_days = 0;
    }
}

} // namespace

extern "C" void game_time_init(int year)
{
    g_data.tick = 0;
    g_data.day = 0;
    g_data.month = 0;
    g_data.total_days = 0;
    g_data.year = year;
}

extern "C" int game_time_tick(void)
{
    return g_data.tick;
}

extern "C" int game_time_day(void)
{
    return g_data.day;
}

extern "C" int game_time_month(void)
{
    return g_data.month;
}

extern "C" int game_time_year(void)
{
    return g_data.year;
}

extern "C" int game_time_ticks_per_day(void)
{
    return game_defines_ticks_per_day();
}

extern "C" int game_time_days_in_month(int month)
{
    return game_defines_days_in_month(month);
}

extern "C" int game_time_days_in_current_month(void)
{
    return game_time_days_in_month(g_data.month);
}

extern "C" int game_time_days_per_year(void)
{
    return game_defines_days_per_year();
}

extern "C" int game_time_ticks_per_month(int month)
{
    return game_defines_ticks_per_month(month);
}

extern "C" int game_time_ticks_per_year(void)
{
    return game_defines_ticks_per_year();
}

extern "C" int game_time_is_last_day_of_month(void)
{
    return game_defines_is_last_day_of_month(g_data.month, g_data.day);
}

extern "C" int game_time_is_last_day_of_year(void)
{
    return game_defines_is_last_day_of_year(g_data.month, g_data.day);
}

extern "C" int game_time_advance_tick(void)
{
    if (++g_data.tick >= game_time_ticks_per_day()) {
        g_data.tick = 0;
        return 1;
    }
    return 0;
}

extern "C" int game_time_advance_day(void)
{
    g_data.total_days++;
    if (++g_data.day >= game_time_days_in_current_month()) {
        g_data.day = 0;
        return 1;
    }
    return 0;
}

extern "C" int game_time_advance_month(void)
{
    if (++g_data.month >= GAME_TIME_MONTHS_PER_YEAR) {
        g_data.month = 0;
        return 1;
    }
    return 0;
}

extern "C" void game_time_advance_year(void)
{
    ++g_data.year;
}

extern "C" int game_time_total_months(void)
{
    return calculate_total_months_from_days(g_data.total_days);
}

extern "C" int game_time_total_years(void)
{
    return game_time_total_months() / GAME_TIME_MONTHS_PER_YEAR;
}

extern "C" void game_time_save_state(buffer *buf)
{
    buffer_write_i32(buf, g_data.tick);
    buffer_write_i32(buf, g_data.day);
    buffer_write_i32(buf, g_data.month);
    buffer_write_i32(buf, g_data.year);
    buffer_write_i32(buf, g_data.total_days);
}

extern "C" void game_time_load_state(buffer *buf)
{
    g_data.tick = buffer_read_i32(buf);
    g_data.day = buffer_read_i32(buf);
    g_data.month = buffer_read_i32(buf);
    g_data.year = buffer_read_i32(buf);
    g_data.total_days = buffer_read_i32(buf);
    clamp_loaded_time_state();
}

extern "C" void game_time_load_basic_info(buffer *buf, int *month, int *year)
{
    buffer_skip(buf, 8);
    *month = buffer_read_i32(buf);
    *year = buffer_read_i32(buf);
}
