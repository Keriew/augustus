#ifndef SCENARIO_EVENT_FORMULA_H
#define SCENARIO_EVENT_FORMULA_H

#include <stddef.h>

/**
 * @brief Evaluate a mathematical formula string with floating-point precision.
 *
 * This function parses and evaluates a mathematical expression containing:
 *  - Numbers (integer or decimal)
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
 *  - "5/2*2"      → 5
 *  - "3.7 + 1.2"  → 5
 *  - "(2 + [3])/2" → result depends on variable [3]
 *
 * @param str  Null-terminated input string (max length ≈ 100 characters).
 * @return The rounded integer result of the evaluated expression.
 *
 * @note  Division by zero is handled: any divisors too close to 0 are treated as multiplication by 0 instead.
 * @note Decimal inputs are supported, but the output is always rounded to int.
 */
int scenario_event_formula_evaluate(const char *str);


#endif // SCENARIO_EVENT_FORMULA_H