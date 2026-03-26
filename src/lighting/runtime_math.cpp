#include "lighting/runtime_internal.h"

#include <algorithm>
#include <cmath>

Vec3 LerpVec3(Vec3 a, Vec3 b, float t)
{
    return Vec3Make(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t
    );
}

Vec3 NormalizeOrFallback(Vec3 v, Vec3 fallback)
{
    float length = Vec3Length(v);
    if (length <= 1e-5f)
    {
        return fallback;
    }
    return Vec3Scale(v, 1.0f / length);
}

Vec3 RotateYOffset(Vec3 v, float yawDegrees)
{
    float radians = DegreesToRadians(yawDegrees);
    float c = std::cos(radians);
    float s = std::sin(radians);
    return Vec3Make(
        c * v.x + s * v.z,
        v.y,
        -s * v.x + c * v.z
    );
}

Vec3 CameraUpVector(const Camera& camera)
{
    return Vec3Normalize(Vec3Cross(CameraRight(camera), CameraForward(camera)));
}

Mat4 MakeOrthographic(float left, float right, float bottom, float top, float zNear, float zFar)
{
    Mat4 result{};
    result.m[0] = 2.0f / (right - left);
    result.m[5] = 2.0f / (top - bottom);
    result.m[10] = -1.0f / (zFar - zNear);
    result.m[12] = -(right + left) / (right - left);
    result.m[13] = -(top + bottom) / (top - bottom);
    result.m[14] = -zNear / (zFar - zNear);
    result.m[15] = 1.0f;
    return result;
}

ShadowCascadeBounds FitShadowCascadeToCameraSlice(
    const Camera& camera,
    float aspect,
    float fovYRadians,
    float sliceNear,
    float sliceFar,
    Vec3 lightDirToScene,
    float casterDepthPadding,
    float receiverDepthPadding
)
{
    const Vec3 forward = CameraForward(camera);
    const Vec3 right = CameraRight(camera);
    const Vec3 up = CameraUpVector(camera);
    const float tanHalfFovY = std::tan(fovYRadians * 0.5f);

    const Vec3 nearCenter = Vec3Add(camera.position, Vec3Scale(forward, sliceNear));
    const Vec3 farCenter = Vec3Add(camera.position, Vec3Scale(forward, sliceFar));

    const float nearHalfHeight = tanHalfFovY * sliceNear;
    const float nearHalfWidth = nearHalfHeight * aspect;
    const float farHalfHeight = tanHalfFovY * sliceFar;
    const float farHalfWidth = farHalfHeight * aspect;

    std::array<Vec3, 8> corners = {
        Vec3Add(Vec3Add(nearCenter, Vec3Scale(right, nearHalfWidth)), Vec3Scale(up, nearHalfHeight)),
        Vec3Add(Vec3Add(nearCenter, Vec3Scale(right, -nearHalfWidth)), Vec3Scale(up, nearHalfHeight)),
        Vec3Add(Vec3Add(nearCenter, Vec3Scale(right, nearHalfWidth)), Vec3Scale(up, -nearHalfHeight)),
        Vec3Add(Vec3Add(nearCenter, Vec3Scale(right, -nearHalfWidth)), Vec3Scale(up, -nearHalfHeight)),
        Vec3Add(Vec3Add(farCenter, Vec3Scale(right, farHalfWidth)), Vec3Scale(up, farHalfHeight)),
        Vec3Add(Vec3Add(farCenter, Vec3Scale(right, -farHalfWidth)), Vec3Scale(up, farHalfHeight)),
        Vec3Add(Vec3Add(farCenter, Vec3Scale(right, farHalfWidth)), Vec3Scale(up, -farHalfHeight)),
        Vec3Add(Vec3Add(farCenter, Vec3Scale(right, -farHalfWidth)), Vec3Scale(up, -farHalfHeight)),
    };

    std::array<Vec3, 16> fitPoints{};
    for (std::size_t i = 0; i < corners.size(); ++i)
    {
        fitPoints[i] = corners[i];
        fitPoints[i + corners.size()] = Vec3Add(corners[i], Vec3Scale(lightDirToScene, casterDepthPadding));
    }

    Vec3 centroid = {};
    for (const Vec3& point : fitPoints)
    {
        centroid = Vec3Add(centroid, point);
    }
    centroid = Vec3Scale(centroid, 1.0f / static_cast<float>(fitPoints.size()));

    const float eyeOffset = sliceFar + casterDepthPadding;
    const Mat4 lightView = Mat4LookAt(
        Vec3Add(centroid, Vec3Scale(lightDirToScene, eyeOffset)),
        centroid,
        Vec3Make(0.0f, 1.0f, 0.0f)
    );

    float minX = 1e30f;
    float maxX = -1e30f;
    float minY = 1e30f;
    float maxY = -1e30f;
    float minDepth = 1e30f;
    float maxDepth = -1e30f;
    for (const Vec3& point : fitPoints)
    {
        Vec4 lightSpace = Mat4MulVec4(lightView, Vec4Make(point.x, point.y, point.z, 1.0f));
        float depth = -lightSpace.z;
        minX = std::min(minX, lightSpace.x);
        maxX = std::max(maxX, lightSpace.x);
        minY = std::min(minY, lightSpace.y);
        maxY = std::max(maxY, lightSpace.y);
        minDepth = std::min(minDepth, depth);
        maxDepth = std::max(maxDepth, depth);
    }

    const float xyPadding = std::max((sliceFar - sliceNear) * 0.08f, 1.5f);
    ShadowCascadeBounds bounds{};
    bounds.center = centroid;
    bounds.halfWidth = std::max((maxX - minX) * 0.5f + xyPadding, 1.0f);
    bounds.halfHeight = std::max((maxY - minY) * 0.5f + xyPadding, 1.0f);
    bounds.nearDepth = std::max(minDepth - receiverDepthPadding, 0.1f);
    bounds.farDepth = std::max(maxDepth + casterDepthPadding, bounds.nearDepth + 1.0f);
    return bounds;
}

