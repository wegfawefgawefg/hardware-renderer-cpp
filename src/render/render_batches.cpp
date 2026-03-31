#include "render/internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace
{
struct TileFrustumPlane
{
    Vec3 normal = {};
    float d = 0.0f;
};

float ProcCityEffectiveTileRadius(float range, float intensity, float contributionCutoff)
{
    (void)intensity;
    (void)contributionCutoff;
    return std::max(range, 0.0f);
}

bool ProjectSphereBoundsToScreen(
    const Mat4& proj,
    Vec4 viewCenter,
    float radius,
    std::uint32_t width,
    std::uint32_t height,
    float& outCenterX,
    float& outCenterY,
    float& outRadiusPixels,
    float& outMinX,
    float& outMinY,
    float& outMaxX,
    float& outMaxY)
{
    const float vz = -viewCenter.z;
    if (radius <= 0.0f || vz <= radius + 1e-4f)
    {
        return false;
    }

    auto solveAxis = [vz, radius](float axis, float& outMinSlope, float& outMaxSlope) -> bool {
        const float denom = vz * vz - radius * radius;
        if (denom <= 1e-6f)
        {
            return false;
        }
        const float rootTerm = axis * axis + vz * vz - radius * radius;
        if (rootTerm <= 1e-6f)
        {
            return false;
        }
        const float root = radius * std::sqrt(rootTerm);
        outMinSlope = (axis * vz - root) / denom;
        outMaxSlope = (axis * vz + root) / denom;
        if (outMinSlope > outMaxSlope)
        {
            std::swap(outMinSlope, outMaxSlope);
        }
        return true;
    };

    float minSlopeX = 0.0f;
    float maxSlopeX = 0.0f;
    float minSlopeY = 0.0f;
    float maxSlopeY = 0.0f;
    if (!solveAxis(viewCenter.x, minSlopeX, maxSlopeX) ||
        !solveAxis(viewCenter.y, minSlopeY, maxSlopeY))
    {
        return false;
    }

    const float ndcMinX = proj.m[0] * minSlopeX;
    const float ndcMaxX = proj.m[0] * maxSlopeX;
    const float ndcMinY = proj.m[5] * minSlopeY;
    const float ndcMaxY = proj.m[5] * maxSlopeY;

    outMinX = (std::min(ndcMinX, ndcMaxX) * 0.5f + 0.5f) * static_cast<float>(width);
    outMaxX = (std::max(ndcMinX, ndcMaxX) * 0.5f + 0.5f) * static_cast<float>(width);
    outMinY = (std::min(ndcMinY, ndcMaxY) * 0.5f + 0.5f) * static_cast<float>(height);
    outMaxY = (std::max(ndcMinY, ndcMaxY) * 0.5f + 0.5f) * static_cast<float>(height);

    outCenterX = 0.5f * (outMinX + outMaxX);
    outCenterY = 0.5f * (outMinY + outMaxY);
    const float radiusX = 0.5f * (outMaxX - outMinX);
    const float radiusY = 0.5f * (outMaxY - outMinY);
    outRadiusPixels = 0.5f * (radiusX + radiusY);
    return outRadiusPixels > 0.0f;
}

TileFrustumPlane MakeTileFrustumPlane(Vec3 a, Vec3 b)
{
    return TileFrustumPlane{Vec3Normalize(Vec3Cross(a, b)), 0.0f};
}

bool SphereIntersectsTileFrustum(
    const Mat4& proj,
    Vec4 viewCenter,
    float radius,
    int tileX,
    int tileY,
    std::uint32_t tileSize,
    std::uint32_t width,
    std::uint32_t height)
{
    constexpr float kNearPlane = 0.1f;

    auto pixelToNearPoint = [&](float px, float py) -> Vec3 {
        float ndcX = (px / static_cast<float>(width)) * 2.0f - 1.0f;
        float ndcY = (py / static_cast<float>(height)) * 2.0f - 1.0f;
        return Vec3Make(
            ndcX * kNearPlane / proj.m[0],
            ndcY * kNearPlane / proj.m[5],
            -kNearPlane);
    };

    float minPx = static_cast<float>(tileX) * static_cast<float>(tileSize);
    float minPy = static_cast<float>(tileY) * static_cast<float>(tileSize);
    float maxPx = std::min(minPx + static_cast<float>(tileSize), static_cast<float>(width));
    float maxPy = std::min(minPy + static_cast<float>(tileSize), static_cast<float>(height));

    Vec3 p00 = pixelToNearPoint(minPx, minPy);
    Vec3 p10 = pixelToNearPoint(maxPx, minPy);
    Vec3 p01 = pixelToNearPoint(minPx, maxPy);
    Vec3 p11 = pixelToNearPoint(maxPx, maxPy);

    const std::array<TileFrustumPlane, 4> planes = {
        MakeTileFrustumPlane(p01, p00),
        MakeTileFrustumPlane(p11, p10),
        MakeTileFrustumPlane(p00, p10),
        MakeTileFrustumPlane(p01, p11),
    };

    Vec3 center = Vec3Make(viewCenter.x, viewCenter.y, viewCenter.z);
    for (const TileFrustumPlane& plane : planes)
    {
        float signedDistance = Vec3Dot(plane.normal, center) + plane.d;
        if (signedDistance < -radius)
        {
            return false;
        }
    }
    return true;
}

Vec3 TransformPointLocal(Mat4 m, Vec3 p)
{
    Vec4 out = Mat4MulVec4(m, Vec4Make(p.x, p.y, p.z, 1.0f));
    return Vec3Make(out.x, out.y, out.z);
}

float MaxAxisScaleLocal(Mat4 m)
{
    Vec3 x = Vec3Make(m.m[0], m.m[1], m.m[2]);
    Vec3 y = Vec3Make(m.m[4], m.m[5], m.m[6]);
    Vec3 z = Vec3Make(m.m[8], m.m[9], m.m[10]);
    return std::max(Vec3Length(x), std::max(Vec3Length(y), Vec3Length(z)));
}
}

