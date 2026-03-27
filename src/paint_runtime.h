#pragma once

#include <array>
#include <cstdint>

#include "math_types.h"

struct PaintSplat
{
    Vec3 position = {};
    float radius = 0.35f;
    Vec3 normal = {0.0f, 1.0f, 0.0f};
    float pad0 = 0.0f;
    Vec3 color = {1.0f, 0.2f, 0.75f};
    float pad1 = 0.0f;
};
