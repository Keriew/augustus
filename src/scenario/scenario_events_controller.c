#include "scenario_events_controller.h"

#include "core/log.h"
#include "game/save_version.h"
#include "scenario/actiontype/action_types.h"
#include "scenario/conditiontype/condition_types.h"
#include "scenario/scenario.h"
#include "scenario/scenario_event.h"

#define SCENARIO_EVENTS_SIZE_STEP 50

static array(scenario_event_t) scenario_events;

static int event_in_use(const scenario_event_t *event)
{
    return (event->state != EVENT_STATE_UNDEFINED);
}

void scenario_events_init()
{
    for (int i = 0; i < scenario_events.size; i++) {
        scenario_event_t *current = array_item(scenario_events, i);
        scenario_event_init(current);
    }
}

void new_scenario_event(scenario_event_t *obj, int position)
{
    obj->id = position;
}

void scenario_events_clear()
{
    scenario_events.size = 0;
    if (!array_init(scenario_events, SCENARIO_EVENTS_SIZE_STEP, new_scenario_event, event_in_use)) {
        log_error("Unable to allocate enough memory for the scenario events array. The game will now crash.", 0, 0);
    }
}

scenario_event_t *scenario_event_get(int event_id)
{
    return array_item(scenario_events, event_id);
}

scenario_event_t *scenario_event_create(int repeat_min, int repeat_max, int max_repeats)
{
    if (repeat_min > 0
        && repeat_max < repeat_min) {
        log_error("Event maximum repeat is less than its minimum.", 0, 0);
        return 0;
    }

    scenario_event_t *event = 0;
    array_new_item(scenario_events, 0, event);
    if (!event) {
        return 0;
    }
    event->id = scenario_events.size - 1;
    event->state = EVENT_STATE_ACTIVE;
    event->repeat_months_min = repeat_min;
    event->repeat_months_max = repeat_max;
    event->max_number_of_repeats = max_repeats;

    return event;
}

int scenario_events_get_count()
{
    return scenario_events.size;
}

static void scenario_events_info_save_state(buffer *buf)
{
    int32_t array_size = scenario_events.size;
    int32_t struct_size = (6 * sizeof(int32_t)) + (3 * sizeof(int16_t));
    buffer_init_dynamic_piece(buf,
        SCENARIO_CURRENT_VERSION,
        array_size,
        struct_size);

    for (int i = 0; i < scenario_events.size; i++) {
        scenario_event_save_state(buf, array_item(scenario_events, i));
    }
}

static void scenario_events_conditions_save_state(buffer *buf)
{
    int32_t array_size = 0;
    for (int i = 0; i < scenario_events.size; i++) {
        scenario_event_t *current = array_item(scenario_events, i);
        array_size += current->conditions.size;
    }
    // If in future conditions can be linked to more things, then also take them into account here.

    int32_t struct_size = (2 * sizeof(int16_t)) + (6 * sizeof(int32_t));
    buffer_init_dynamic_piece(buf,
        SCENARIO_CURRENT_VERSION,
        array_size,
        struct_size);

    for (int i = 0; i < scenario_events.size; i++) {
        scenario_event_t *current_event = array_item(scenario_events, i);

        for (int j = 0; j < current_event->conditions.size; j++) {
            scenario_condition_t *current = array_item(current_event->conditions, j);
            scenario_condition_save_state(buf, current, LINK_TYPE_SCENARIO_EVENT, current_event->id);
        }
    }
}

