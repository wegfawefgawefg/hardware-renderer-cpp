#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "assets/asset_registry.h"
#include "math_types.h"
#include "render/renderer.h"

struct FractureChunk
{
    Vec3 center = {};
    float halfExtent = 0.75f;
    Vec3 color = {0.6f, 0.6f, 0.62f};
    bool active = true;
};

struct FractureDebris
{
    Vec3 position = {};
    Vec3 velocity = {};
    Vec3 color = {0.6f, 0.6f, 0.62f};
    float halfExtent = 0.35f;
    float lifetime = 0.0f;
    bool active = false;
};

struct FractureHit
{
    bool hit = false;
    float distance = 0.0f;
    Vec3 position = {};
    Vec3 normal = {0.0f, 1.0f, 0.0f};
    std::uint32_t chunkIndex = UINT32_MAX;
};

struct FractureSettings
{
    float chunkHalfExtent = 0.75f;
    float blastRadius = 1.65f;
    float fireRate = 6.0f;
    float debrisSpeed = 9.0f;
    float debrisGravity = 18.0f;
    float debrisLifetime = 4.0f;
    std::uint32_t maxDebrisPerBlast = 24;
};

struct FractureSystem
{
    bool InitializeFromAsset(const AssetRegistry& assetRegistry, std::string_view relativePath, const FractureSettings& settings);
    void Reset();
    void Clear();
    void Update(float dtSeconds, const FractureSettings& settings);
    FractureHit Raycast(Vec3 origin, Vec3 direction, float maxDistance) const;
    std::uint32_t FractureAt(const FractureHit& hit, const FractureSettings& settings);
    void AppendDebugGeometry(DebugRenderOptions& debug) const;
    std::uint32_t ActiveChunkCount() const;
    std::uint32_t ActiveDebrisCount() const;
    bool Ready() const { return !m_templateChunks.empty(); }

    std::vector<FractureChunk> m_templateChunks;
    std::vector<FractureChunk> m_chunks;
    std::vector<FractureDebris> m_debris;
};
