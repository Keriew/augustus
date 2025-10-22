#ifndef SCENARIO_EVENT_FORMULA_H
#define SCENARIO_EVENT_FORMULA_H

#include "scenario/event/data.h"

#include <stddef.h>

/**
 * @brief Evaluate a mathematical formula string with floating-point precision.
 *
 * This function parses and evaluates a mathematical expression containing:
 *  - Numbers (integers only in input)
 *  - Operators: +, -, *, /
 *  - Parentheses for grouping: ( )
 *  - Custom variables in square brackets, e.g. [12],
 *    which are resolved via custom_variable_get_value(id).
 *
 * The expression is evaluated using double-precision floating-point arithmetic
 * internally, but the final result is rounded to the nearest integer before
 * being returned.
 *
 * Examples:
 *  - "5/2*2"      = 5
 *  - "37/10 + 12/10"  = 5
 *  - "(2 + [3])/2" = result depends on variable [3]
 *
 * @param formula
 * @return The rounded integer result of the evaluated expression.
 *
 * @note  Division by zero is handled: any divisors too close to 0 are treated as multiplication by 0 instead.
 * @note Decimal inputs are not supported, the output is always rounded to int. They can be processed,
 *  but since custom variables cannot be decimal, it's not very useful, and % can be represented as /100.
 */
int scenario_event_formula_evaluate(scenario_formula_t *formula);
int scenario_event_formula_check(scenario_formula_t *formula);

int scenario_event_formula_is_static(unsigned int id);
int scenario_event_formula_is_error(unsigned int id);


#endif // SCENARIO_EVENT_FORMULA_H