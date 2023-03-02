#include "victory.h"

#include "building/construction.h"
#include "city/data_private.h"
#include "city/figures.h"
#include "city/finance.h"
#include "city/message.h"
#include "core/config.h"
#include "game/time.h"
#include "scenario/criteria.h"
#include "scenario/property.h"
#include "sound/music.h"
#include "sound/speech.h"
#include "window/mission_end.h"
#include "window/victory_dialog.h"

static struct {
    int state;
    int force_win;
} data;

void city_victory_reset(void)
{
    data.state = VICTORY_STATE_NONE;
    data.force_win = 0;
}

void city_victory_force_win(void)
{
    data.force_win = 1;
}

int city_victory_state(void)
{
    return data.state;
}

static int determine_victory_state(void)
{
    int state = VICTORY_STATE_WON;
    int has_criteria = 0;

    win_criteria_satisfy_state criteria_state = scenario_criteria_test_all();
    switch (criteria_state) {
        case WIN_CRITERIA_STATE_NONE:
            // There are no criteria set
            state = VICTORY_STATE_NONE;
            has_criteria = 0;
            break;
        case WIN_CRITERIA_STATE_OK:
            // All criteria satisfied
            has_criteria = 1;
            break;
        case WIN_CRITERIA_STATE_FAIL:
            // At least one criteria not satisfied
            has_criteria = 1;
            state = VICTORY_STATE_NONE;
            break;
        case WIN_CRITERIA_STATE_LOSE:
            // At least one instant-loss criteria failed.
            return VICTORY_STATE_LOST;
            has_criteria = 1;
            break;
    }
    
    if (city_figures_total_invading_enemies() > 2 + city_data.figure.soldiers) {
        if (city_data.population.population < city_data.population.highest_ever / 4) {
            return VICTORY_STATE_LOST;
        }
    }
    if (city_figures_total_invading_enemies() > 0) {
        if (city_data.population.population <= 0) {
            return VICTORY_STATE_LOST;
        }
    }
    
    if (!has_criteria) {
        state = VICTORY_STATE_NONE;
    }
    return state;
}

void city_victory_check(void)
{
    if (scenario_is_open_play()) {
        return;
    }
    data.state = determine_victory_state();

    if (city_data.mission.has_won) {
        data.state = city_data.mission.continue_months_left <= 0 ? VICTORY_STATE_WON : VICTORY_STATE_NONE;
    }
    if (data.force_win) {
        data.state = VICTORY_STATE_WON;
    }
    if (data.state != VICTORY_STATE_NONE) {
        building_construction_clear_type();
        if (data.state == VICTORY_STATE_LOST) {
            if (city_data.mission.fired_message_shown) {
                window_mission_end_show_fired();
            } else {
                city_data.mission.fired_message_shown = 1;
                city_message_post(1, MESSAGE_FIRED, 0, 0);
            }
            data.force_win = 0;
        } else if (data.state == VICTORY_STATE_WON) {
            sound_music_stop();
            if (city_data.mission.victory_message_shown) {
                window_mission_end_show_won();
                data.force_win = 0;
            } else {
                city_data.mission.victory_message_shown = 1;
                sound_speech_play_file("wavs/fanfare_nu2.wav");
                window_victory_dialog_show();
            }
        }
    }
}

void city_victory_update_months_to_govern(void)
{
    if (city_data.mission.has_won) {
        city_data.mission.continue_months_left--;
    }
}

void city_victory_continue_governing(int months)
{
    city_data.mission.victory_message_shown = 0;
    city_data.mission.has_won = 1;
    city_data.mission.continue_months_left += months;
    city_data.mission.continue_months_chosen = months;
    city_data.emperor.salary_rank = 0;
    city_data.emperor.salary_amount = 0;
    city_finance_update_salary();
}

void city_victory_stop_governing(void)
{
    city_data.mission.has_won = 0;
    city_data.mission.continue_months_left = 0;
    city_data.mission.continue_months_chosen = 0;
}

int city_victory_has_won(void)
{
    return city_data.mission.has_won;
}
