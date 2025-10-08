#include "controller.h"

#include "core/log.h"
#include "game/save_version.h"
#include "scenario/event/action_handler.h"
#include "scenario/event/condition_handler.h"
#include "scenario/event/event.h"
#include "scenario/event/formula.h"
#include "scenario/scenario.h"

#include <string.h>

#define SCENARIO_EVENTS_SIZE_STEP 50

static array(scenario_event_t) scenario_events;
static scenario_formula_t scenario_formulas[MAX_FORMULAS];
static unsigned int scenario_formulas_size;

static void formulas_save_state(buffer *buf);
static void formulas_load_state(buffer *buf);

void scenario_events_init(void)
{
    scenario_event_t *current;
    array_foreach(scenario_events, current)
    {
        scenario_event_init(current);
    }
}

unsigned int scenario_formula_add(const uint8_t *formatted_calculation)
{
    scenario_formulas_size++;
    if (scenario_formulas_size >= MAX_FORMULAS) {
        log_error("Maximum number of custom formulas reached.", 0, 0);
        return 0;
    }
    scenario_formula_t calculation;
    calculation.id = scenario_formulas_size;
    calculation.evaluation = 0;
    strncpy((char *) calculation.formatted_calculation, (const char *) formatted_calculation, MAX_FORMULA_LENGTH - 1);
    scenario_formulas[scenario_formulas_size] = calculation;
    scenario_event_formula_check(&scenario_formulas[scenario_formulas_size]);
    // null termination on last char-  treating as string 
    return calculation.id;
}

void scenario_formula_change(unsigned int id, const uint8_t *formatted_calculation)
{
    if (id <= 0 || id > scenario_formulas_size) {
        log_error("Invalid formula ID.", 0, 0);
        return;
    }
    strncpy((char *) scenario_formulas[id].formatted_calculation,
        (const char *) formatted_calculation, MAX_FORMULA_LENGTH - 1);
}

const uint8_t *scenario_formula_get_string(unsigned int id)
{
    if (id == 0 || id > scenario_formulas_size) {
        log_error("Invalid formula index.", 0, 0);
        return NULL;
    }
    return scenario_formulas[id].formatted_calculation;
}

scenario_formula_t *scenario_formula_get(unsigned int id)
{
    if (id == 0 || id > scenario_formulas_size) {
        log_error("Invalid formula index.", 0, 0);
        return NULL;
    }
    return &scenario_formulas[id];
}

int scenario_formula_evaluate_formula(unsigned int id)
{
    if (id == 0 || id > scenario_formulas_size) {
        log_error("Invalid formula index.", 0, 0);
        return 0;
    }
    int evaluation = scenario_event_formula_evaluate(&scenario_formulas[id]);
    return evaluation;
}

void scenario_events_clear(void)
{
    scenario_event_t *current;
    array_foreach(scenario_events, current)
    {
        scenario_event_delete(current);
    }
    scenario_events.size = 0;
    if (!array_init(scenario_events, SCENARIO_EVENTS_SIZE_STEP, scenario_event_new, scenario_event_is_active)) {
        log_error("Unable to allocate enough memory for the scenario events array. The game will now crash.", 0, 0);
    }

    // Clear formulas
    scenario_formulas_size = 0;
    memset(scenario_formulas, 0, sizeof(scenario_formulas));
}

scenario_event_t *scenario_event_get(int event_id)
{
    return array_item(scenario_events, event_id);
}

scenario_event_t *scenario_event_create(int repeat_min, int repeat_max, int max_repeats)
{
    if (repeat_min < 0) {
        log_error("Event minimum repeat is less than 0.", 0, 0);
        return 0;
    }
    if (repeat_max < 0) {
        log_error("Event maximum repeat is less than 0.", 0, 0);
        return 0;
    }

    if (repeat_max < repeat_min) {
        log_info("Event maximum repeat is less than its minimum. Swapping the two values.", 0, 0);
        int temp = repeat_min;
        repeat_min = repeat_max;
        repeat_max = temp;
    }

    scenario_event_t *event = 0;
    array_new_item(scenario_events, event);
    if (!event) {
        return 0;
    }
    event->state = EVENT_STATE_ACTIVE;
    event->repeat_days_min = repeat_min;
    event->repeat_days_max = repeat_max;
    event->max_number_of_repeats = max_repeats;

    return event;
}

void scenario_event_delete(scenario_event_t *event)
{
    scenario_condition_group_t *condition_group;
    array_foreach(event->condition_groups, condition_group)
    {
        array_clear(condition_group->conditions);
    }
    array_clear(event->condition_groups);
    array_clear(event->actions);
    memset(event, 0, sizeof(scenario_event_t));
    event->state = EVENT_STATE_UNDEFINED;
    array_trim(scenario_events);
}

