#pragma once

#include <string>
#include <unordered_map>

#include "math_types.h"

enum class VehicleLightSlot
{
    HeadA = 0,
    HeadB,
    RearA,
    RearB,
};

struct VehicleFrontLightConfig
{
    Vec3 offset = {};
    float yawDegrees = 0.0f;
    float pitchDegrees = -8.0f;
    float range = 12.0f;
};

struct VehicleRearLightConfig
{
    Vec3 offset = {};
    float range = 4.0f;
    float intensity = 2.5f;
};

struct VehicleLightRig
{
    VehicleFrontLightConfig headA = {.offset = {-0.55f, 0.18f, 1.35f}, .yawDegrees = -2.0f, .pitchDegrees = -8.0f, .range = 14.0f};
    VehicleFrontLightConfig headB = {.offset = {0.55f, 0.18f, 1.35f}, .yawDegrees = 2.0f, .pitchDegrees = -8.0f, .range = 14.0f};
    VehicleRearLightConfig rearA = {.offset = {-0.45f, 0.35f, -1.25f}, .range = 3.2f, .intensity = 2.8f};
    VehicleRearLightConfig rearB = {.offset = {0.45f, 0.35f, -1.25f}, .range = 3.2f, .intensity = 2.8f};
};

using VehicleLightRigMap = std::unordered_map<std::string, VehicleLightRig>;
