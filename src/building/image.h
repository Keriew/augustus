#ifndef BUILDING_IMAGE_H
#define BUILDING_IMAGE_H

#include "building/building.h"

int building_image_get_base_farm_crop(building_type type);

int building_image_get(const building *b);

int building_image_get_for_type(building_type type);

#endif // BUILDING_IMAGE_H
