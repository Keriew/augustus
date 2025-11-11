#ifndef BUILDING_PROPERTIES_H
#define BUILDING_PROPERTIES_H

#include "building/type.h"
#include "city/resource.h"
#include "core/buffer.h"

// MODEL DATA

 /**
  * Building model
  */
typedef struct {
    int cost; /**< Cost of structure or of one tile of a structure (for walls) */
    int desirability_value; /**< Initial desirability value */
    int desirability_step; /**< Desirability step (in tiles) */
    int desirability_step_size; /**< Desirability step size */
    int desirability_range; /**< Max desirability range */
    int laborers; /**< Number of people a building employs (max) */
} model_building;

/**
 * House model
 */
typedef struct {
    int devolve_desirability; /**< Desirability at which the house devolves */
    int evolve_desirability; /**< Desirability at which the house evolves */
    int entertainment; /**< Entertainment points required */
    int water; /**< Water required: 1 = well, 2 = fountain */
    int religion; /**< Number of gods required */
    int education; /**< Education required:
        1 = school or library, 2 = school and library, 3 = school, library and academy */
    int barber; /**< Barber required (boolean) */
    int bathhouse; /**< Bathhouse required (boolean) */
    int health; /**< Health required: 1 = doctor or hospital, 2 = doctor and hospital */
    int food_types; /**< Number of food types required */
    int pottery; /**< Pottery required */
    int oil; /**< Oil required */
    int furniture; /**< Furniture required */
    int wine; /**< Wine types required: 1 = any wine, 2 = two types of wine */
    int prosperity; /**< Prosperity contribution */
    int max_people; /**< Maximum people per tile (medium insula and lower) or per house (large insula and up) */
    int tax_multiplier; /**< Tax rate multiplier */
} model_house;

enum {
    MODEL_COST,
    MODEL_DESIRABILITY_VALUE,
    MODEL_DESIRABILITY_STEP,
    MODEL_DESIRABILITY_STEP_SIZE,
    MODEL_DESIRABILITY_RANGE,
    MODEL_LABORERS,
};

enum {
    MODEL_DEVOLVE_DESIRABILITY,
    MODEL_EVOLVE_DESIRABILITY,
    MODEL_ENTERTAINMENT,
    MODEL_WATER,
    MODEL_RELIGION,
    MODEL_EDUCATION,
    MODEL_BARBER,
    MODEL_BATHHOUSE,
    MODEL_HEALTH,
    MODEL_FOOD_TYPES,
    MODEL_POTTERY,
    MODEL_OIL,
    MODEL_FURNITURE,
    MODEL_WINE,
    MODEL_PROSPERITY,
    MODEL_MAX_PEOPLE,
    MODEL_TAX_MULTIPLIER
};

/**
 * Resets house model data from properties
 */
void model_reset_houses(void);
/**
 * Resets building model data from properties
 */
void model_reset_buildings(void);
/**
 * Resets all model data from properties
 */
void model_reset(void);

void model_save_model_data(buffer *buf);
void model_load_model_data(buffer *buf);

/**
 * Gets the model for a building
 * @param type Building type
 * @return Read-only model
 */
model_building *model_get_building(building_type type);

/**
 * Gets the model for a house
 * @param level House level
 * @return Read-only model
 */
model_house *model_get_house(house_level level);

/**
 * Checks whether house level requires resource
 * @param level The house level to check
 * @param inventory The resource to check
 * @return 0/1 required/not required
 */
int model_house_uses_inventory(house_level level, resource_type inventory);

// PROPERTIES

typedef struct {
    int size;
    int fire_proof;
    int image_group;
    int image_offset;
    int rotation_offset;
    int sound_id;
    int draw_desirability_range;
    int venus_gt_bonus; // indicator of whether building is part of the 'garden/statue/temple' group
    struct {
        const char *group;
        const char *id;
    } custom_asset;
    struct {
        const char *attr;
        int key;
        int cannot_count;
    } event_data;
    model_building building_model_data;
    model_house house_model_data;
} building_properties;

void building_properties_init(void);
const building_properties *building_properties_for_type(building_type type);

#endif // BUILDING_PROPERTIES_H
