#include "app.h"

#include <cmath>
#include <stdexcept>

namespace
{
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
}

void App::ReloadScene()
{
    auto& core = m_state.core;
    auto& runtime = m_state.runtime;
    auto& lighting = m_state.lighting;
    ShutdownImGui();
    core.renderer.Shutdown();

    core.scene = LoadSampleScene(core.assetRegistry, lighting.sceneKind);
    core.sceneBounds = ComputeSceneBounds(core.scene);
    runtime.sceneTriangleCount = CountSceneTriangles(core.scene);
    m_state.paint.splats = {};
    m_state.paint.splatCount = 0;
    m_state.paint.nextSplatIndex = 0;
    m_state.paint.entityLayers.assign(core.scene.entities.size(), EntityPaintLayer{});
    core.traffic.Initialize(core.scene);
    core.worldCollider.BuildFromScene(core.scene);

    if (core.sceneBounds.valid)
    {
        core.camera.yawRadians = DegreesToRadians(180.0f);
        core.camera.pitchRadians = DegreesToRadians(-14.0f);
    }

    PlayerSpawn(core.player, core.worldCollider, core.sceneBounds);
    runtime.characterModelYaw = core.camera.yawRadians;
    PlayerSyncCamera(core.player, core.worldCollider, core.camera);

    if (lighting.sceneKind == SceneKind::ShadowTest)
    {
        lighting.cycleDayNight = false;
        lighting.timeOfDay = 0.50f;
        lighting.sunAzimuthDegrees = -35.0f;
        lighting.sunIntensity = 1.35f;
        lighting.moonIntensity = 0.0f;
        lighting.ambientIntensity = 0.18f;
        lighting.pointLightIntensity = 0.0f;
        lighting.shadowCascadeSplit = 12.0f;
        if (!core.scene.spotLights.empty())
        {
            Vec3 source = Vec3Add(
                core.scene.spotLights[0].position,
                RotateYOffset(lighting.spotLightSourceOffset, core.scene.spotLights[0].yawDegrees)
            );
            Vec3 defaultTarget = Vec3Add(source, Vec3Scale(core.scene.spotLights[0].direction, 7.0f));
            TriangleMeshCollider::RayHit hit = core.worldCollider.Raycast(source, core.scene.spotLights[0].direction, 24.0f);
            lighting.shadowTestSpotTargetValid = hit.hit;
            lighting.shadowTestSpotTargetWorld = hit.hit ? hit.position : defaultTarget;
            lighting.shadowTestSpotTargetOffset = Vec3Sub(lighting.shadowTestSpotTargetWorld, source);
        }
    }
    else if (lighting.sceneKind == SceneKind::SpotShadowTest)
    {
        lighting.shadowTestSpotTargetValid = false;
        lighting.cycleDayNight = false;
        lighting.timeOfDay = 0.50f;
        lighting.sunAzimuthDegrees = -35.0f;
        lighting.sunIntensity = 0.0f;
        lighting.moonIntensity = 0.0f;
        lighting.ambientIntensity = 0.05f;
        lighting.pointLightIntensity = 0.0f;
        lighting.shadowCascadeSplit = 16.0f;
    }
    else if (lighting.sceneKind == SceneKind::VehicleLightTest)
    {
        lighting.shadowTestSpotTargetValid = false;
        lighting.cycleDayNight = false;
        lighting.timeOfDay = 0.72f;
        lighting.sunAzimuthDegrees = -28.0f;
        lighting.sunIntensity = 0.12f;
        lighting.moonIntensity = 0.0f;
        lighting.ambientIntensity = 0.06f;
        lighting.pointLightIntensity = 0.0f;
        lighting.shadowCascadeSplit = 20.0f;
        core.player.position = Vec3Make(-56.0f, 1.0f, -6.0f);
        core.player.velocity = Vec3Make(0.0f, 0.0f, 0.0f);
        core.player.onGround = true;
        core.camera.position = Vec3Make(-56.0f, 3.2f, -12.0f);
        core.camera.yawRadians = DegreesToRadians(180.0f);
        core.camera.pitchRadians = DegreesToRadians(-10.0f);
        PlayerSyncCamera(core.player, core.worldCollider, core.camera);
    }
    else
    {
        lighting.shadowTestSpotTargetValid = false;
    }

    core.renderer.m_shadowMapSize = lighting.shadowMapSize;
    core.renderer.Initialize(m_window, core.scene, runtime.hasCharacter ? &core.characterSet.asset : nullptr);
    InitializeImGui();
    SyncRendererSize();
    UpdateWindowTitle();
    UpdateOverlayText();
    runtime.reloadSceneRequested = false;
}
