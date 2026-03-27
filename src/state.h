#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "animation/character.h"
#include "assets/asset_registry.h"
#include "camera.h"
#include "collision/triangle_collider.h"
#include "gameplay/paint_balls.h"
#include "gameplay/player_controller.h"
#include "gameplay/traffic.h"
#include "paint_runtime.h"
#include "render/renderer.h"
#include "scene.h"
#include "vehicle_lights.h"

struct CpuProfilingStats
{
    float inputMs = 0.0f;
    float imguiMs = 0.0f;
    float lightingMs = 0.0f;
    float renderMs = 0.0f;
    float frameMs = 0.0f;
};

struct CoreState
{
    VulkanRenderer renderer;
    AssetRegistry assetRegistry;
    SceneData scene;
    SceneBounds sceneBounds;
    TriangleMeshCollider worldCollider;
    PlayerController player;
    TrafficSystem traffic;
    PaintBallSystem paintBalls;
    CharacterAnimationSet characterSet;
    CharacterRenderState characterRenderState;
    Camera camera;
};

struct RuntimeState
{
    bool running = true;
    bool showImGui = true;
    bool mouseCaptured = false;
    bool windowResized = false;
    bool hasCharacter = false;
    bool reloadSceneRequested = false;
    std::uint32_t windowWidth = 0;
    std::uint32_t windowHeight = 0;
    std::uint32_t sceneTriangleCount = 0;
    float elapsedSeconds = 0.0f;
    float smoothedFps = 0.0f;
    float titleRefreshSeconds = 0.0f;
    float overlayRefreshSeconds = 0.0f;
    float characterAnimTime = 0.0f;
    float characterModelYaw = 0.0f;
    int activeCharacterAnim = 0;
    CpuProfilingStats cpuProfiling = {};
};

struct LightingState
{
    bool cycleDayNight = true;
    bool animateSunAzimuth = true;
    bool debugVisualizeUv = false;
    bool shadowBlur = true;
    bool debugDrawActivationVolumes = true;
    bool drawLightProxies = true;
    bool debugDrawSceneLightGizmos = true;
    bool debugDrawLightDirections = true;
    bool debugDrawLightVolumes = true;
    bool debugDrawLightLabels = true;
    bool shadowTestSpotTargetValid = false;
    SceneKind sceneKind = SceneKind::PlayerMaskTest;
    std::uint32_t shadowMapSize = 2048;
    std::uint32_t spotLightMaxActive = 16;
    std::uint32_t shadowedSpotLightMaxActive = 2;
    float timeOfDay = 0.32f;
    float dayNightSpeed = 0.035f;
    float sunAzimuthDegrees = -35.0f;
    float orbitDistanceScale = 1.6f;
    float sunIntensity = 1.35f;
    float moonIntensity = 0.18f;
    float ambientIntensity = 0.35f;
    float pointLightIntensity = 0.0f;
    float shadowCascadeSplit = 32.0f;
    float mainDrawDistance = 140.0f;
    float shadowDrawDistance = 180.0f;
    float spotLightIntensityScale = 1.0f;
    float spotLightRangeScale = 1.0f;
    float spotLightInnerAngleDegrees = 18.0f;
    float spotLightOuterAngleDegrees = 34.0f;
    float spotLightActivationDistance = 28.0f;
    float spotLightActivationForwardOffset = 10.0f;
    float shadowedSpotLightActivationDistance = 16.0f;
    float shadowedSpotLightActivationForwardOffset = 6.0f;
    float uvDebugScale = 8.0f;
    std::uint32_t uvDebugMode = 0;
    Vec3 sunWorldPosition = {};
    Vec3 moonWorldPosition = {};
    Vec3 spotLightSourceOffset = {};
    Vec3 shadowTestSpotTargetWorld = {};
    Vec3 shadowTestSpotTargetOffset = {};
};

struct VehicleLightEditorState
{
    VehicleLightSlot slot = VehicleLightSlot::HeadA;
    VehicleLightRigMap rigs;
    bool debugDrawVehicleVolumes = true;
    bool debugDrawVehicleLightRanges = true;
};

struct PaintState
{
    PaintBallSettings ballSettings = {};
    bool fireHeld = false;
    float fireCooldown = 0.0f;
    std::array<PaintSplat, kMaxPaintSplats> splats = {};
    std::uint32_t splatCount = 0;
    std::uint32_t nextSplatIndex = 0;
};

struct OverlayState
{
    std::array<std::uint32_t, 512 * 128> pixels = {};
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct State
{
    CoreState core;
    RuntimeState runtime;
    LightingState lighting;
    VehicleLightEditorState vehicleLights;
    PaintState paint;
    OverlayState overlay;
};
