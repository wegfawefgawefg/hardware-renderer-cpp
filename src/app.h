#pragma once

#include <chrono>
#include <cstdint>
#include <array>
#include <string>

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "camera.h"
#include "assets/asset_registry.h"
#include "collision/triangle_collider.h"
#include "gameplay/player_controller.h"
#include "gameplay/traffic.h"
#include "animation/character.h"
#include "scene.h"
#include "render/renderer.h"

struct App
{
    ~App();

    void Run();
    void Initialize();
    void Shutdown();
    void HandleEvent(const SDL_Event& event);
    void Update(float dtSeconds);
    void SyncRendererSize();
    void UpdateWindowTitle();
    void ResetMouseCapture(bool captured);
    void InitializeImGui();
    void ShutdownImGui();
    void ProcessImGuiEvent(const SDL_Event& event);
    void BuildImGui();
    void ApplyLighting(SceneUniforms& uniforms, float dtSeconds);
    void TryPlaceShadowTestSpotlight(int mouseX, int mouseY);
    void UpdateOverlayText(const SceneUniforms* uniforms = nullptr);
    void ReloadScene();
    void LoadDebugSettings();
    void SaveDebugSettings() const;

    SDL_Window* m_window = nullptr;
    VulkanRenderer m_renderer;
    AssetRegistry m_assetRegistry;
    SceneData m_scene;
    SceneBounds m_sceneBounds;
    TriangleMeshCollider m_worldCollider;
    PlayerController m_player;
    TrafficSystem m_traffic;
    CharacterAnimationSet m_characterSet;
    CharacterRenderState m_characterRenderState;
    Camera m_camera;
    TTF_Font* m_uiFont = nullptr;

    bool m_running = true;
    bool m_showImGui = true;
    bool m_mouseCaptured = false;
    bool m_windowResized = false;
    std::uint32_t m_windowWidth = 0;
    std::uint32_t m_windowHeight = 0;
    float m_elapsedSeconds = 0.0f;
    float m_smoothedFps = 0.0f;
    float m_titleRefreshSeconds = 0.0f;
    float m_overlayRefreshSeconds = 0.0f;
    float m_characterAnimTime = 0.0f;
    float m_characterModelYaw = 0.0f;
    int m_activeCharacterAnim = 0;
    std::uint32_t m_sceneTriangleCount = 0;
    bool m_hasCharacter = false;
    bool m_reloadSceneRequested = false;
    bool m_cycleDayNight = true;
    float m_timeOfDay = 0.32f;
    float m_dayNightSpeed = 0.035f;
    float m_sunAzimuthDegrees = -35.0f;
    float m_orbitDistanceScale = 1.6f;
    float m_sunIntensity = 1.35f;
    float m_moonIntensity = 0.18f;
    float m_ambientIntensity = 0.35f;
    float m_pointLightIntensity = 0.0f;
    Vec3 m_sunWorldPosition = {};
    Vec3 m_moonWorldPosition = {};
    SceneKind m_sceneKind = SceneKind::ShadowTest;
    bool m_shadowBlur = true;
    std::uint32_t m_shadowMapSize = 2048;
    float m_shadowCascadeSplit = 32.0f;
    float m_spotLightIntensityScale = 1.0f;
    float m_spotLightRangeScale = 1.0f;
    float m_spotLightInnerAngleDegrees = 18.0f;
    float m_spotLightOuterAngleDegrees = 34.0f;
    std::uint32_t m_spotLightMaxActive = 16;
    float m_spotLightActivationDistance = 28.0f;
    float m_spotLightActivationForwardOffset = 10.0f;
    std::uint32_t m_shadowedSpotLightMaxActive = 2;
    float m_shadowedSpotLightActivationDistance = 16.0f;
    float m_shadowedSpotLightActivationForwardOffset = 6.0f;
    Vec3 m_spotLightSourceOffset = {};
    bool m_debugDrawActivationVolumes = true;
    bool m_debugDrawSceneLightGizmos = true;
    bool m_debugDrawLightDirections = true;
    bool m_debugDrawLightLabels = true;
    bool m_shadowTestSpotTargetValid = false;
    Vec3 m_shadowTestSpotTargetWorld = {};
    Vec3 m_shadowTestSpotTargetOffset = {};

    std::array<std::uint32_t, 512 * 128> m_overlayPixels = {};
    std::uint32_t m_overlayWidth = 0;
    std::uint32_t m_overlayHeight = 0;
};
