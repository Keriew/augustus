#include "scenario/custom_variable.h"

#include <stdio.h>
#include <ctype.h>

int parse_expr(const char **s);

int get_var_value(int id)
{
    int var_value = scenario_custom_variable_get_value(id);
    return var_value;
}

void skip_spaces(const char **s)
{
    while (isspace(**s)) (*s)++;
}

int parse_number(const char **s)
{
    int val = 0;
    while (isdigit(**s)) {
        val = val * 10 + (**s - '0');
        (*s)++;
    }
    return val;
}

int parse_factor(const char **s)
{
    skip_spaces(s);

    if (**s == '(') {
        (*s)++;
        int val = parse_expr(s);
        if (**s == ')') (*s)++;
        return val;
    } else if (**s == '[') {
        (*s)++;
        int id = parse_number(s);
        if (**s == ']') (*s)++;
        return get_var_value(id);
    } else if (**s == '-') { // unary minus
        (*s)++;
        return -parse_factor(s);
    } else if (isdigit(**s)) {
        return parse_number(s);
    }

    return 0; // fallback if unexpected input
}

int parse_term(const char **s)
{
    int val = parse_factor(s);
    skip_spaces(s);

    while (**s == '*' || **s == '/') {
        char op = **s;
        (*s)++;
        int right = parse_factor(s);
        if (op == '*') val *= right;
        else if (right != 0) val /= right;
        skip_spaces(s);
    }

    return val;
}

int parse_expr(const char **s)
{
    int val = parse_term(s);
    skip_spaces(s);

    while (**s == '+' || **s == '-') {
        char op = **s;
        (*s)++;
        int right = parse_term(s);
        if (op == '+') val += right;
        else val -= right;
        skip_spaces(s);
    }

    return val;
}

int scenario_event_formula_evaluate(const char *str)
{
    return parse_expr(&str);
}

