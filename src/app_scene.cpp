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
    ShutdownImGui();
    m_renderer.Shutdown();

    m_scene = LoadSampleScene(m_assetRegistry, m_sceneKind);
    m_sceneBounds = ComputeSceneBounds(m_scene);
    m_sceneTriangleCount = CountSceneTriangles(m_scene);
    m_traffic.Initialize(m_scene);
    m_worldCollider.BuildFromScene(m_scene);

    if (m_sceneBounds.valid)
    {
        m_camera.yawRadians = DegreesToRadians(180.0f);
        m_camera.pitchRadians = DegreesToRadians(-14.0f);
    }

    PlayerSpawn(m_player, m_worldCollider, m_sceneBounds);
    m_characterModelYaw = m_camera.yawRadians;
    PlayerSyncCamera(m_player, m_camera);

    if (m_sceneKind == SceneKind::ShadowTest)
    {
        m_cycleDayNight = false;
        m_timeOfDay = 0.50f;
        m_sunAzimuthDegrees = -35.0f;
        m_sunIntensity = 1.35f;
        m_moonIntensity = 0.0f;
        m_ambientIntensity = 0.18f;
        m_pointLightIntensity = 0.0f;
        m_shadowCascadeSplit = 12.0f;
        if (!m_scene.spotLights.empty())
        {
            Vec3 source = Vec3Add(
                m_scene.spotLights[0].position,
                RotateYOffset(m_spotLightSourceOffset, m_scene.spotLights[0].yawDegrees)
            );
            Vec3 defaultTarget = Vec3Add(source, Vec3Scale(m_scene.spotLights[0].direction, 7.0f));
            TriangleMeshCollider::RayHit hit = m_worldCollider.Raycast(source, m_scene.spotLights[0].direction, 24.0f);
            m_shadowTestSpotTargetValid = hit.hit;
            m_shadowTestSpotTargetWorld = hit.hit ? hit.position : defaultTarget;
            m_shadowTestSpotTargetOffset = Vec3Sub(m_shadowTestSpotTargetWorld, source);
        }
    }
    else if (m_sceneKind == SceneKind::SpotShadowTest)
    {
        m_shadowTestSpotTargetValid = false;
        m_cycleDayNight = false;
        m_timeOfDay = 0.50f;
        m_sunAzimuthDegrees = -35.0f;
        m_sunIntensity = 0.0f;
        m_moonIntensity = 0.0f;
        m_ambientIntensity = 0.05f;
        m_pointLightIntensity = 0.0f;
        m_shadowCascadeSplit = 16.0f;
        m_spotLightMaxActive = 8;
        m_spotLightActivationDistance = 32.0f;
        m_spotLightActivationForwardOffset = 8.0f;
        m_shadowedSpotLightMaxActive = 2;
        m_shadowedSpotLightActivationDistance = 16.0f;
        m_shadowedSpotLightActivationForwardOffset = 6.0f;
    }
    else
    {
        m_shadowTestSpotTargetValid = false;
    }

    m_renderer.m_shadowMapSize = m_shadowMapSize;
    m_renderer.Initialize(m_window, m_scene, m_hasCharacter ? &m_characterSet.asset : nullptr);
    InitializeImGui();
    SyncRendererSize();
    UpdateWindowTitle();
    UpdateOverlayText();
    m_reloadSceneRequested = false;
}