void VulkanRenderer::ClearStaticBatchVisibility()
{
    for (std::vector<std::uint32_t>& batchItems : m_visibleStaticBatchDrawItems)
    {
        batchItems.clear();
    }
}

void VulkanRenderer::BuildVisibleStaticInstances()
{
    m_visibleStaticInstances.clear();
    m_visibleProcCityDynamicLightIndices.clear();
    m_staticBatchFirstInstance.assign(m_staticBatches.size(), 0);
    for (std::size_t batchIndex = 0; batchIndex < m_visibleStaticBatchDrawItems.size(); ++batchIndex)
    {
        m_staticBatchFirstInstance[batchIndex] = static_cast<std::uint32_t>(m_visibleStaticInstances.size());
        for (std::uint32_t drawIndex : m_visibleStaticBatchDrawItems[batchIndex])
        {
            const DrawItem& drawItem = m_drawItems[drawIndex];
            StaticInstanceGpu instance{};
            instance.model = drawItem.model;
            instance.pointLightMask = drawItem.pointLightMask;
            instance.spotLightMask = drawItem.spotLightMask;
            instance.shadowedSpotLightMask = drawItem.shadowedSpotLightMask;
            instance.materialFlags = drawItem.materialFlags;
            if (drawItem.flipNormalY)
            {
                instance.materialFlags |= 1u;
            }
            if (m_useProcCityPipeline && !m_useProcCityTiledLighting && !m_procCityDynamicLights.empty())
            {
                Vec3 worldCenter = TransformPointLocal(drawItem.model, drawItem.localBoundsCenter);
                float worldRadius = drawItem.localBoundsRadius * MaxAxisScaleLocal(drawItem.model);
                struct Candidate
                {
                    float dist2 = 0.0f;
                    std::uint32_t lightIndex = 0;
                };
                std::array<Candidate, kMaxProcCityLightRefsPerInstance> nearest{};
                for (Candidate& candidate : nearest)
                {
                    candidate.dist2 = std::numeric_limits<float>::max();
                    candidate.lightIndex = 0;
                }
                std::uint32_t candidateCount = 0;
                for (std::uint32_t lightIndex = 0; lightIndex < m_procCityDynamicLights.size(); ++lightIndex)
                {
                    const DynamicPointLightGpu& light = m_procCityDynamicLights[lightIndex];
                    Vec3 lightPosition = Vec3Make(
                        light.positionRange.x,
                        light.positionRange.y,
                        light.positionRange.z);
                    float influenceRange = light.positionRange.w + worldRadius;
                    Vec3 delta = Vec3Sub(lightPosition, worldCenter);
                    float dist2 = Vec3Dot(delta, delta);
                    if (dist2 > influenceRange * influenceRange)
                    {
                        continue;
                    }

                    std::uint32_t insertIndex = std::min(candidateCount, kMaxProcCityLightRefsPerInstance - 1u);
                    while (insertIndex > 0 && dist2 < nearest[insertIndex - 1].dist2)
                    {
                        if (insertIndex < kMaxProcCityLightRefsPerInstance)
                        {
                            nearest[insertIndex] = nearest[insertIndex - 1];
                        }
                        --insertIndex;
                    }
                    nearest[insertIndex] = Candidate{dist2, lightIndex};
                    candidateCount = std::min(candidateCount + 1u, kMaxProcCityLightRefsPerInstance);
                }

                instance.localLightListOffset = static_cast<std::uint32_t>(m_visibleProcCityDynamicLightIndices.size());
                instance.localLightCount = candidateCount;
                for (std::uint32_t i = 0; i < candidateCount; ++i)
                {
                    m_visibleProcCityDynamicLightIndices.push_back(nearest[i].lightIndex);
                }
            }
            m_visibleStaticInstances.push_back(instance);
        }
    }

    if (!m_visibleStaticInstances.empty())
    {
        std::memcpy(
            m_staticInstanceBuffer.mapped,
            m_visibleStaticInstances.data(),
            sizeof(StaticInstanceGpu) * m_visibleStaticInstances.size());
    }
    if (!m_visibleProcCityDynamicLightIndices.empty())
    {
        std::memcpy(
            m_procCityDynamicLightIndexBuffer.mapped,
            m_visibleProcCityDynamicLightIndices.data(),
            sizeof(std::uint32_t) * m_visibleProcCityDynamicLightIndices.size());
    }
}

