#include "formula.h"

#include "core/random.h"
#include "scenario/custom_variable.h"
#include "scenario/event/controller.h"
#include "scenario/event/data.h"


#include <stdio.h>
#include <ctype.h>
#include <math.h>

#define CLAMP(x, low, high) ((x) < (low) ? (low) : ((x) > (high) ? (high) : (x))) // simple clamp macro 
// because mathematicians have only discovered clamping in 2023, so its not in all math.h yet

double parse_expr(const char **s);
static int formula_evaluate(const char *str);

double get_var_value(int id)
{
    // variables are limited to int, cast to double
    return (double) scenario_custom_variable_get_value(id);
}

void skip_spaces(const char **s)
{
    while (isspace(**s)) (*s)++;
}

double parse_number(const char **s)
{
    double val = 0.0;
    double frac = 0.0;
    double divisor = 1.0;
    int has_decimal = 0;

    while (isdigit(**s) || **s == '.') {
        if (**s == '.') {
            if (has_decimal) break; // stop if second dot
            has_decimal = 1;
            (*s)++;
        } else if (!has_decimal) {
            val = val * 10.0 + (**s - '0');
            (*s)++;
        } else {
            frac = frac * 10.0 + (**s - '0');
            divisor *= 10.0;
            (*s)++;
        }
    }

    return val + frac / divisor;
}

double parse_factor(const char **s)
{
    skip_spaces(s);

    if (**s == '(') {
        (*s)++;
        double val = parse_expr(s);
        if (**s == ',') {
            (*s)++;
            double val2 = parse_expr(s);
            val = val < val2 ? random_between_from_stdlib(val, val2) : random_between_from_stdlib(val2, val);
            // random_between_from_stdlib does only work if min <= max otherwise it return min so the values have to be switched
        }
        if (**s == ')') (*s)++;
        return val;
    } else if (**s == '[') {
        (*s)++;
        int id = (int) parse_number(s);
        if (**s == ']') (*s)++;
        return get_var_value(id);
    } else if (**s == '-') { // unary minus
        (*s)++;
        return -parse_factor(s);
    } else if (isdigit(**s) || **s == '.') {
        return parse_number(s);
    }

    return 0.0; // fallback
}

double parse_term(const char **s)
{
    double val = parse_factor(s);
    skip_spaces(s);

    while (**s == '*' || **s == '/') {
        char op = **s;
        (*s)++;
        double right = parse_factor(s);
        skip_spaces(s);

        if (op == '*') {
            val *= right;
        } else if (op == '/') {
            if (fabs(right) < 1e-12) {
                // Treat division by zero as multiplication by zero
                val = 0.0;
            } else {
                val /= right;
            }
        }
    }

    return val;
}


double parse_expr(const char **s)
{
    double val = parse_term(s);
    skip_spaces(s);

    while (**s == '+' || **s == '-') {
        char op = **s;
        (*s)++;
        double right = parse_term(s);
        if (op == '+') val += right;
        else val -= right;
        skip_spaces(s);
    }

    return val;
}

int scenario_event_formula_check(scenario_formula_t *s_formula)
{
    char *s = (char *) s_formula->formatted_calculation;
    s_formula->is_error = 0;
    s_formula->is_static = 1;
    while (*s) {
        if (*s == ',') {
            s_formula->is_static = 0; // found random value
        }
        if (*s == '[') {
            s++; // Move past '['
            if (!isdigit(*s)) { // Check if there's at least one digit
                s_formula->is_error = 1;
                return 0; // Invalid: no number after [
            }
            int variable_id = 0;
            while (isdigit(*s)) { // Parse the variable ID
                variable_id = variable_id * 10 + (*s - '0');
                s++;
            }
            if (*s != ']') { // Check for closing bracket
                s_formula->is_error = 1;
                return 0; // Invalid: missing ] or non-digit character
            }
            if (!scenario_custom_variable_exists(variable_id)) { // Check if variable exists
                s_formula->is_error = 1;
                return 0; // Invalid: variable doesn't exist
            }
            s_formula->is_static = 0; // Found a variable
            s++; // Skip the ]
        } else if (*s == ']') {
            s_formula->is_error = 1;
            return 0; // Invalid: ] without matching [
        } else {
            s++;
        }
    }
    if (s_formula->is_static) {
        // Evaluate static formula once
        int evaluation = formula_evaluate((const char *) s_formula->formatted_calculation);
        evaluation = CLAMP(evaluation, s_formula->min_evaluation, s_formula->max_evaluation);
        s_formula->evaluation = evaluation;
    }
    return 1; // Valid 
}

static int formula_evaluate(const char *str)
{
    double result = parse_expr(&str);
    // round() from <math.h> gives nearest integer (e.g. 4.5 -> 5)
    return (int) round(result);
}

int scenario_event_formula_evaluate(scenario_formula_t *s_formula)
{
    if (s_formula->is_error) {
        return 0;
    }
    if (s_formula->is_static) {
        return s_formula->evaluation;
    }
    int evaluation = formula_evaluate((const char *) s_formula->formatted_calculation);
    evaluation = CLAMP(evaluation, s_formula->min_evaluation, s_formula->max_evaluation);
    s_formula->evaluation = evaluation;
    return evaluation;
}

int scenario_event_formula_is_static(unsigned int id)
{
    scenario_formula_t *form = scenario_formula_get(id);
    return form->is_static;
}

int scenario_event_formula_is_error(unsigned int id)
{
    scenario_formula_t *form = scenario_formula_get(id);
    return form->is_error;
}
