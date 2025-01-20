#ifndef BUILDING_PROPERTIES_H
#define BUILDING_PROPERTIES_H

#include "building/type.h"

typedef struct {
    int size;
    int fire_proof;
    int disallowable;
    int image_group;
    int image_offset;
    int rotation_offset;
    struct {
        const char *group;
        const char *id;
    } custom_asset;
} building_properties;

void building_properties_init(void);
const building_properties *building_properties_for_type(building_type type);

#endif // BUILDING_PROPERTIES_H