static void scenario_events_actions_save_state(buffer *buf)
{
    int32_t array_size = 0;
    for (int i = 0; i < scenario_events.size; i++) {
        scenario_event_t *current = array_item(scenario_events, i);
        array_size += current->actions.size;
    }
    // If in future actions can be linked to more things, then also take them into account here.

    int32_t struct_size = (2 * sizeof(int16_t)) + (6 * sizeof(int32_t));
    buffer_init_dynamic_piece(buf,
        SCENARIO_CURRENT_VERSION,
        array_size,
        struct_size);

    for (int i = 0; i < scenario_events.size; i++) {
        scenario_event_t *current_event = array_item(scenario_events, i);

        for (int j = 0; j < current_event->actions.size; j++) {
            scenario_action_t *current = array_item(current_event->actions, j);
            scenario_action_save_state(buf, current, LINK_TYPE_SCENARIO_EVENT, current_event->id);
        }
    }
}

void scenario_events_save_state(buffer *buf_events, buffer *buf_conditions, buffer *buf_actions)
{
    scenario_events_info_save_state(buf_events);
    scenario_events_conditions_save_state(buf_conditions);
    scenario_events_actions_save_state(buf_actions);
}

static void scenario_load_link_condition(scenario_condition_t *condition, int link_type, int32_t link_id)
{
    switch (link_type) {
        case LINK_TYPE_SCENARIO_EVENT:
            {
                scenario_event_t *event = scenario_event_get(link_id);
                scenario_event_link_condition(event, condition);
            }
            break;
        default:
            log_error("Unhandled condition link type. The game will probably crash.", 0, 0);
            break;
    }
}

static void scenario_events_info_load_state(buffer *buf)
{
    int buffer_size, version, array_size, struct_size;
    buffer_load_dynamic_piece_header_data(buf,
        &buffer_size,
        &version,
        &array_size,
        &struct_size);

    int link_type = 0;
    int32_t link_id = 0;
    for (int i = 0; i < array_size; i++) {
        scenario_event_t *event = scenario_event_create(0, 0, 0);
        scenario_event_load_state(buf, version, event);
    }
}

static void scenario_conditions_load_state(buffer *buf)
{
    int buffer_size, version, array_size, struct_size;
    buffer_load_dynamic_piece_header_data(buf,
        &buffer_size,
        &version,
        &array_size,
        &struct_size);

    int link_type = 0;
    int32_t link_id = 0;
    for (int i = 0; i < array_size; i++) {
        scenario_condition_t condition;
        scenario_condition_load_state(buf, version, &condition, &link_type, &link_id);
        scenario_load_link_condition(&condition, link_type, link_id);
    }
}

static void scenario_load_link_action(scenario_action_t *action, int link_type, int32_t link_id)
{
    switch (link_type) {
        case LINK_TYPE_SCENARIO_EVENT:
            {
                scenario_event_t *event = scenario_event_get(link_id);
                scenario_event_link_action(event, action);
            }
            break;
        default:
            log_error("Unhandled action link type. The game will probably crash.", 0, 0);
            break;
    }
}

static void scenario_actions_load_state(buffer *buf)
{
    int buffer_size, version, array_size, struct_size;
    buffer_load_dynamic_piece_header_data(buf,
        &buffer_size,
        &version,
        &array_size,
        &struct_size);

    int link_type = 0;
    int32_t link_id = 0;
    for (int i = 0; i < array_size; i++) {
        scenario_action_t action;
        scenario_action_load_state(buf, version, &action, &link_type, &link_id);
        scenario_load_link_action(&action, link_type, link_id);
    }
}

void scenario_events_load_state(buffer *buf_events, buffer *buf_conditions, buffer *buf_actions)
{
    scenario_events_clear();
    scenario_events_info_load_state(buf_events);
    scenario_conditions_load_state(buf_conditions);
    scenario_actions_load_state(buf_actions);
}

void scenario_events_process_all()
{
    for (int i = 0; i < scenario_events.size; i++) {
        scenario_event_t *current = array_item(scenario_events, i);
        scenario_event_conditional_execute(current);
    }
}

void scenario_events_progress_paused(int months_passed)
{
    for (int i = 0; i < scenario_events.size; i++) {
        scenario_event_t *current = array_item(scenario_events, i);
        scenario_event_decrease_pause_time(current, months_passed);
    }
}
