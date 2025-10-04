#ifndef SCENARIO_EVENT_FORMULA_H
#define SCENARIO_EVENT_FORMULA_H

#include <stddef.h>

/**
 * @brief Evaluate an integer formula string.
 *
 * This function parses and evaluates a mathematical expression containing:
 *  - Integers
 *  - Operators: +, -, *, /
 *  - Parentheses for grouping: ( )
 *  - Custom variables in square brackets, e.g. [12],
 *    which are resolved via a user-provided get_var_value(int id) function.
 *
 * @param str  Null-terminated input string (max length ≈ 100 characters).
 * @return The evaluated integer result of the expression.
 *
 * @note The function performs integer arithmetic only (no floats).
 * @note Division by zero yields undefined behavior unless handled internally.
 */
int scenario_event_formula_evaluate(const char *str);

#endif // SCENARIO_EVENT_FORMULA_H