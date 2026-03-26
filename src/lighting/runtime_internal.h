#pragma once

#include <array>
#include <limits>
#include <vector>

#include "state.h"

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

struct LightCandidate
{
    float dist2 = 1e30f;
    std::uint32_t lightIndex = std::numeric_limits<std::uint32_t>::max();
};

struct RearPointCandidate
{
    float dist2 = 1e30f;
    Vec3 position = {};
    Vec3 color = {};
    float range = 0.0f;
};

Vec3 LerpVec3(Vec3 a, Vec3 b, float t);
Vec3 NormalizeOrFallback(Vec3 v, Vec3 fallback);
Vec3 RotateYOffset(Vec3 v, float yawDegrees);
Vec3 CameraUpVector(const Camera& camera);
Mat4 MakeOrthographic(float left, float right, float bottom, float top, float zNear, float zFar);
ShadowCascadeBounds FitShadowCascadeToCameraSlice(
    const Camera& camera,
    float aspect,
    float fovYRadians,
    float sliceNear,
    float sliceFar,
    Vec3 lightDirToScene,
    float casterDepthPadding,
    float receiverDepthPadding
);
Mat4 MakeShadowViewProj(
    Vec3 center,
    Vec3 lightDirToScene,
    float halfWidth,
    float halfHeight,
    float zNear,
    float zFar
);
Mat4 MakePerspectiveShadow(float fovYRadians, float aspect, float zNear, float zFar);
Mat4 MakeSpotShadowViewProj(Vec3 position, Vec3 direction, float outerAngleRadians, float range);
Vec3 VehicleLightDirection(float yawDegrees, float pitchDegrees);
Vec3 TransformLightPoint(Mat4 transform, Vec3 point);
Vec3 TransformLightDirection(Mat4 transform, Vec3 direction);
DayNightSample SampleDayNight(float timeOfDay);

void CollectCityVehicleLights(
    const State& state,
    std::vector<SpotLightData>& spotLightSources,
    std::vector<RearPointCandidate>& rearPointCandidates
);

bool PopulateVehicleLightTestLighting(State& state, SceneUniforms& uniforms);
void PopulateFallbackOrbitPointLights(const State& state, SceneUniforms& uniforms, Vec3 center);
