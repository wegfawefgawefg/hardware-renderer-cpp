#include "app.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <vector>

namespace
{
struct ShadowCascadeBounds
{
    Vec3 center = {};
    float halfWidth = 1.0f;
    float halfHeight = 1.0f;
    float nearDepth = 0.1f;
    float farDepth = 10.0f;
};

struct DayNightSample
{
    Vec3 sky = {};
    Vec3 ambient = {};
    Vec3 sun = {};
    Vec3 moon = {};
};

struct DayNightKey
{
    float time = 0.0f;
    Vec3 sky = {};
    Vec3 ambient = {};
    Vec3 sun = {};
    Vec3 moon = {};
};

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
}

void App::ApplyLighting(SceneUniforms& uniforms, float dtSeconds)
{
    for (std::uint32_t i = 0; i < kMaxSceneSpotLights; ++i)
    {
        uniforms.spotLightPositions[i] = Vec4Make(0.0f, 0.0f, 0.0f, 0.0f);
        uniforms.spotLightDirections[i] = Vec4Make(0.0f, -1.0f, 0.0f, 0.0f);
        uniforms.spotLightColors[i] = Vec4Make(0.0f, 0.0f, 0.0f, 0.0f);
        uniforms.spotLightParams[i] = Vec4Make(0.0f, 0.0f, 0.0f, 0.0f);
    }
    for (std::uint32_t i = 0; i < kMaxShadowedSpotLights; ++i)
    {
        uniforms.shadowedSpotLightPositions[i] = Vec4Make(0.0f, 0.0f, 0.0f, 0.0f);
        uniforms.shadowedSpotLightDirections[i] = Vec4Make(0.0f, -1.0f, 0.0f, 0.0f);
        uniforms.shadowedSpotLightColors[i] = Vec4Make(0.0f, 0.0f, 0.0f, 0.0f);
        uniforms.shadowedSpotLightParams[i] = Vec4Make(0.0f, 0.0f, 0.0f, 0.0f);
        uniforms.shadowViewProj[kSunShadowCascadeCount + i] = Mat4Identity();
    }
    uniforms.sceneLightCounts = Vec4Make(0.0f, 0.0f, 0.0f, 0.0f);

    if (m_cycleDayNight)
    {
        m_timeOfDay += dtSeconds * m_dayNightSpeed;
        if (m_timeOfDay >= 1.0f)
        {
            m_timeOfDay -= std::floor(m_timeOfDay);
        }
    }
    else
    {
        m_timeOfDay = std::clamp(m_timeOfDay, 0.0f, 1.0f);
    }

    DayNightSample sample = SampleDayNight(m_timeOfDay);
    Vec3 center = m_sceneBounds.valid ? m_sceneBounds.center : Vec3Make(0.0f, 0.0f, 0.0f);
    float orbitRadius = (m_sceneBounds.valid ? m_sceneBounds.radius : 30.0f) * m_orbitDistanceScale;
    float solarAngle = (m_timeOfDay - 0.25f) * 6.28318530718f;
    float azimuth = DegreesToRadians(m_sunAzimuthDegrees);
    Vec3 horizontal = Vec3Make(std::sin(azimuth), 0.0f, std::cos(azimuth));
    Vec3 sunDir = NormalizeOrFallback(
        Vec3Add(Vec3Scale(horizontal, std::cos(solarAngle)), Vec3Make(0.0f, std::sin(solarAngle), 0.0f)),
        Vec3Make(0.0f, 1.0f, 0.0f)
    );

    m_sunWorldPosition = Vec3Add(center, Vec3Scale(sunDir, orbitRadius));
    m_moonWorldPosition = Vec3Add(center, Vec3Scale(sunDir, -orbitRadius));

    const bool sunAboveHorizon = sunDir.y >= 0.0f;
    Vec3 keyDirToLight = sunAboveHorizon ? sunDir : Vec3Scale(sunDir, -1.0f);
    Vec3 keyColor = sunAboveHorizon
        ? Vec3Scale(sample.sun, m_sunIntensity)
        : Vec3Scale(sample.moon, m_moonIntensity);

    uniforms.sunDirection = Vec4Make(-keyDirToLight.x, -keyDirToLight.y, -keyDirToLight.z, 0.0f);
    uniforms.sunColor = Vec4Make(keyColor.x, keyColor.y, keyColor.z, 1.0f);

    Vec3 ambient = Vec3Scale(sample.ambient, m_ambientIntensity);
    uniforms.ambientColor = Vec4Make(ambient.x, ambient.y, ambient.z, 1.0f);
    uniforms.clearColor = Vec4Make(sample.sky.x, sample.sky.y, sample.sky.z, 1.0f);

    uniforms.celestialPositions[0] = Vec4Make(m_sunWorldPosition.x, m_sunWorldPosition.y, m_sunWorldPosition.z, 1.0f);
    uniforms.celestialPositions[1] = Vec4Make(m_moonWorldPosition.x, m_moonWorldPosition.y, m_moonWorldPosition.z, 1.0f);
    uniforms.celestialColors[0] = Vec4Make(sample.sun.x * 3.0f, sample.sun.y * 3.0f, sample.sun.z * 3.0f, 1.0f);
    uniforms.celestialColors[1] = Vec4Make(sample.moon.x * 2.0f, sample.moon.y * 2.0f, sample.moon.z * 2.0f, 1.0f);

    struct LightCandidate
    {
        float dist2 = 1e30f;
        std::uint32_t lightIndex = std::numeric_limits<std::uint32_t>::max();
    };
    std::array<LightCandidate, kMaxSceneSpotLights> selectedLights{};
    std::array<LightCandidate, kMaxShadowedSpotLights> shadowedLights{};
    for (LightCandidate& candidate : selectedLights)
    {
        candidate.dist2 = 1e30f;
        candidate.lightIndex = std::numeric_limits<std::uint32_t>::max();
    }
    for (LightCandidate& candidate : shadowedLights)
    {
        candidate.dist2 = 1e30f;
        candidate.lightIndex = std::numeric_limits<std::uint32_t>::max();
    }
    std::uint32_t activeBudget = std::min(m_spotLightMaxActive, kMaxSceneSpotLights);
    std::uint32_t shadowedBudget = std::min(m_shadowedSpotLightMaxActive, kMaxShadowedSpotLights);
    float activationDistance2 = m_spotLightActivationDistance * m_spotLightActivationDistance;
    float shadowedActivationDistance2 = m_shadowedSpotLightActivationDistance * m_shadowedSpotLightActivationDistance;
    Vec3 activationCenter = Vec3Add(
        m_camera.position,
        Vec3Scale(CameraForward(m_camera), m_spotLightActivationForwardOffset)
    );
    Vec3 shadowedActivationCenter = Vec3Add(
        m_camera.position,
        Vec3Scale(CameraForward(m_camera), m_shadowedSpotLightActivationForwardOffset)
    );
    std::vector<bool> shadowedMask(m_scene.spotLights.size(), false);

    for (std::uint32_t lightIndex = 0; lightIndex < m_scene.spotLights.size(); ++lightIndex)
    {
        const SpotLightData& light = m_scene.spotLights[lightIndex];
        Vec3 lightPosition = Vec3Add(light.position, RotateYOffset(m_spotLightSourceOffset, light.yawDegrees));
        Vec3 shadowedDelta = Vec3Sub(lightPosition, shadowedActivationCenter);
        float shadowedDist2 = Vec3Dot(shadowedDelta, shadowedDelta);
        if (shadowedDist2 <= shadowedActivationDistance2)
        {
            for (std::size_t i = 0; i < shadowedBudget; ++i)
            {
                if (shadowedDist2 >= shadowedLights[i].dist2)
                {
                    continue;
                }

                for (std::size_t j = shadowedBudget - 1; j > i; --j)
                {
                    shadowedLights[j] = shadowedLights[j - 1];
                }
                shadowedLights[i] = LightCandidate{shadowedDist2, lightIndex};
                break;
            }
        }

        Vec3 delta = Vec3Sub(lightPosition, activationCenter);
        float dist2 = Vec3Dot(delta, delta);
        if (dist2 > activationDistance2)
        {
            continue;
        }

        for (std::size_t i = 0; i < activeBudget; ++i)
        {
            if (dist2 >= selectedLights[i].dist2)
            {
                continue;
            }

            for (std::size_t j = activeBudget - 1; j > i; --j)
            {
                selectedLights[j] = selectedLights[j - 1];
            }
            selectedLights[i] = LightCandidate{dist2, lightIndex};
            break;
        }
    }

    for (std::size_t i = 0; i < shadowedBudget; ++i)
    {
        if (shadowedLights[i].lightIndex < shadowedMask.size())
        {
            shadowedMask[shadowedLights[i].lightIndex] = true;
        }
    }

    float sceneLightScale = (m_sceneKind == SceneKind::ShadowTest || m_sceneKind == SceneKind::SpotShadowTest)
        ? 1.0f
        : std::clamp((0.15f - sunDir.y) / 0.35f, 0.0f, 1.0f);
    float innerCos = std::cos(DegreesToRadians(m_spotLightInnerAngleDegrees));
    float outerCos = std::cos(DegreesToRadians(m_spotLightOuterAngleDegrees));
    std::uint32_t shadowedSpotCount = 0;
    for (std::size_t i = 0; i < shadowedBudget; ++i)
    {
        if (shadowedLights[i].lightIndex >= m_scene.spotLights.size())
        {
            continue;
        }

        const SpotLightData& light = m_scene.spotLights[shadowedLights[i].lightIndex];
        if (light.intensity * sceneLightScale * m_spotLightIntensityScale <= 0.001f)
        {
            continue;
        }

        Vec3 lightPosition = Vec3Add(light.position, RotateYOffset(m_spotLightSourceOffset, light.yawDegrees));
        Vec3 lightDirection = light.direction;
        if (m_sceneKind == SceneKind::ShadowTest &&
            shadowedLights[i].lightIndex == 0 &&
            m_shadowTestSpotTargetValid)
        {
            lightDirection = NormalizeOrFallback(
                Vec3Sub(m_shadowTestSpotTargetWorld, lightPosition),
                light.direction
            );
        }

        float range = light.range * m_spotLightRangeScale;
        uniforms.shadowedSpotLightPositions[shadowedSpotCount] = Vec4Make(
            lightPosition.x,
            lightPosition.y,
            lightPosition.z,
            range
        );
        uniforms.shadowedSpotLightDirections[shadowedSpotCount] = Vec4Make(
            lightDirection.x,
            lightDirection.y,
            lightDirection.z,
            light.outerCos
        );
        uniforms.shadowedSpotLightColors[shadowedSpotCount] = Vec4Make(
            light.color.x,
            light.color.y,
            light.color.z,
            light.intensity * sceneLightScale * m_spotLightIntensityScale
        );
        uniforms.shadowedSpotLightParams[shadowedSpotCount] = Vec4Make(innerCos, outerCos, 0.0f, 0.0f);
        uniforms.shadowViewProj[kSunShadowCascadeCount + shadowedSpotCount] = MakeSpotShadowViewProj(
            lightPosition,
            lightDirection,
            DegreesToRadians(m_spotLightOuterAngleDegrees),
            range
        );
        ++shadowedSpotCount;
    }

    std::uint32_t sceneLightCount = 0;
    for (std::size_t i = 0; i < activeBudget; ++i)
    {
        if (selectedLights[i].lightIndex >= m_scene.spotLights.size() ||
            shadowedMask[selectedLights[i].lightIndex])
        {
            continue;
        }

        const SpotLightData& light = m_scene.spotLights[selectedLights[i].lightIndex];
        if (light.intensity * sceneLightScale * m_spotLightIntensityScale <= 0.001f)
        {
            continue;
        }

        Vec3 lightPosition = Vec3Add(light.position, RotateYOffset(m_spotLightSourceOffset, light.yawDegrees));
        Vec3 lightDirection = light.direction;
        if (m_sceneKind == SceneKind::ShadowTest &&
            selectedLights[i].lightIndex == 0 &&
            m_shadowTestSpotTargetValid)
        {
            lightDirection = NormalizeOrFallback(
                Vec3Sub(m_shadowTestSpotTargetWorld, lightPosition),
                light.direction
            );
        }

        uniforms.spotLightPositions[sceneLightCount] = Vec4Make(
            lightPosition.x,
            lightPosition.y,
            lightPosition.z,
            light.range * m_spotLightRangeScale
        );
        uniforms.spotLightDirections[sceneLightCount] = Vec4Make(
            lightDirection.x,
            lightDirection.y,
            lightDirection.z,
            light.outerCos
        );
        uniforms.spotLightColors[sceneLightCount] = Vec4Make(
            light.color.x,
            light.color.y,
            light.color.z,
            light.intensity * sceneLightScale * m_spotLightIntensityScale
        );
        uniforms.spotLightParams[sceneLightCount] = Vec4Make(innerCos, outerCos, 0.0f, 0.0f);
        ++sceneLightCount;
    }
    uniforms.sceneLightCounts = Vec4Make(
        static_cast<float>(sceneLightCount),
        static_cast<float>(shadowedSpotCount),
        0.0f,
        0.0f
    );

    float cameraNear = 0.1f;
    float cameraFar = 200.0f;
    float cameraAspect = static_cast<float>(std::max(m_windowWidth, 1u)) / static_cast<float>(std::max(m_windowHeight, 1u));
    float cameraFovY = DegreesToRadians(60.0f);
    float splitDistance = std::clamp(m_shadowCascadeSplit, 8.0f, cameraFar - 10.0f);
    float sceneRadius = m_sceneBounds.valid ? m_sceneBounds.radius : 24.0f;

    ShadowCascadeBounds nearCascade = FitShadowCascadeToCameraSlice(
        m_camera,
        cameraAspect,
        cameraFovY,
        cameraNear,
        splitDistance,
        keyDirToLight,
        std::max(splitDistance * 1.2f, 18.0f),
        std::max(splitDistance * 0.45f, 6.0f)
    );
    uniforms.shadowViewProj[0] = MakeShadowViewProj(
        nearCascade.center,
        keyDirToLight,
        nearCascade.halfWidth,
        nearCascade.halfHeight,
        nearCascade.nearDepth,
        nearCascade.farDepth
    );

    float farExtent = std::max(sceneRadius * 1.15f, splitDistance * 1.75f);
    uniforms.shadowViewProj[1] = MakeShadowViewProj(
        center,
        keyDirToLight,
        farExtent,
        farExtent,
        0.1f,
        farExtent * 3.0f
    );
    float cascadeBlendWidth = std::max(splitDistance * 0.12f, 2.0f);
    uniforms.shadowParams = Vec4Make(m_shadowBlur ? 1.0f : 0.0f, splitDistance, cascadeBlendWidth, 0.0f);

    float t = m_elapsedSeconds;
    float pointScale = m_pointLightIntensity;
    Vec3 lightCenter = center;
    float orbit = std::max((m_sceneBounds.valid ? m_sceneBounds.radius : 20.0f) * 0.6f, 6.0f);
    float highY = center.y + std::max((m_sceneBounds.valid ? m_sceneBounds.radius : 20.0f) * 0.4f, 4.0f);
    float midY = center.y + std::max((m_sceneBounds.valid ? m_sceneBounds.radius : 20.0f) * 0.22f, 2.5f);
    float lowY = center.y + std::max((m_sceneBounds.valid ? m_sceneBounds.radius : 20.0f) * 0.15f, 1.5f);

    uniforms.lightPositions[0] = Vec4Make(lightCenter.x + std::cos(t * 0.8f) * orbit, highY, lightCenter.z + std::sin(t * 0.8f) * orbit, 1.0f);
    uniforms.lightColors[0] = Vec4Make(5.0f * pointScale, 4.8f * pointScale, 4.5f * pointScale, 1.0f);
    uniforms.lightPositions[1] = Vec4Make(lightCenter.x - orbit * 0.8f, midY + std::sin(t * 2.1f) * 0.5f, lightCenter.z + std::cos(t * 0.9f) * orbit * 0.55f, 1.0f);
    uniforms.lightColors[1] = Vec4Make(4.8f * pointScale, 0.45f * pointScale, 0.45f * pointScale, 1.0f);
    uniforms.lightPositions[2] = Vec4Make(lightCenter.x + std::cos(t * 1.1f) * orbit * 0.55f, lowY + std::cos(t * 1.8f) * 0.6f, lightCenter.z - orbit * 0.75f, 1.0f);
    uniforms.lightColors[2] = Vec4Make(0.45f * pointScale, 4.4f * pointScale, 0.55f * pointScale, 1.0f);
    uniforms.lightPositions[3] = Vec4Make(lightCenter.x + orbit * 0.8f, midY + std::sin(t * 1.5f) * 0.55f, lightCenter.z + std::sin(t * 1.0f) * orbit * 0.62f, 1.0f);
    uniforms.lightColors[3] = Vec4Make(0.55f * pointScale, 0.9f * pointScale, 4.8f * pointScale, 1.0f);
}
