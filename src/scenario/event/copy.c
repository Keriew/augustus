#include "copy.h"

#include "scenario/event/parameter_data.h"
#include "scenario/event/controller.h"

void copy_formulas_action(scenario_action_t *action, int **params, int index)
{
    int *param_value = params[index - 1];
    parameter_type p_type = scenario_events_parameter_data_get_action_parameter_type(
        action->type, index, NULL, NULL);
    if (p_type == PARAMETER_TYPE_FORMULA || p_type == PARAMETER_TYPE_GRID_SLICE) {
        scenario_formula_t *formula = scenario_formula_get(*param_value);
        if (!formula) {
            return;
        }
        unsigned int id = scenario_formula_add(scenario_formula_get_string(*param_value),
            formula->min_evaluation, formula->max_evaluation);
        *param_value = id;
    }
}

void copy_formulas_condition(scenario_condition_t *condition, int **params, int index)
{
    int *param_value = params[index - 1];
    parameter_type p_type = scenario_events_parameter_data_get_condition_parameter_type(
        condition->type, index, NULL, NULL);
    if (p_type == PARAMETER_TYPE_FORMULA || p_type == PARAMETER_TYPE_GRID_SLICE) {
        scenario_formula_t *formula = scenario_formula_get(*param_value);
        if (!formula) {
            return;
        }
        unsigned int id = scenario_formula_add(scenario_formula_get_string(*param_value),
            formula->min_evaluation, formula->max_evaluation);
        *param_value = id;
    }
}