int scenario_events_get_count(void)
{
    return scenario_events.size;
}

static void info_save_state(buffer *buf)
{
    uint32_t array_size = scenario_events.size;
    uint32_t struct_size = (6 * sizeof(int32_t)) + (3 * sizeof(int16_t)) + EVENT_NAME_LENGTH * 2 * sizeof(char);
    buffer_init_dynamic_array(buf, array_size, struct_size);

    scenario_event_t *current;
    array_foreach(scenario_events, current)
    {
        scenario_event_save_state(buf, current);
    }
}

static void conditions_save_state(buffer *buf)
{
    unsigned int total_groups = 0;
    unsigned int total_conditions = 0;

    const scenario_event_t *event;
    const scenario_condition_group_t *group;

    array_foreach(scenario_events, event)
    {
        total_groups += event->condition_groups.size;
        array_foreach(event->condition_groups, group)
        {
            total_conditions += group->conditions.size;
        }
    }

    unsigned int size = total_groups * CONDITION_GROUP_STRUCT_SIZE + total_conditions * CONDITION_STRUCT_SIZE;

    buffer_init_dynamic(buf, size);

    array_foreach(scenario_events, event)
    {
        array_foreach(event->condition_groups, group)
        {
            scenario_condition_group_save_state(buf, group, LINK_TYPE_SCENARIO_EVENT, event->id);
        }
    }
}

static void actions_save_state(buffer *buf)
{
    int32_t array_size = 0;
    scenario_event_t *current_event;
    array_foreach(scenario_events, current_event)
    {
        array_size += current_event->actions.size;
    }

    int struct_size = (2 * sizeof(int16_t)) + (6 * sizeof(int32_t));
    buffer_init_dynamic_array(buf, array_size, struct_size);

    for (int i = 0; i < scenario_events.size; i++) {
        current_event = array_item(scenario_events, i);

        for (int j = 0; j < current_event->actions.size; j++) {
            scenario_action_t *current_action = array_item(current_event->actions, j);
            scenario_action_type_save_state(buf, current_action, LINK_TYPE_SCENARIO_EVENT, current_event->id);
        }
    }

}

void scenario_events_save_state(buffer *buf_events, buffer *buf_conditions, buffer *buf_actions, buffer *buf_formulas)
{
    info_save_state(buf_events);
    conditions_save_state(buf_conditions);
    actions_save_state(buf_actions);
    formulas_save_state(buf_formulas);
}

static void info_load_state(buffer *buf, int scenario_version)
{
    unsigned int array_size = buffer_load_dynamic_array(buf);

    for (unsigned int i = 0; i < array_size; i++) {
        scenario_event_t *event = scenario_event_create(0, 0, 0);
        scenario_event_load_state(buf, event, scenario_version);
    }
}

static void conditions_load_state_old_version(buffer *buf)
{
    unsigned int total_conditions = buffer_load_dynamic_array(buf);

    for (unsigned int i = 0; i < total_conditions; i++) {
        buffer_skip(buf, 2); // Skip the link type
        int event_id = buffer_read_i32(buf);
        scenario_event_t *event = scenario_event_get(event_id);
        scenario_condition_group_t *group = array_item(event->condition_groups, 0);
        scenario_condition_t *condition;
        array_new_item(group->conditions, condition);
        scenario_condition_load_state(buf, group, condition);
    }
}

static void load_link_condition_group(scenario_condition_group_t *condition_group, int link_type, int32_t link_id)
{
    switch (link_type) {
        case LINK_TYPE_SCENARIO_EVENT:
        {
            scenario_event_t *event = scenario_event_get(link_id);
            scenario_event_link_condition_group(event, condition_group);
        }
        break;
        default:
            log_error("Unhandled condition link type. The game will probably crash.", 0, 0);
            break;
    }
}

static void conditions_load_state(buffer *buf)
{
    buffer_load_dynamic(buf);

    int link_type = 0;
    int32_t link_id = 0;

    // Using `buffer_at_end` is a hackish way to load all the condition groups
    // However, we never stored the total condition group count anywhere and, except for some sort of corruption,
    // this should work. Regardless, this is not a good practice.
    while (!buffer_at_end(buf)) {
        scenario_condition_group_t condition_group = { 0 };
        scenario_condition_group_load_state(buf, &condition_group, &link_type, &link_id);
        load_link_condition_group(&condition_group, link_type, link_id);
    }
}

