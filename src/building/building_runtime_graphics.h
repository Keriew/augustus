#pragma once

#include "graphics/runtime_texture.h"

struct building;

const RuntimeDrawSlice *building_runtime_get_graphic_footprint_slice(building *b);
const RuntimeDrawSlice *building_runtime_get_graphic_top_slice(building *b);
int building_runtime_owns_graphics(building *b);
int building_runtime_owns_graphic_animation(building *b);
int building_runtime_get_graphic_animation_layer_count(building *b);
const RuntimeDrawSlice *building_runtime_get_graphic_animation_layer_frame(building *b, int layer_index);
