#ifndef CORE_DIRECTION_H
#define CORE_DIRECTION_H
#define DIR_MAX_MOVEMENT 9

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

    // Cardinal directions
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
    
    // Defines the boundary for movement checks
    DIR_MAX_MOVEMENT = 9 
} direction_type;

#endif // CORE_DIRECTION_H
