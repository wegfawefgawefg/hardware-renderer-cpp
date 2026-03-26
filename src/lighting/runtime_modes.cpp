#include "lighting/runtime_internal.h"

#include <algorithm>
#include <cmath>

namespace
{
int FindActiveVehicleLightIndex(const State& state)
{
    if (state.lighting.sceneKind != SceneKind::VehicleLightTest)
    {
        return -1;
    }

    int bestIndex = -1;
    float bestDist2 = 1e30f;
    for (std::size_t i = 0; i < state.core.scene.vehicleLightTestItems.size(); ++i)
    {
        const SceneData::VehicleLightTestItem& item = state.core.scene.vehicleLightTestItems[i];
        Vec3 delta = Vec3Sub(state.core.player.position, item.origin);
        float dist2 = Vec3Dot(delta, delta);
        if (dist2 <= item.selectionRadius * item.selectionRadius && dist2 < bestDist2)
        {
            bestIndex = static_cast<int>(i);
            bestDist2 = dist2;
        }
    }
    return bestIndex;
}
}

void CollectCityVehicleLights(
    const State& state,
    std::vector<SpotLightData>& spotLightSources,
    std::vector<RearPointCandidate>& rearPointCandidates
)
{
    rearPointCandidates.reserve(state.core.scene.entities.size());
    for (const EntityData& entity : state.core.scene.entities)
    {
        if (!entity.traffic || entity.assetPath.empty())
        {
            continue;
        }

        auto found = state.vehicleLights.rigs.find(entity.assetPath);
        if (found == state.vehicleLights.rigs.end())
        {
            continue;
        }

        const VehicleLightRig& rig = found->second;
        float entityScale = Vec3Length(Vec3Make(entity.transform.m[0], entity.transform.m[1], entity.transform.m[2]));
        Vec3 headADir = VehicleLightDirection(rig.headA.yawDegrees, rig.headA.pitchDegrees);
        Vec3 headBDir = VehicleLightDirection(rig.headB.yawDegrees, rig.headB.pitchDegrees);
        Vec3 headAPos = TransformLightPoint(entity.transform, rig.headA.offset);
        Vec3 headBPos = TransformLightPoint(entity.transform, rig.headB.offset);

        SpotLightData headA{};
        headA.position = headAPos;
        headA.range = rig.headA.range * entityScale;
        headA.direction = TransformLightDirection(entity.transform, headADir);
        headA.color = Vec3Make(1.0f, 0.95f, 0.82f);
        headA.intensity = 3.2f;
        headA.sourceOffsetScale = 0.0f;
        spotLightSources.push_back(headA);

        SpotLightData headB{};
        headB.position = headBPos;
        headB.range = rig.headB.range * entityScale;
        headB.direction = TransformLightDirection(entity.transform, headBDir);
        headB.color = Vec3Make(1.0f, 0.95f, 0.82f);
        headB.intensity = 3.2f;
        headB.sourceOffsetScale = 0.0f;
        spotLightSources.push_back(headB);

        Vec3 rearAPos = TransformLightPoint(entity.transform, rig.rearA.offset);
        Vec3 rearBPos = TransformLightPoint(entity.transform, rig.rearB.offset);
        Vec3 rearAColor = Vec3Make(rig.rearA.intensity, 0.08f * rig.rearA.intensity, 0.08f * rig.rearA.intensity);
        Vec3 rearBColor = Vec3Make(rig.rearB.intensity, 0.08f * rig.rearB.intensity, 0.08f * rig.rearB.intensity);
        Vec3 rearADelta = Vec3Sub(rearAPos, state.core.camera.position);
        Vec3 rearBDelta = Vec3Sub(rearBPos, state.core.camera.position);
        rearPointCandidates.push_back(RearPointCandidate{
            .dist2 = Vec3Dot(rearADelta, rearADelta),
            .position = rearAPos,
            .color = rearAColor,
            .range = rig.rearA.range * entityScale,
        });
        rearPointCandidates.push_back(RearPointCandidate{
            .dist2 = Vec3Dot(rearBDelta, rearBDelta),
            .position = rearBPos,
            .color = rearBColor,
            .range = rig.rearB.range * entityScale,
        });
    }
}

