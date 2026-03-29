#include "gameplay/fracture.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#include "assets/fbx_loader.h"
#include "collision/triangle_collider.h"
#include "scene.h"

namespace
{
constexpr std::string_view kFractureTallBuildingAsset =
    "kenney/kenney_city-kit-commercial_2.1/Models/FBX format/building-skyscraper-b.fbx";
constexpr std::string_view kFractureSmallBuildingAsset =
    "kenney/kenney_city-kit-suburban_20/Models/FBX format/building-type-q.fbx";

Vec3 TransformPoint(Mat4 m, Vec3 p)
{
    Vec4 out = Mat4MulVec4(m, Vec4Make(p.x, p.y, p.z, 1.0f));
    if (std::fabs(out.w) > 1e-6f && out.w != 1.0f)
    {
        float invW = 1.0f / out.w;
        return Vec3Make(out.x * invW, out.y * invW, out.z * invW);
    }
    return Vec3Make(out.x, out.y, out.z);
}

float ComputeModelFootprint(const ModelData& model)
{
    if (model.mesh.vertices.empty())
    {
        return 1.0f;
    }

    Vec3 mn = model.mesh.vertices.front().position;
    Vec3 mx = model.mesh.vertices.front().position;
    for (const Vertex& vertex : model.mesh.vertices)
    {
        mn.x = std::min(mn.x, vertex.position.x);
        mn.z = std::min(mn.z, vertex.position.z);
        mx.x = std::max(mx.x, vertex.position.x);
        mx.z = std::max(mx.z, vertex.position.z);
    }
    return std::max(std::max(mx.x - mn.x, mx.z - mn.z), 0.001f);
}

float ComputeModelMinY(const ModelData& model, Mat4 transform)
{
    float minY = 1e30f;
    for (const Vertex& vertex : model.mesh.vertices)
    {
        minY = std::min(minY, TransformPoint(transform, vertex.position).y);
    }
    return minY;
}

void ComputeWorldBounds(const ModelData& model, Mat4 transform, Vec3& outMin, Vec3& outMax)
{
    outMin = Vec3Make(1e30f, 1e30f, 1e30f);
    outMax = Vec3Make(-1e30f, -1e30f, -1e30f);
    for (const Vertex& vertex : model.mesh.vertices)
    {
        Vec3 p = TransformPoint(transform, vertex.position);
        outMin.x = std::min(outMin.x, p.x);
        outMin.y = std::min(outMin.y, p.y);
        outMin.z = std::min(outMin.z, p.z);
        outMax.x = std::max(outMax.x, p.x);
        outMax.y = std::max(outMax.y, p.y);
        outMax.z = std::max(outMax.z, p.z);
    }
}

bool RayIntersectsChunk(Vec3 origin, Vec3 dir, const FractureChunk& chunk, float maxDistance, float& outDistance, Vec3& outNormal)
{
    Vec3 min = Vec3Sub(chunk.center, Vec3Make(chunk.halfExtent, chunk.halfExtent, chunk.halfExtent));
    Vec3 max = Vec3Add(chunk.center, Vec3Make(chunk.halfExtent, chunk.halfExtent, chunk.halfExtent));
    float tMin = 0.0f;
    float tMax = maxDistance;
    Vec3 normal = {};

    auto updateAxis = [&](float originAxis, float dirAxis, float minAxis, float maxAxis, Vec3 negNormal, Vec3 posNormal)
    {
        if (std::fabs(dirAxis) < 1e-6f)
        {
            return originAxis >= minAxis && originAxis <= maxAxis;
        }
        float invDir = 1.0f / dirAxis;
        float t0 = (minAxis - originAxis) * invDir;
        float t1 = (maxAxis - originAxis) * invDir;
        Vec3 axisNormal = negNormal;
        if (t0 > t1)
        {
            std::swap(t0, t1);
            axisNormal = posNormal;
        }
        if (t0 > tMin)
        {
            tMin = t0;
            normal = axisNormal;
        }
        tMax = std::min(tMax, t1);
        return tMax >= tMin;
    };

    if (!updateAxis(origin.x, dir.x, min.x, max.x, Vec3Make(-1.0f, 0.0f, 0.0f), Vec3Make(1.0f, 0.0f, 0.0f)) ||
        !updateAxis(origin.y, dir.y, min.y, max.y, Vec3Make(0.0f, -1.0f, 0.0f), Vec3Make(0.0f, 1.0f, 0.0f)) ||
        !updateAxis(origin.z, dir.z, min.z, max.z, Vec3Make(0.0f, 0.0f, -1.0f), Vec3Make(0.0f, 0.0f, 1.0f)))
    {
        return false;
    }

    if (tMin < 0.0f || tMin > maxDistance)
    {
        return false;
    }
    outDistance = tMin;
    outNormal = normal;
    return true;
}

float Hash01(std::uint32_t v)
{
    v ^= v >> 16;
    v *= 0x7feb352dU;
    v ^= v >> 15;
    v *= 0x846ca68bU;
    v ^= v >> 16;
    return static_cast<float>(v & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

bool AppendAssetInstance(
    std::vector<FractureChunk>& outChunks,
    const AssetRegistry& assetRegistry,
    std::string_view relativePath,
    const FractureSettings& settings,
    Vec3 worldOffset,
    float targetFootprint)
{
    const std::filesystem::path* path = assetRegistry.FindByRelativePath(relativePath);
    if (path == nullptr)
    {
        return false;
    }

    ModelData model = LoadFbxModel(path->string());
    if (model.mesh.vertices.empty() || model.mesh.indices.empty())
    {
        return false;
    }

    float footprint = ComputeModelFootprint(model);
    float scale = targetFootprint / footprint;
    Mat4 baseTransform = Mat4Mul(Mat4Translate(worldOffset), Mat4Scale(scale));
    float minY = ComputeModelMinY(model, baseTransform);
    Mat4 transform = Mat4Mul(Mat4Translate(Vec3Make(0.0f, -minY, 0.0f)), baseTransform);

    SceneData tempScene{};
    tempScene.models.push_back(model);
    tempScene.entities.push_back(EntityData{
        .modelIndex = 0,
        .transform = transform,
        .collidable = true,
    });
    TriangleMeshCollider collider;
    collider.BuildFromScene(tempScene);

    Vec3 boundsMin = {};
    Vec3 boundsMax = {};
    ComputeWorldBounds(model, transform, boundsMin, boundsMax);
    const float step = std::max(settings.chunkHalfExtent * 2.0f, 0.2f);
    const float half = step * 0.5f;
    const float rayStart = boundsMax.y + step * 2.0f;
    const float rayDistance = (boundsMax.y - boundsMin.y) + step * 4.0f;

    std::uint32_t baseSeed = static_cast<std::uint32_t>(outChunks.size() * 9781u + 17u);
    for (float x = boundsMin.x + half; x <= boundsMax.x - half; x += step)
    {
        for (float z = boundsMin.z + half; z <= boundsMax.z - half; z += step)
        {
            TriangleMeshCollider::RayHit hit = collider.RaycastDown(x, z, rayStart, rayDistance);
            if (!hit.hit)
            {
                continue;
            }

            for (float y = boundsMin.y + half; y <= hit.position.y - half * 0.1f; y += step)
            {
                std::uint32_t seed = static_cast<std::uint32_t>(outChunks.size() * 9781u + baseSeed);
                float tone = 0.44f + Hash01(seed) * 0.18f + (y - boundsMin.y) * 0.006f;
                FractureChunk chunk{};
                chunk.center = Vec3Make(x, y, z);
                chunk.halfExtent = half * 0.92f;
                chunk.color = Vec3Make(tone * 0.94f, tone * 0.96f, tone);
                chunk.active = true;
                outChunks.push_back(chunk);
            }
        }
    }

    return true;
}
}

bool FractureSystem::InitializeFromAsset(const AssetRegistry& assetRegistry, std::string_view relativePath, const FractureSettings& settings)
{
    Clear();
    if (!AppendAssetInstance(
            m_templateChunks,
            assetRegistry,
            relativePath.empty() ? kFractureTallBuildingAsset : relativePath,
            settings,
            Vec3Make(0.0f, 0.0f, 0.0f),
            8.5f))
    {
        return false;
    }

    Reset();
    return !m_templateChunks.empty();
}

bool FractureSystem::InitializeTestSet(const AssetRegistry& assetRegistry, const FractureSettings& settings)
{
    Clear();
    bool tallReady = AppendAssetInstance(
        m_templateChunks,
        assetRegistry,
        kFractureTallBuildingAsset,
        settings,
        Vec3Make(6.5f, 0.0f, 0.0f),
        8.5f
    );
    bool smallReady = AppendAssetInstance(
        m_templateChunks,
        assetRegistry,
        kFractureSmallBuildingAsset,
        settings,
        Vec3Make(-6.0f, 0.0f, 1.5f),
        6.0f
    );
    if (!tallReady && !smallReady)
    {
        return false;
    }

    Reset();
    return !m_templateChunks.empty();
}

void FractureSystem::Reset()
{
    m_chunks = m_templateChunks;
    m_debris.clear();
    m_debris.resize(std::max<std::size_t>(192, m_templateChunks.size() / 3));
}

void FractureSystem::Clear()
{
    m_templateChunks.clear();
    m_chunks.clear();
    m_debris.clear();
}

void FractureSystem::Update(float dtSeconds, const FractureSettings& settings)
{
    for (FractureDebris& debris : m_debris)
    {
        if (!debris.active)
        {
            continue;
        }
        debris.lifetime -= dtSeconds;
        if (debris.lifetime <= 0.0f)
        {
            debris.active = false;
            continue;
        }

        debris.velocity.y -= settings.debrisGravity * dtSeconds;
        debris.position = Vec3Add(debris.position, Vec3Scale(debris.velocity, dtSeconds));
        if (debris.position.y < debris.halfExtent)
        {
            debris.position.y = debris.halfExtent;
            if (std::fabs(debris.velocity.y) < 1.0f)
            {
                debris.velocity.y = 0.0f;
                debris.velocity.x *= 0.88f;
                debris.velocity.z *= 0.88f;
            }
            else
            {
                debris.velocity.y *= -0.24f;
                debris.velocity.x *= 0.86f;
                debris.velocity.z *= 0.86f;
            }
        }
    }
}

FractureHit FractureSystem::Raycast(Vec3 origin, Vec3 direction, float maxDistance) const
{
    FractureHit best{};
    Vec3 dir = Vec3Normalize(direction);
    if (Vec3Length(dir) <= 1e-6f)
    {
        return best;
    }

    float closest = maxDistance;
    for (std::uint32_t i = 0; i < m_chunks.size(); ++i)
    {
        const FractureChunk& chunk = m_chunks[i];
        if (!chunk.active)
        {
            continue;
        }
        float hitDistance = 0.0f;
        Vec3 hitNormal = {};
        if (!RayIntersectsChunk(origin, dir, chunk, closest, hitDistance, hitNormal))
        {
            continue;
        }
        closest = hitDistance;
        best.hit = true;
        best.distance = hitDistance;
        best.position = Vec3Add(origin, Vec3Scale(dir, hitDistance));
        best.normal = hitNormal;
        best.chunkIndex = i;
    }
    return best;
}

std::uint32_t FractureSystem::FractureAt(const FractureHit& hit, const FractureSettings& settings)
{
    if (!hit.hit)
    {
        return 0;
    }

    const float radius2 = settings.blastRadius * settings.blastRadius;
    std::uint32_t removed = 0;
    std::uint32_t debrisBudget = 0;
    for (std::uint32_t i = 0; i < m_chunks.size(); ++i)
    {
        FractureChunk& chunk = m_chunks[i];
        if (!chunk.active)
        {
            continue;
        }
        Vec3 delta = Vec3Sub(chunk.center, hit.position);
        float dist2 = Vec3Dot(delta, delta);
        if (dist2 > radius2)
        {
            continue;
        }

        chunk.active = false;
        ++removed;
        if (debrisBudget >= settings.maxDebrisPerBlast)
        {
            continue;
        }
        for (FractureDebris& debris : m_debris)
        {
            if (debris.active)
            {
                continue;
            }
            float dist = std::sqrt(std::max(dist2, 1e-6f));
            Vec3 outward = dist > 1e-4f ? Vec3Scale(delta, 1.0f / dist) : hit.normal;
            std::uint32_t seed = i * 1664525u + removed * 1013904223u;
            Vec3 jitter = Vec3Make(Hash01(seed) - 0.5f, Hash01(seed + 1u), Hash01(seed + 2u) - 0.5f);
            debris.position = chunk.center;
            debris.velocity = Vec3Add(Vec3Scale(outward, settings.debrisSpeed), Vec3Scale(jitter, settings.debrisSpeed * 0.65f));
            debris.color = chunk.color;
            debris.halfExtent = chunk.halfExtent * (0.65f + Hash01(seed + 3u) * 0.25f);
            debris.lifetime = settings.debrisLifetime * (0.75f + Hash01(seed + 4u) * 0.5f);
            debris.active = true;
            ++debrisBudget;
            break;
        }
    }
    return removed;
}

void FractureSystem::AppendDebugGeometry(DebugRenderOptions& debug) const
{
    std::uint32_t cubeIndex = debug.customCubeCount;
    for (const FractureChunk& chunk : m_chunks)
    {
        if (!chunk.active || cubeIndex >= DebugRenderOptions::kMaxCustomCubes)
        {
            continue;
        }
        debug.customCubes[cubeIndex] = Vec4Make(chunk.center.x, chunk.center.y, chunk.center.z, chunk.halfExtent);
        debug.customCubeColors[cubeIndex] = Vec4Make(chunk.color.x, chunk.color.y, chunk.color.z, 1.0f);
        ++cubeIndex;
    }
    for (const FractureDebris& debris : m_debris)
    {
        if (!debris.active || cubeIndex >= DebugRenderOptions::kMaxCustomCubes)
        {
            continue;
        }
        debug.customCubes[cubeIndex] = Vec4Make(debris.position.x, debris.position.y, debris.position.z, debris.halfExtent);
        debug.customCubeColors[cubeIndex] = Vec4Make(debris.color.x, debris.color.y, debris.color.z, 1.0f);
        ++cubeIndex;
    }
    debug.customCubeCount = cubeIndex;
}

std::uint32_t FractureSystem::ActiveChunkCount() const
{
    return static_cast<std::uint32_t>(std::count_if(m_chunks.begin(), m_chunks.end(), [](const FractureChunk& chunk) { return chunk.active; }));
}

std::uint32_t FractureSystem::ActiveDebrisCount() const
{
    return static_cast<std::uint32_t>(std::count_if(m_debris.begin(), m_debris.end(), [](const FractureDebris& debris) { return debris.active; }));
}