Mat4 MakeShadowViewProj(
    Vec3 center,
    Vec3 lightDirToScene,
    float halfWidth,
    float halfHeight,
    float zNear,
    float zFar
)
{
    Mat4 shadowView = Mat4LookAt(
        Vec3Add(center, Vec3Scale(lightDirToScene, zFar * 0.5f)),
        center,
        Vec3Make(0.0f, 1.0f, 0.0f)
    );
    Mat4 shadowProj = MakeOrthographic(-halfWidth, halfWidth, -halfHeight, halfHeight, zNear, zFar);
    shadowProj.m[5] *= -1.0f;
    return Mat4Mul(shadowProj, shadowView);
}

Mat4 MakePerspectiveShadow(float fovYRadians, float aspect, float zNear, float zFar)
{
    float tanHalfFov = std::tan(fovYRadians * 0.5f);
    Mat4 result{};
    result.m[0] = 1.0f / (aspect * tanHalfFov);
    result.m[5] = -1.0f / tanHalfFov;
    result.m[10] = zFar / (zNear - zFar);
    result.m[11] = -1.0f;
    result.m[14] = (zFar * zNear) / (zNear - zFar);
    return result;
}

Mat4 MakeSpotShadowViewProj(Vec3 position, Vec3 direction, float outerAngleRadians, float range)
{
    Vec3 forward = NormalizeOrFallback(direction, Vec3Make(0.0f, -1.0f, 0.0f));
    Vec3 up = std::abs(Vec3Dot(forward, Vec3Make(0.0f, 1.0f, 0.0f))) > 0.98f
        ? Vec3Make(0.0f, 0.0f, 1.0f)
        : Vec3Make(0.0f, 1.0f, 0.0f);
    Mat4 view = Mat4LookAt(position, Vec3Add(position, forward), up);
    Mat4 proj = MakePerspectiveShadow(outerAngleRadians * 2.1f, 1.0f, 0.2f, std::max(range, 1.0f));
    return Mat4Mul(proj, view);
}

Vec3 VehicleLightDirection(float yawDegrees, float pitchDegrees)
{
    float yaw = DegreesToRadians(yawDegrees);
    float pitch = DegreesToRadians(pitchDegrees);
    float cp = std::cos(pitch);
    return NormalizeOrFallback(
        Vec3Make(std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp),
        Vec3Make(0.0f, -0.15f, 1.0f)
    );
}

Vec3 TransformLightPoint(Mat4 transform, Vec3 point)
{
    Vec4 v = Mat4MulVec4(transform, Vec4Make(point.x, point.y, point.z, 1.0f));
    return Vec3Make(v.x, v.y, v.z);
}

Vec3 TransformLightDirection(Mat4 transform, Vec3 direction)
{
    Vec4 v = Mat4MulVec4(transform, Vec4Make(direction.x, direction.y, direction.z, 0.0f));
    return NormalizeOrFallback(Vec3Make(v.x, v.y, v.z), direction);
}

DayNightSample SampleDayNight(float timeOfDay)
{
    static constexpr std::array<DayNightKey, 5> kKeys = {{
        {0.00f, Vec3{0.02f, 0.03f, 0.06f}, Vec3{0.015f, 0.020f, 0.030f}, Vec3{0.00f, 0.00f, 0.00f}, Vec3{0.35f, 0.42f, 0.55f}},
        {0.24f, Vec3{0.42f, 0.34f, 0.30f}, Vec3{0.040f, 0.032f, 0.030f}, Vec3{1.00f, 0.58f, 0.32f}, Vec3{0.12f, 0.14f, 0.18f}},
        {0.50f, Vec3{0.36f, 0.50f, 0.72f}, Vec3{0.030f, 0.035f, 0.042f}, Vec3{1.00f, 0.96f, 0.88f}, Vec3{0.04f, 0.05f, 0.06f}},
        {0.76f, Vec3{0.48f, 0.25f, 0.20f}, Vec3{0.050f, 0.035f, 0.038f}, Vec3{1.00f, 0.42f, 0.22f}, Vec3{0.10f, 0.12f, 0.16f}},
        {1.00f, Vec3{0.02f, 0.03f, 0.06f}, Vec3{0.015f, 0.020f, 0.030f}, Vec3{0.00f, 0.00f, 0.00f}, Vec3{0.35f, 0.42f, 0.55f}},
    }};

    float t = std::clamp(timeOfDay, 0.0f, 1.0f);
    for (std::size_t i = 0; i + 1 < kKeys.size(); ++i)
    {
        const DayNightKey& a = kKeys[i];
        const DayNightKey& b = kKeys[i + 1];
        if (t >= a.time && t <= b.time)
        {
            float localT = (t - a.time) / std::max(b.time - a.time, 1e-5f);
            return DayNightSample{
                .sky = LerpVec3(a.sky, b.sky, localT),
                .ambient = LerpVec3(a.ambient, b.ambient, localT),
                .sun = LerpVec3(a.sun, b.sun, localT),
                .moon = LerpVec3(a.moon, b.moon, localT),
            };
        }
    }

    return DayNightSample{
        .sky = kKeys.back().sky,
        .ambient = kKeys.back().ambient,
        .sun = kKeys.back().sun,
        .moon = kKeys.back().moon,
    };
}