static void load_link_action(scenario_action_t *action, int link_type, int32_t link_id)
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

static void actions_load_state(buffer *buf, int is_new_version)
{
    unsigned int array_size = buffer_load_dynamic_array(buf);

    int link_type = 0;
    int32_t link_id = 0;
    for (unsigned int i = 0; i < array_size; i++) {
        scenario_action_t action = { 0 };
        int original_id = scenario_action_type_load_state(buf, &action, &link_type, &link_id, is_new_version);
        load_link_action(&action, link_type, link_id);
        if (original_id) {
            unsigned int index = 1;
            while (index) {
                index = scenario_action_type_load_allowed_building(&action, original_id, index);
                load_link_action(&action, link_type, link_id);
            }
        }
    }
}

static void formulas_save_state(buffer *buf)
{
    // Each record: id (u32) + formatted_calculation (fixed MAX_FORMULA_LENGTH bytes) + evaluation (i32)
    int struct_size = sizeof(uint32_t) + MAX_FORMULA_LENGTH + sizeof(int32_t);
    buffer_init_dynamic_array(buf, scenario_formulas_size, struct_size);

    for (unsigned int id = 1; id <= scenario_formulas_size; ++id) {
        buffer_write_u32(buf, id);
        buffer_write_raw(buf, scenario_formulas[id].formatted_calculation, MAX_FORMULA_LENGTH);
        buffer_write_i32(buf, scenario_formulas[id].evaluation);
        buffer_write_u8(buf, scenario_formulas[id].is_static);
        buffer_write_u8(buf, scenario_formulas[id].is_error);
    }
}

static void formulas_load_state(buffer *buf)
{
    unsigned int array_size = buffer_load_dynamic_array(buf);
    memset(scenario_formulas, 0, sizeof(scenario_formulas));
    scenario_formulas_size = 0;
    unsigned int max_id = 0;
    for (unsigned int i = 0; i < array_size; ++i) {

        unsigned int id = buffer_read_u32(buf);
        scenario_formulas[id].id = id;
        if (id >= MAX_FORMULAS) {// Sanity guard: discard out-of-range IDs
            // Skip payload for this bad entry
            buffer_skip(buf, MAX_FORMULA_LENGTH + (unsigned int) sizeof(int32_t));
            continue;
        }
        buffer_read_raw(buf, scenario_formulas[id].formatted_calculation, MAX_FORMULA_LENGTH);
        scenario_formulas[id].formatted_calculation[MAX_FORMULA_LENGTH - 1] = '\0'; // ensure safety
        scenario_formulas[id].evaluation = buffer_read_i32(buf);
        scenario_formulas[id].is_static = buffer_read_u8(buf);
        scenario_formulas[id].is_error = buffer_read_u8(buf);
        max_id = id > max_id ? id : max_id;
    }

    // Make size reflect the highest valid ID loaded (your IDs are 1-based)
    scenario_formulas_size = max_id; // should be also equal to array_size unless there were bad IDs
}

void scenario_events_load_state(buffer *buf_events, buffer *buf_conditions, buffer *buf_actions, buffer *buf_formulas,
     int scenario_version)
{
    scenario_events_clear();
    info_load_state(buf_events, scenario_version);
    if (scenario_version > SCENARIO_LAST_STATIC_ORIGINAL_DATA) {
        conditions_load_state(buf_conditions);
    } else {
        conditions_load_state_old_version(buf_conditions);
    }
    actions_load_state(buf_actions, scenario_version > SCENARIO_LAST_STATIC_ORIGINAL_DATA);
    if (scenario_version > SCENARIO_LAST_NO_FORMULAS) {
        formulas_load_state(buf_formulas);
    }

    scenario_event_t *current;
    array_foreach(scenario_events, current)
    {
        if (current->state == EVENT_STATE_DELETED) {
            current->state = EVENT_STATE_UNDEFINED;
        }
    }
}

void scenario_events_process_all(void)
{
    scenario_event_t *current;
    array_foreach(scenario_events, current)
    {
        scenario_event_conditional_execute(current);
    }
}

scenario_event_t *scenario_events_get_using_custom_variable(int custom_variable_id)
{
    scenario_event_t *current;
    array_foreach(scenario_events, current)
    {
        if (scenario_event_uses_custom_variable(current, custom_variable_id)) {
            return current;
        }
    }
    return 0;
}

void scenario_events_progress_paused(int days_passed)
{
    scenario_event_t *current;
    array_foreach(scenario_events, current)
    {
        scenario_event_decrease_pause_time(current, days_passed);
    }
}
