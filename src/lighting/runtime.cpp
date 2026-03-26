#include "lighting/runtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

#include "lighting/runtime_internal.h"

void ApplyRuntimeLighting(State& state, SceneUniforms& uniforms, float dtSeconds)
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

    LightingState& lighting = state.lighting;
    CoreState& core = state.core;
    RuntimeState& runtime = state.runtime;

    if (lighting.cycleDayNight)
    {
        lighting.timeOfDay += dtSeconds * lighting.dayNightSpeed;
        if (lighting.timeOfDay >= 1.0f)
        {
            lighting.timeOfDay -= std::floor(lighting.timeOfDay);
        }
    }
    else
    {
        lighting.timeOfDay = std::clamp(lighting.timeOfDay, 0.0f, 1.0f);
    }

    DayNightSample sample = SampleDayNight(lighting.timeOfDay);
    Vec3 center = core.sceneBounds.valid ? core.sceneBounds.center : Vec3Make(0.0f, 0.0f, 0.0f);
    float orbitRadius = (core.sceneBounds.valid ? core.sceneBounds.radius : 30.0f) * lighting.orbitDistanceScale;
    float solarAngle = (lighting.timeOfDay - 0.25f) * 6.28318530718f;
    float azimuthDegrees = lighting.sunAzimuthDegrees;
    if (lighting.animateSunAzimuth)
    {
        azimuthDegrees += (lighting.timeOfDay - 0.5f) * 180.0f;
    }
    float azimuth = DegreesToRadians(azimuthDegrees);
    Vec3 horizontal = Vec3Make(std::sin(azimuth), 0.0f, std::cos(azimuth));
    Vec3 sunDir = NormalizeOrFallback(
        Vec3Add(Vec3Scale(horizontal, std::cos(solarAngle)), Vec3Make(0.0f, std::sin(solarAngle), 0.0f)),
        Vec3Make(0.0f, 1.0f, 0.0f)
    );

    lighting.sunWorldPosition = Vec3Add(center, Vec3Scale(sunDir, orbitRadius));
    lighting.moonWorldPosition = Vec3Add(center, Vec3Scale(sunDir, -orbitRadius));

    const bool sunAboveHorizon = sunDir.y >= 0.0f;
    Vec3 keyDirToLight = sunAboveHorizon ? sunDir : Vec3Scale(sunDir, -1.0f);
    Vec3 keyColor = sunAboveHorizon
        ? Vec3Scale(sample.sun, lighting.sunIntensity)
        : Vec3Scale(sample.moon, lighting.moonIntensity);

    uniforms.sunDirection = Vec4Make(-keyDirToLight.x, -keyDirToLight.y, -keyDirToLight.z, 0.0f);
    uniforms.sunColor = Vec4Make(keyColor.x, keyColor.y, keyColor.z, 1.0f);

    Vec3 ambient = Vec3Scale(sample.ambient, lighting.ambientIntensity);
    uniforms.ambientColor = Vec4Make(ambient.x, ambient.y, ambient.z, 1.0f);
    uniforms.clearColor = Vec4Make(sample.sky.x, sample.sky.y, sample.sky.z, 1.0f);
    uniforms.celestialPositions[0] = Vec4Make(lighting.sunWorldPosition.x, lighting.sunWorldPosition.y, lighting.sunWorldPosition.z, 1.0f);
    uniforms.celestialPositions[1] = Vec4Make(lighting.moonWorldPosition.x, lighting.moonWorldPosition.y, lighting.moonWorldPosition.z, 1.0f);
    uniforms.celestialColors[0] = Vec4Make(sample.sun.x * 3.0f, sample.sun.y * 3.0f, sample.sun.z * 3.0f, 1.0f);
    uniforms.celestialColors[1] = Vec4Make(sample.moon.x * 2.0f, sample.moon.y * 2.0f, sample.moon.z * 2.0f, 1.0f);

    std::array<LightCandidate, kMaxSceneSpotLights> selectedLights{};
    std::array<LightCandidate, kMaxShadowedSpotLights> shadowedLights{};
    std::uint32_t activeBudget = std::min(lighting.spotLightMaxActive, kMaxSceneSpotLights);
    std::uint32_t shadowedBudget = std::min(lighting.shadowedSpotLightMaxActive, kMaxShadowedSpotLights);
    float activationDistance2 = lighting.spotLightActivationDistance * lighting.spotLightActivationDistance;
    float shadowedActivationDistance2 =
        lighting.shadowedSpotLightActivationDistance * lighting.shadowedSpotLightActivationDistance;
    Vec3 activationCenter = Vec3Add(
        core.camera.position,
        Vec3Scale(CameraForward(core.camera), lighting.spotLightActivationForwardOffset)
    );
    Vec3 shadowedActivationCenter = Vec3Add(
        core.camera.position,
        Vec3Scale(CameraForward(core.camera), lighting.shadowedSpotLightActivationForwardOffset)
    );

    std::vector<SpotLightData> spotLightSources = core.scene.spotLights;
    std::vector<RearPointCandidate> rearPointCandidates;
    if (lighting.sceneKind == SceneKind::City)
    {
        CollectCityVehicleLights(state, spotLightSources, rearPointCandidates);
    }

    std::vector<bool> shadowedMask(spotLightSources.size(), false);
    for (std::uint32_t lightIndex = 0; lightIndex < spotLightSources.size(); ++lightIndex)
    {
        const SpotLightData& light = spotLightSources[lightIndex];
        Vec3 sourceOffset = Vec3Scale(lighting.spotLightSourceOffset, light.sourceOffsetScale);
        Vec3 lightPosition = Vec3Add(light.position, RotateYOffset(sourceOffset, light.yawDegrees));

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

    float sceneLightScale = (lighting.sceneKind == SceneKind::ShadowTest || lighting.sceneKind == SceneKind::SpotShadowTest)
        ? 1.0f
        : std::clamp((0.15f - sunDir.y) / 0.35f, 0.0f, 1.0f);
    float innerCos = std::cos(DegreesToRadians(lighting.spotLightInnerAngleDegrees));
    float outerCos = std::cos(DegreesToRadians(lighting.spotLightOuterAngleDegrees));

    std::uint32_t shadowedSpotCount = 0;
    for (std::size_t i = 0; i < shadowedBudget; ++i)
    {
        if (shadowedLights[i].lightIndex >= spotLightSources.size())
        {
            continue;
        }

        const SpotLightData& light = spotLightSources[shadowedLights[i].lightIndex];
        if (light.intensity * sceneLightScale * lighting.spotLightIntensityScale <= 0.001f)
        {
            continue;
        }

        Vec3 sourceOffset = Vec3Scale(lighting.spotLightSourceOffset, light.sourceOffsetScale);
        Vec3 lightPosition = Vec3Add(light.position, RotateYOffset(sourceOffset, light.yawDegrees));
        Vec3 lightDirection = light.direction;
        if (lighting.sceneKind == SceneKind::ShadowTest &&
            shadowedLights[i].lightIndex == 0 &&
            lighting.shadowTestSpotTargetValid)
        {
            lightDirection = NormalizeOrFallback(
                Vec3Sub(lighting.shadowTestSpotTargetWorld, lightPosition),
                light.direction
            );
        }

        float range = light.range * lighting.spotLightRangeScale;
        uniforms.shadowedSpotLightPositions[shadowedSpotCount] = Vec4Make(lightPosition.x, lightPosition.y, lightPosition.z, range);
        uniforms.shadowedSpotLightDirections[shadowedSpotCount] = Vec4Make(lightDirection.x, lightDirection.y, lightDirection.z, light.outerCos);
        uniforms.shadowedSpotLightColors[shadowedSpotCount] = Vec4Make(
            light.color.x,
            light.color.y,
            light.color.z,
            light.intensity * sceneLightScale * lighting.spotLightIntensityScale
        );
        uniforms.shadowedSpotLightParams[shadowedSpotCount] = Vec4Make(innerCos, outerCos, 0.0f, 0.0f);
        uniforms.shadowViewProj[kSunShadowCascadeCount + shadowedSpotCount] = MakeSpotShadowViewProj(
            lightPosition,
            lightDirection,
            DegreesToRadians(lighting.spotLightOuterAngleDegrees),
            range
        );
        ++shadowedSpotCount;
    }

    std::uint32_t sceneLightCount = 0;
    for (std::size_t i = 0; i < activeBudget; ++i)
    {
        if (selectedLights[i].lightIndex >= spotLightSources.size() || shadowedMask[selectedLights[i].lightIndex])
        {
            continue;
        }

        const SpotLightData& light = spotLightSources[selectedLights[i].lightIndex];
        if (light.intensity * sceneLightScale * lighting.spotLightIntensityScale <= 0.001f)
        {
            continue;
        }

        Vec3 sourceOffset = Vec3Scale(lighting.spotLightSourceOffset, light.sourceOffsetScale);
        Vec3 lightPosition = Vec3Add(light.position, RotateYOffset(sourceOffset, light.yawDegrees));
        Vec3 lightDirection = light.direction;
        if (lighting.sceneKind == SceneKind::ShadowTest &&
            selectedLights[i].lightIndex == 0 &&
            lighting.shadowTestSpotTargetValid)
        {
            lightDirection = NormalizeOrFallback(
                Vec3Sub(lighting.shadowTestSpotTargetWorld, lightPosition),
                light.direction
            );
        }

        uniforms.spotLightPositions[sceneLightCount] = Vec4Make(
            lightPosition.x,
            lightPosition.y,
            lightPosition.z,
            light.range * lighting.spotLightRangeScale
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
            light.intensity * sceneLightScale * lighting.spotLightIntensityScale
        );
        uniforms.spotLightParams[sceneLightCount] = Vec4Make(innerCos, outerCos, 0.0f, 0.0f);
        ++sceneLightCount;
    }
    uniforms.sceneLightCounts = Vec4Make(static_cast<float>(sceneLightCount), static_cast<float>(shadowedSpotCount), 0.0f, 0.0f);

    if (lighting.sceneKind == SceneKind::City)
    {
        std::sort(
            rearPointCandidates.begin(),
            rearPointCandidates.end(),
            [](const RearPointCandidate& a, const RearPointCandidate& b) { return a.dist2 < b.dist2; }
        );
        for (int i = 0; i < 4; ++i)
        {
            if (static_cast<std::size_t>(i) < rearPointCandidates.size())
            {
                const RearPointCandidate& candidate = rearPointCandidates[static_cast<std::size_t>(i)];
                uniforms.lightPositions[i] = Vec4Make(candidate.position.x, candidate.position.y, candidate.position.z, candidate.range);
                uniforms.lightColors[i] = Vec4Make(candidate.color.x, candidate.color.y, candidate.color.z, 1.0f);
            }
            else
            {
                uniforms.lightPositions[i] = Vec4Make(0.0f, -1000.0f, 0.0f, 1.0f);
                uniforms.lightColors[i] = Vec4Make(0.0f, 0.0f, 0.0f, 1.0f);
            }
        }
    }

    float cameraNear = 0.1f;
    float cameraFar = 200.0f;
    float cameraAspect = static_cast<float>(std::max(runtime.windowWidth, 1u)) / static_cast<float>(std::max(runtime.windowHeight, 1u));
    float cameraFovY = DegreesToRadians(60.0f);
    float splitDistance = std::clamp(lighting.shadowCascadeSplit, 8.0f, cameraFar - 10.0f);
    float sceneRadius = core.sceneBounds.valid ? core.sceneBounds.radius : 24.0f;

    ShadowCascadeBounds nearCascade = FitShadowCascadeToCameraSlice(
        core.camera,
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
    uniforms.shadowViewProj[1] = MakeShadowViewProj(center, keyDirToLight, farExtent, farExtent, 0.1f, farExtent * 3.0f);
    float cascadeBlendWidth = std::max(splitDistance * 0.12f, 2.0f);
    uniforms.shadowParams = Vec4Make(lighting.shadowBlur ? 1.0f : 0.0f, splitDistance, cascadeBlendWidth, 0.0f);

    if (PopulateVehicleLightTestLighting(state, uniforms))
    {
        return;
    }

    if (lighting.sceneKind != SceneKind::City)
    {
        PopulateFallbackOrbitPointLights(state, uniforms, center);
    }
}