void VulkanRenderer::BuildShadowVisibleStaticInstances(std::uint32_t cascadeIndex)
{
    for (std::vector<std::uint32_t>& batchItems : m_shadowVisibleStaticBatchDrawItems)
    {
        batchItems.clear();
    }

    for (std::uint32_t drawIndex = 0; drawIndex < m_drawItems.size(); ++drawIndex)
    {
        const DrawItem& drawItem = m_drawItems[drawIndex];
        bool hasUniquePaint = drawIndex < m_paintLayers.size() && m_paintLayers[drawIndex].allocated;
        if (!drawItem.castsShadows || !drawItem.batchedStatic || hasUniquePaint)
        {
            continue;
        }
        if (!ShadowDrawItemVisible(drawItem, cascadeIndex))
        {
            continue;
        }
        if (drawItem.staticBatchIndex < m_shadowVisibleStaticBatchDrawItems.size())
        {
            m_shadowVisibleStaticBatchDrawItems[drawItem.staticBatchIndex].push_back(drawIndex);
        }
    }

    m_shadowVisibleStaticInstances.clear();
    m_shadowStaticBatchFirstInstance.assign(m_staticBatches.size(), 0);
    for (std::size_t batchIndex = 0; batchIndex < m_shadowVisibleStaticBatchDrawItems.size(); ++batchIndex)
    {
        m_shadowStaticBatchFirstInstance[batchIndex] = static_cast<std::uint32_t>(m_shadowVisibleStaticInstances.size());
        for (std::uint32_t drawIndex : m_shadowVisibleStaticBatchDrawItems[batchIndex])
        {
            StaticInstanceGpu instance{};
            instance.model = m_drawItems[drawIndex].model;
            m_shadowVisibleStaticInstances.push_back(instance);
        }
    }

    if (!m_shadowVisibleStaticInstances.empty())
    {
        std::memcpy(
            m_shadowStaticInstanceBuffer.mapped,
            m_shadowVisibleStaticInstances.data(),
            sizeof(StaticInstanceGpu) * m_shadowVisibleStaticInstances.size());
    }
}

