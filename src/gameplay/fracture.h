#pragma once

#include <cstdint>

#include "damage/mesh_damage.h"

struct FractureSettings
{
    damage::MeshSettings mesh = {};
    float fireRate = 6.0f;
};
