#ifndef SCENARIO_EVENTS_EXPORT_XML_H
#define SCENARIO_EVENTS_EXPORT_XML_H

#include "scenario/event/data.h"

/*
 * Auxiliary functions to correctly copy formulas since they are stored in their own array
 * @param action/condition A pointer to the action or condition in which the formulas to copy are
 * @param params A list of pointers to the values of parameter in the action/condition
 * @param index The index of the parameter to look for the formula
 */
void copy_formulas_action(scenario_action_t *action, int **params, int index);
void copy_formulas_condition(scenario_condition_t *condition, int **params, int index);

#endif // SCENARIO_EVENTS_EXPORT_XML_H
