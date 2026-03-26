#include "render/culling.h"

#include <algorithm>
#include <cmath>

namespace
{
FrustumPlane NormalizePlane(FrustumPlane plane)
{
    float length = Vec3Length(plane.normal);
    if (length <= 1e-5f)
    {
        return plane;
    }

    float invLength = 1.0f / length;
    plane.normal = Vec3Scale(plane.normal, invLength);
    plane.distance *= invLength;
    return plane;
}

FrustumPlane MakePlane(float a, float b, float c, float d)
{
    return NormalizePlane(FrustumPlane{Vec3Make(a, b, c), d});
}
}

Frustum ExtractFrustum(Mat4 viewProj)
{
    const float r0x = viewProj.m[0];
    const float r0y = viewProj.m[4];
    const float r0z = viewProj.m[8];
    const float r0w = viewProj.m[12];
    const float r1x = viewProj.m[1];
    const float r1y = viewProj.m[5];
    const float r1z = viewProj.m[9];
    const float r1w = viewProj.m[13];
    const float r2x = viewProj.m[2];
    const float r2y = viewProj.m[6];
    const float r2z = viewProj.m[10];
    const float r2w = viewProj.m[14];
    const float r3x = viewProj.m[3];
    const float r3y = viewProj.m[7];
    const float r3z = viewProj.m[11];
    const float r3w = viewProj.m[15];

    Frustum frustum{};
    frustum.planes[0] = MakePlane(r3x + r0x, r3y + r0y, r3z + r0z, r3w + r0w);
    frustum.planes[1] = MakePlane(r3x - r0x, r3y - r0y, r3z - r0z, r3w - r0w);
    frustum.planes[2] = MakePlane(r3x + r1x, r3y + r1y, r3z + r1z, r3w + r1w);
    frustum.planes[3] = MakePlane(r3x - r1x, r3y - r1y, r3z - r1z, r3w - r1w);
    frustum.planes[4] = MakePlane(r3x + r2x, r3y + r2y, r3z + r2z, r3w + r2w);
    frustum.planes[5] = MakePlane(r3x - r2x, r3y - r2y, r3z - r2z, r3w - r2w);
    return frustum;
}

Vec3 TransformPoint(Mat4 m, Vec3 p)
{
    Vec4 transformed = Mat4MulVec4(m, Vec4Make(p.x, p.y, p.z, 1.0f));
    return Vec3Make(transformed.x, transformed.y, transformed.z);
}

float MaxAxisScale(Mat4 m)
{
    Vec3 xAxis = Vec3Make(m.m[0], m.m[1], m.m[2]);
    Vec3 yAxis = Vec3Make(m.m[4], m.m[5], m.m[6]);
    Vec3 zAxis = Vec3Make(m.m[8], m.m[9], m.m[10]);
    return std::max({Vec3Length(xAxis), Vec3Length(yAxis), Vec3Length(zAxis)});
}

bool SphereIntersectsFrustum(const Frustum& frustum, Vec3 center, float radius)
{
    for (const FrustumPlane& plane : frustum.planes)
    {
        float signedDistance = Vec3Dot(plane.normal, center) + plane.distance;
        if (signedDistance < -radius)
        {
            return false;
        }
    }
    return true;
}
