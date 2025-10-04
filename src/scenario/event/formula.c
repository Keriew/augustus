#include "scenario/custom_variable.h"

#include <stdio.h>
#include <ctype.h>
#include <math.h> // for round()

double parse_expr(const char **s);

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

int scenario_event_formula_evaluate(const char *str)
{
    double result = parse_expr(&str);
    // round() from <math.h> gives nearest integer (e.g. 4.5 -> 5)
    return (int) round(result);
}
