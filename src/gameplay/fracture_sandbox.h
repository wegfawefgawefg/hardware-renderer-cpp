#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "camera.h"
#include "collision/triangle_collider.h"
#include "decals/flat_decal_system.h"
#include "gameplay/fracture.h"
#include "scene.h"

namespace audio
{
struct System;
}

struct FractureSandboxState
{
    struct PrismSettings
    {
        Vec3 halfExtents = {3.5f, 5.5f, 3.0f};
        std::uint32_t segX = 14;
        std::uint32_t segY = 20;
        std::uint32_t segZ = 12;
    };

    FractureSettings settings = {};
    PrismSettings prism = {};
    bool showWireframe = false;
    bool dentDepthMatchesRadius = true;
    bool fireHeld = false;
    float fireCooldown = 0.0f;
    bool hitValid = false;
    static constexpr std::uint32_t kMaxDamageDecalTemplates = 8;
    std::array<decals::FlatDecalTemplateId, kMaxDamageDecalTemplates> damageDecalTemplates = {};
    std::array<std::string, kMaxDamageDecalTemplates> damageDecalTemplateNames = {};
    std::uint32_t damageDecalTemplateCount = 0;
    std::uint32_t selectedDamageDecalTemplate = 0;
    std::uint32_t decalShotCounter = 0;
    Vec3 hitPosition = {};
    Vec3 hitNormal = {0.0f, 1.0f, 0.0f};
};

enum class FractureSceneRefresh
{
    None,
    GeometryOnly,
    FullRebuild,
};

struct FractureShotResult
{
    bool applied = false;
    FractureSceneRefresh refresh = FractureSceneRefresh::None;
};

void UpdateFractureSandboxHover(
    const TriangleMeshCollider& worldCollider,
    const Camera& camera,
    FractureSandboxState& state,
    bool fractureSceneActive,
    float dtSeconds);

FractureShotResult FireFractureSandboxShot(
    SceneData& scene,
    const TriangleMeshCollider& worldCollider,
    const Camera& camera,
    FractureSandboxState& state,
    decals::FlatDecalSystem& flatDecals,
    audio::System& audio);
