#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "collision/triangle_collider.h"
#include "math_types.h"

enum class SurfaceMaskChannel : std::uint32_t
{
    Grime = 0,
    Glow = 1,
    Wetness = 2,
    Vanish = 3,
};

enum class PaintInteractionMode : std::uint32_t
{
    PaintBalls = 0,
    SurfaceBrush = 1,
};

struct PaintBallSettings
{
    static constexpr std::uint32_t kMaxBalls = 64;

    std::uint32_t bounceLimit = 4;
    Vec3 baseColor = {1.0f, 0.2f, 0.75f};
    bool cycleColorOnShoot = false;
    float radius = 0.12f;
    float blobRadius = 0.38f;
    float shootSpeed = 28.0f;
    float fireRate = 8.0f;
    float gravity = 22.0f;
    float restitution = 0.72f;
    SurfaceMaskChannel maskChannel = SurfaceMaskChannel::Grime;
    float maskStrength = 0.8f;
};

struct SurfaceMaskBrushSettings
{
    SurfaceMaskChannel channel = SurfaceMaskChannel::Grime;
    float strength = 0.85f;
    float radius = 0.30f;
    float flowRate = 18.0f;
};

struct PaintBall
{
    bool active = false;
    Vec3 position = {};
    Vec3 velocity = {};
    Vec3 color = {1.0f, 0.2f, 0.75f};
    float radius = 0.12f;
    float lifetime = 0.0f;
    std::uint32_t remainingBounces = 0;
    std::uint64_t shotId = 0;
};

struct PaintSplatSpawn
{
    Vec3 position = {};
    Vec3 normal = {0.0f, 1.0f, 0.0f};
    Vec3 color = {1.0f, 0.2f, 0.75f};
    float radius = 0.4f;
    std::uint32_t entityIndex = UINT32_MAX;
    std::uint32_t primitiveIndex = UINT32_MAX;
    Vec2 uv = {};
    float uvWorldScale = 1.0f;
    std::uint32_t maskChannel = 0;
    float maskStrength = 0.8f;
};

struct PaintBallSystem
{
    void Reset();
    void Fire(Vec3 origin, Vec3 direction, const PaintBallSettings& settings);
    void Update(
        const TriangleMeshCollider& worldCollider,
        float dtSeconds,
        const PaintBallSettings& settings,
        std::vector<PaintSplatSpawn>& outSplats
    );

    std::uint32_t ActiveCount() const;
    const std::array<PaintBall, PaintBallSettings::kMaxBalls>& Balls() const { return m_balls; }

    std::array<PaintBall, PaintBallSettings::kMaxBalls> m_balls = {};
    std::uint64_t m_nextShotId = 1;
};
