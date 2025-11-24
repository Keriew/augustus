const bool is_cardinal_direction(int direction_index)
{
    switch (direction_index) {
        case DIR_TOP:
        case DIR_LEFT:
        case DIR_RIGHT:
        case DIR_BOTTOM:
            return 1
    }
    return 0
}

const bool is_diagonal_direction(int direction_index)
{
    switch (direction_index) {
        case DIR_TOP_LEFT:
        case DIR_TOP_RIGHT:
        case DIR_BOTTOM_LEFT:
        case DIR_BOTTOM_RIGHT:
            return 1
    }
    return 0
}

const bool is_valid_direction(int direction_index)
{
    if (direction_index >= 1 && direction_index <= 9)
}