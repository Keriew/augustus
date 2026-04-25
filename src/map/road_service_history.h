#ifndef MAP_ROAD_SERVICE_HISTORY_H
#define MAP_ROAD_SERVICE_HISTORY_H

#include "core/buffer.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ROAD_SERVICE_EFFECT_NONE = 0,
    ROAD_SERVICE_EFFECT_LABOR = 1,
    ROAD_SERVICE_EFFECT_ACADEMY = 2,
    ROAD_SERVICE_EFFECT_LIBRARY = 3,
    ROAD_SERVICE_EFFECT_BARBER = 4,
    ROAD_SERVICE_EFFECT_BATHHOUSE = 5,
    ROAD_SERVICE_EFFECT_SCHOOL = 6,
    ROAD_SERVICE_EFFECT_DAMAGE_RISK = 7,
    ROAD_SERVICE_EFFECT_FIRE_RISK = 8,
    ROAD_SERVICE_EFFECT_RELIGION_CERES = 9,
    ROAD_SERVICE_EFFECT_RELIGION_NEPTUNE = 10,
    ROAD_SERVICE_EFFECT_RELIGION_MERCURY = 11,
    ROAD_SERVICE_EFFECT_RELIGION_MARS = 12,
    ROAD_SERVICE_EFFECT_RELIGION_VENUS = 13,
    ROAD_SERVICE_EFFECT_RELIGION_PANTHEON = 14,
    ROAD_SERVICE_EFFECT_MAX = 15
} road_service_effect;

/* Clears all pathing-only service visit stamps for a fresh city/load. */
void map_road_service_history_clear(void);

/* Returns zero for never-visited roads, invalid effects, and invalid offsets. */
uint32_t map_road_service_history_get(road_service_effect effect, int grid_offset);

/* Records the current game-time stamp for a smart service walker on one road tile. */
void map_road_service_history_record(road_service_effect effect, int grid_offset);

/* Saves one full per-effect road grid. Effect ids are an append-only save contract. */
void map_road_service_history_save_state(buffer *buf);

/* Loads history if present; missing or newer effect ids start with zeroed history. */
void map_road_service_history_load_state(buffer *buf, int has_saved_state, int has_religion_effects);

#ifdef __cplusplus
}
#endif

#endif // MAP_ROAD_SERVICE_HISTORY_H
