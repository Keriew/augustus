#include "comparison_helper.h"

#include "scenario/scenario_event_data.h"

int scenario_event_condition_compare_values(int compare_type, int value1, int value2)
{
    switch (compare_type) {
        case COMPARISON_TYPE_EQUAL:
            return (value1 == value2);
            break;
        case COMPARISON_TYPE_EQUAL_OR_LESS:
            return (value1 <= value2);
            break;
        case COMPARISON_TYPE_EQUAL_OR_MORE:
            return (value1 >= value2);
            break;
        default:
            return 0;
            break;
    }
}