void VulkanRenderer::SetProcCityDynamicLights(std::span<const DynamicPointLightGpu> lights)
{
    std::size_t count = std::min<std::size_t>(lights.size(), kMaxProcCityDynamicLights);
    m_procCityDynamicLights.assign(lights.begin(), lights.begin() + count);
    if (!m_procCityDynamicLights.empty())
    {
        std::memcpy(
            m_procCityDynamicLightBuffer.mapped,
            m_procCityDynamicLights.data(),
            sizeof(DynamicPointLightGpu) * m_procCityDynamicLights.size());
    }
}

void VulkanRenderer::BuildProcCityTiledLightLists(const SceneUniforms& uniforms)
{
    m_procCityLightTiles.clear();
    m_procCityTileLightIndices.clear();
    m_procCityMaxTileLightCount = 0;
    if (!m_useProcCityPipeline || !m_useProcCityTiledLighting)
    {
        return;
    }

    constexpr std::uint32_t kTileSize = 32;
    std::uint32_t tileCountX = std::max<std::uint32_t>(1u, (m_swapchainExtent.width + kTileSize - 1u) / kTileSize);
    std::uint32_t tileCountY = std::max<std::uint32_t>(1u, (m_swapchainExtent.height + kTileSize - 1u) / kTileSize);
    std::uint32_t totalTiles = std::min<std::uint32_t>(tileCountX * tileCountY, kMaxProcCityLightTiles);
    m_procCityLightTiles.resize(totalTiles);
    if (m_procCityDynamicLights.empty())
    {
        if (!m_procCityLightTiles.empty())
        {
            std::memset(
                m_procCityLightTileBuffer.mapped,
                0,
                sizeof(ProcCityLightTileGpu) * m_procCityLightTiles.size());
        }
        return;
    }
    std::vector<std::vector<std::uint32_t>> tileBuckets(totalTiles);
    bool frustumMode = m_procCityTiledOccupancyMode == 1u;

    for (std::uint32_t lightIndex = 0; lightIndex < m_procCityDynamicLights.size(); ++lightIndex)
    {
        const DynamicPointLightGpu& light = m_procCityDynamicLights[lightIndex];
        float lightRange = light.positionRange.w;
        Vec4 world = Vec4Make(light.positionRange.x, light.positionRange.y, light.positionRange.z, 1.0f);
        Vec4 view = Mat4MulVec4(uniforms.view, world);
        float depth = -view.z;
        if (depth + lightRange <= 0.1f)
        {
            continue;
        }

        float screenCenterX = 0.0f;
        float screenCenterY = 0.0f;
        float radiusPixels = 0.0f;
        float minScreenX = 0.0f;
        float minScreenY = 0.0f;
        float maxScreenX = 0.0f;
        float maxScreenY = 0.0f;
        int minTileX = 0;
        int maxTileX = static_cast<int>(tileCountX) - 1;
        int minTileY = 0;
        int maxTileY = static_cast<int>(tileCountY) - 1;
        if (!frustumMode)
        {
            if (depth <= lightRange + 0.1f)
            {
                screenCenterX = 0.5f * static_cast<float>(m_swapchainExtent.width);
                screenCenterY = 0.5f * static_cast<float>(m_swapchainExtent.height);
                minScreenX = 0.0f;
                minScreenY = 0.0f;
                maxScreenX = static_cast<float>(m_swapchainExtent.width);
                maxScreenY = static_cast<float>(m_swapchainExtent.height);
                radiusPixels = 0.5f * std::sqrt(
                    static_cast<float>(m_swapchainExtent.width) * static_cast<float>(m_swapchainExtent.width) +
                    static_cast<float>(m_swapchainExtent.height) * static_cast<float>(m_swapchainExtent.height));
            }
            else if (!ProjectSphereBoundsToScreen(
                         uniforms.proj,
                         view,
                         lightRange,
                         m_swapchainExtent.width,
                         m_swapchainExtent.height,
                         screenCenterX,
                         screenCenterY,
                         radiusPixels,
                         minScreenX,
                         minScreenY,
                         maxScreenX,
                         maxScreenY))
            {
                continue;
            }
            constexpr float kScreenPadPixels = 2.0f;
            minScreenX = std::max(0.0f, minScreenX - kScreenPadPixels);
            minScreenY = std::max(0.0f, minScreenY - kScreenPadPixels);
            maxScreenX = std::min(static_cast<float>(m_swapchainExtent.width), maxScreenX + kScreenPadPixels);
            maxScreenY = std::min(static_cast<float>(m_swapchainExtent.height), maxScreenY + kScreenPadPixels);

            minTileX = std::max(0, static_cast<int>(std::floor(minScreenX / static_cast<float>(kTileSize))));
            maxTileX = std::min(static_cast<int>(tileCountX) - 1, static_cast<int>(std::floor(maxScreenX / static_cast<float>(kTileSize))));
            minTileY = std::max(0, static_cast<int>(std::floor(minScreenY / static_cast<float>(kTileSize))));
            maxTileY = std::min(static_cast<int>(tileCountY) - 1, static_cast<int>(std::floor(maxScreenY / static_cast<float>(kTileSize))));
        }
        if (minTileX > maxTileX || minTileY > maxTileY)
        {
            continue;
        }

        for (int ty = minTileY; ty <= maxTileY; ++ty)
        {
            for (int tx = minTileX; tx <= maxTileX; ++tx)
            {
                if (frustumMode)
                {
                    if (!SphereIntersectsTileFrustum(
                            uniforms.proj,
                            view,
                            lightRange,
                            tx,
                            ty,
                            kTileSize,
                            m_swapchainExtent.width,
                            m_swapchainExtent.height))
                    {
                        continue;
                    }
                }
                else
                {
                    float tileMinX = static_cast<float>(tx) * static_cast<float>(kTileSize);
                    float tileMinY = static_cast<float>(ty) * static_cast<float>(kTileSize);
                    float tileMaxX = tileMinX + static_cast<float>(kTileSize);
                    float tileMaxY = tileMinY + static_cast<float>(kTileSize);
                    float nearestX = std::clamp(screenCenterX, tileMinX, tileMaxX);
                    float nearestY = std::clamp(screenCenterY, tileMinY, tileMaxY);
                    float dx = nearestX - screenCenterX;
                    float dy = nearestY - screenCenterY;
                    if (dx * dx + dy * dy > radiusPixels * radiusPixels)
                    {
                        continue;
                    }
                }
                std::uint32_t tileIndex = static_cast<std::uint32_t>(ty) * tileCountX + static_cast<std::uint32_t>(tx);
                if (tileIndex >= m_procCityLightTiles.size())
                {
                    continue;
                }
                tileBuckets[tileIndex].push_back(lightIndex);
            }
        }
    }

    for (std::size_t tileIndex = 0; tileIndex < tileBuckets.size(); ++tileIndex)
    {
        ProcCityLightTileGpu& tile = m_procCityLightTiles[tileIndex];
        tile.lightOffset = static_cast<std::uint32_t>(m_procCityTileLightIndices.size());
        tile.lightCount = 0;
        for (std::uint32_t lightIndex : tileBuckets[tileIndex])
        {
            if (m_procCityTileLightIndices.size() >= kMaxProcCityTileLightRefs)
            {
                break;
            }
            m_procCityTileLightIndices.push_back(lightIndex);
            ++tile.lightCount;
        }
        m_procCityMaxTileLightCount = std::max(m_procCityMaxTileLightCount, tile.lightCount);
    }

    if (!m_procCityLightTiles.empty())
    {
        std::memcpy(
            m_procCityLightTileBuffer.mapped,
            m_procCityLightTiles.data(),
            sizeof(ProcCityLightTileGpu) * m_procCityLightTiles.size());
    }
    if (!m_procCityTileLightIndices.empty())
    {
        std::memcpy(
            m_procCityTileLightIndexBuffer.mapped,
            m_procCityTileLightIndices.data(),
            sizeof(std::uint32_t) * m_procCityTileLightIndices.size());
    }
}
