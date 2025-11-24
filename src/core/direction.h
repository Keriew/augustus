// Guard
#ifndef CORE_DIRECTION_H
#define CORE_DIRECTION_H

#define DIR_MAX_MOVEMENT 9 // Defines the boundary for movement checks

/**
 * @file
 * Direction
 */

 /**
  * Direction constants
  */
typedef enum {
    // Null/Error value
    DIR_NULL = 0,

    // Directions
    DIR_TOP_LEFT = 1,
    DIR_TOP = 2,
    DIR_TOP_RIGHT = 3,
    DIR_LEFT = 4,
    DIR_CENTER = 5,
    DIR_RIGHT = 6,
    DIR_BOTTOM_LEFT = 7,
    DIR_BOTTOM = 8,
    DIR_BOTTOM_RIGHT = 9,

    // Status Flags
    DIR_FIGURE_REROUTE = 10,
    DIR_FIGURE_LOST = 11,
    DIR_FIGURE_ATTACK = 12,
    DIR_AT_DESTINATION = 13,
} direction_type;

// Array containing only the 4 cardinal directions
static const int CARDINAL_DIRECTIONS[4] = {
    DIR_TOP, DIR_LEFT, DIR_RIGHT, DIR_BOTTOM
};

// Array containing only the 4 diagonal directions
static const int DIAGONAL_DIRECTIONS[4] = {
    DIR_TOP_LEFT, DIR_TOP_RIGHT, DIR_BOTTOM_LEFT, DIR_BOTTOM_RIGHT
};

// Array containing the 8 movement directions
static const int MOVEMENT_DIRECTIONS[8] = {
    DIR_TOP_LEFT, DIR_TOP, DIR_TOP_RIGHT,
    DIR_LEFT, DIR_RIGHT,
    DIR_BOTTOM_LEFT, DIR_BOTTOM, DIR_BOTTOM_RIGHT
};

// Structures
typedef struct {
    int x;
    int y;
} point_2d;

// External
const bool is_cardinal_direction(int direction_index);
const bool is_diagonal_direction(int direction_index);
const bool is_valid_direction(int direction_index);

#endif // CORE_DIRECTION_H