bool PopulateVehicleLightTestLighting(State& state, SceneUniforms& uniforms)
{
    for (int i = 0; i < 4; ++i)
    {
        uniforms.lightPositions[i] = Vec4Make(0.0f, -1000.0f, 0.0f, 1.0f);
        uniforms.lightColors[i] = Vec4Make(0.0f, 0.0f, 0.0f, 1.0f);
    }

    int vehicleIndex = FindActiveVehicleLightIndex(state);
    if (vehicleIndex < 0)
    {
        return true;
    }

    const SceneData::VehicleLightTestItem& item =
        state.core.scene.vehicleLightTestItems[static_cast<std::size_t>(vehicleIndex)];
    VehicleLightRig rig = {};
    if (auto found = state.vehicleLights.rigs.find(item.assetPath); found != state.vehicleLights.rigs.end())
    {
        rig = found->second;
    }

    auto worldPoint = [&](Vec3 local) {
        return Vec3Add(item.origin, Vec3Scale(local, item.scale));
    };

    std::uint32_t shadowCount = 0;
    std::uint32_t unshadowCount = 0;
    auto addHeadlight = [&](const VehicleFrontLightConfig& cfg) {
        Vec3 pos = worldPoint(cfg.offset);
        Vec3 dir = VehicleLightDirection(cfg.yawDegrees, cfg.pitchDegrees);
        if (shadowCount < std::min(state.lighting.shadowedSpotLightMaxActive, kMaxShadowedSpotLights))
        {
            uniforms.shadowedSpotLightPositions[shadowCount] = Vec4Make(pos.x, pos.y, pos.z, cfg.range * item.scale);
            uniforms.shadowedSpotLightDirections[shadowCount] = Vec4Make(dir.x, dir.y, dir.z, 0.0f);
            uniforms.shadowedSpotLightColors[shadowCount] = Vec4Make(1.0f, 0.95f, 0.82f, 3.2f);
            uniforms.shadowedSpotLightParams[shadowCount] = Vec4Make(
                std::cos(DegreesToRadians(state.lighting.spotLightInnerAngleDegrees)),
                std::cos(DegreesToRadians(state.lighting.spotLightOuterAngleDegrees)),
                0.0f,
                0.0f
            );
            uniforms.shadowViewProj[kSunShadowCascadeCount + shadowCount] = MakeSpotShadowViewProj(
                pos,
                dir,
                DegreesToRadians(state.lighting.spotLightOuterAngleDegrees),
                cfg.range * item.scale
            );
            ++shadowCount;
        }
        else if (unshadowCount < std::min(state.lighting.spotLightMaxActive, kMaxSceneSpotLights))
        {
            uniforms.spotLightPositions[unshadowCount] = Vec4Make(pos.x, pos.y, pos.z, cfg.range * item.scale);
            uniforms.spotLightDirections[unshadowCount] = Vec4Make(dir.x, dir.y, dir.z, 0.0f);
            uniforms.spotLightColors[unshadowCount] = Vec4Make(1.0f, 0.95f, 0.82f, 3.2f);
            uniforms.spotLightParams[unshadowCount] = Vec4Make(
                std::cos(DegreesToRadians(state.lighting.spotLightInnerAngleDegrees)),
                std::cos(DegreesToRadians(state.lighting.spotLightOuterAngleDegrees)),
                0.0f,
                0.0f
            );
            ++unshadowCount;
        }
    };

    addHeadlight(rig.headA);
    addHeadlight(rig.headB);

    Vec3 rearA = worldPoint(rig.rearA.offset);
    Vec3 rearB = worldPoint(rig.rearB.offset);
    uniforms.lightPositions[0] = Vec4Make(rearA.x, rearA.y, rearA.z, rig.rearA.range * item.scale);
    uniforms.lightColors[0] = Vec4Make(rig.rearA.intensity, 0.08f * rig.rearA.intensity, 0.08f * rig.rearA.intensity, 1.0f);
    uniforms.lightPositions[1] = Vec4Make(rearB.x, rearB.y, rearB.z, rig.rearB.range * item.scale);
    uniforms.lightColors[1] = Vec4Make(rig.rearB.intensity, 0.08f * rig.rearB.intensity, 0.08f * rig.rearB.intensity, 1.0f);
    uniforms.sceneLightCounts = Vec4Make(static_cast<float>(unshadowCount), static_cast<float>(shadowCount), 0.0f, 0.0f);
    return true;
}

void PopulateFallbackOrbitPointLights(const State& state, SceneUniforms& uniforms, Vec3 center)
{
    float t = state.runtime.elapsedSeconds;
    float pointScale = state.lighting.pointLightIntensity;
    float orbit = std::max((state.core.sceneBounds.valid ? state.core.sceneBounds.radius : 20.0f) * 0.6f, 6.0f);
    float highY = center.y + std::max((state.core.sceneBounds.valid ? state.core.sceneBounds.radius : 20.0f) * 0.4f, 4.0f);
    float midY = center.y + std::max((state.core.sceneBounds.valid ? state.core.sceneBounds.radius : 20.0f) * 0.22f, 2.5f);
    float lowY = center.y + std::max((state.core.sceneBounds.valid ? state.core.sceneBounds.radius : 20.0f) * 0.15f, 1.5f);

    uniforms.lightPositions[0] = Vec4Make(center.x + std::cos(t * 0.8f) * orbit, highY, center.z + std::sin(t * 0.8f) * orbit, orbit * 1.6f);
    uniforms.lightColors[0] = Vec4Make(5.0f * pointScale, 4.8f * pointScale, 4.5f * pointScale, 1.0f);
    uniforms.lightPositions[1] = Vec4Make(center.x - orbit * 0.8f, midY + std::sin(t * 2.1f) * 0.5f, center.z + std::cos(t * 0.9f) * orbit * 0.55f, orbit * 1.2f);
    uniforms.lightColors[1] = Vec4Make(4.8f * pointScale, 0.45f * pointScale, 0.45f * pointScale, 1.0f);
    uniforms.lightPositions[2] = Vec4Make(center.x + std::cos(t * 1.1f) * orbit * 0.55f, lowY + std::cos(t * 1.8f) * 0.6f, center.z - orbit * 0.75f, orbit * 1.2f);
    uniforms.lightColors[2] = Vec4Make(0.45f * pointScale, 4.4f * pointScale, 0.55f * pointScale, 1.0f);
    uniforms.lightPositions[3] = Vec4Make(center.x + orbit * 0.8f, midY + std::sin(t * 1.5f) * 0.55f, center.z + std::sin(t * 1.0f) * orbit * 0.62f, orbit * 1.2f);
    uniforms.lightColors[3] = Vec4Make(0.55f * pointScale, 0.9f * pointScale, 4.8f * pointScale, 1.0f);
}
