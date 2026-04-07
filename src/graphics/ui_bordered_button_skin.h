#pragma once

#include "graphics/runtime_texture.h"

#include <array>

class UiPrimitives;

struct UiBorderedButtonSkin {
    bool use_runtime = false;
    int image_base = 0;
    int tile_width = 0;
    int tile_height = 0;
    std::array<RuntimeDrawSlice, 8> slices = {};
};

UiBorderedButtonSkin ui_bordered_button_skin_resolve(const UiPrimitives &primitives, int has_focus);
