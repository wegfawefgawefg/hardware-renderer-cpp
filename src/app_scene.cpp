#include "app.h"

#include <stdexcept>

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
    }

    m_renderer.m_shadowMapSize = m_shadowMapSize;
    m_renderer.Initialize(m_window, m_scene, m_hasCharacter ? &m_characterSet.asset : nullptr);
    InitializeImGui();
    SyncRendererSize();
    UpdateWindowTitle();
    UpdateOverlayText();
    m_reloadSceneRequested = false;
}
