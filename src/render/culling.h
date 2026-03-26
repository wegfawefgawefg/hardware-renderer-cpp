#pragma once

#include <array>

#include "math_types.h"

struct FrustumPlane
{
    Vec3 normal = {};
    float distance = 0.0f;
};

struct Frustum
{
    std::array<FrustumPlane, 6> planes = {};
};

Frustum ExtractFrustum(Mat4 viewProj);
Vec3 TransformPoint(Mat4 m, Vec3 p);
float MaxAxisScale(Mat4 m);
bool SphereIntersectsFrustum(const Frustum& frustum, Vec3 center, float radius);
