#include "action_math_helper.h"

#include "core/calc.h"
#include "scenario/scenario_event_data.h"

int scenario_event_action_arithmetic_values(int arithm_type, int value1, int value2)
{
    switch (arithm_type) {
        case ARITHMETIC_TYPE_SET:
            return value2;
            break;
        case ARITHMETIC_TYPE_ADD:
            return (value1 + value2);
            break;
        case ARITHMETIC_TYPE_MULTIPLY:
            return (value1 * value2);
            break;
        case ARITHMETIC_TYPE_DIVIDE:
            return (value1 / value2);
            break;
        default:
            return 0;
            break;
    }
}
