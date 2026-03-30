#include "render/internal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace
{
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
    if (!m_useProcCityPipeline || !m_useProcCityTiledLighting || m_procCityDynamicLights.empty())
    {
        return;
    }

    constexpr std::uint32_t kTileSize = 32;
    std::uint32_t tileCountX = std::max<std::uint32_t>(1u, (m_swapchainExtent.width + kTileSize - 1u) / kTileSize);
    std::uint32_t tileCountY = std::max<std::uint32_t>(1u, (m_swapchainExtent.height + kTileSize - 1u) / kTileSize);
    std::uint32_t totalTiles = std::min<std::uint32_t>(tileCountX * tileCountY, kMaxProcCityLightTiles);
    m_procCityLightTiles.resize(totalTiles);
    std::vector<std::vector<std::uint32_t>> tileBuckets(totalTiles);

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

        Vec4 clip = Mat4MulVec4(uniforms.proj, view);
        if (std::fabs(clip.w) <= 1e-6f)
        {
            continue;
        }
        constexpr float kNearPlaneDepth = 0.1f;
        constexpr float kScreenPadPixels = 2.0f;
        const std::array<Vec3, 15> sampleOffsets = {
            Vec3Make(0.0f, 0.0f, 0.0f),
            Vec3Make( lightRange, 0.0f, 0.0f),
            Vec3Make(-lightRange, 0.0f, 0.0f),
            Vec3Make(0.0f,  lightRange, 0.0f),
            Vec3Make(0.0f, -lightRange, 0.0f),
            Vec3Make(0.0f, 0.0f,  lightRange),
            Vec3Make(0.0f, 0.0f, -lightRange),
            Vec3Make( lightRange,  lightRange,  lightRange),
            Vec3Make( lightRange,  lightRange, -lightRange),
            Vec3Make( lightRange, -lightRange,  lightRange),
            Vec3Make( lightRange, -lightRange, -lightRange),
            Vec3Make(-lightRange,  lightRange,  lightRange),
            Vec3Make(-lightRange,  lightRange, -lightRange),
            Vec3Make(-lightRange, -lightRange,  lightRange),
            Vec3Make(-lightRange, -lightRange, -lightRange),
        };

        float minScreenX = static_cast<float>(m_swapchainExtent.width);
        float minScreenY = static_cast<float>(m_swapchainExtent.height);
        float maxScreenX = 0.0f;
        float maxScreenY = 0.0f;
        bool anyProjected = false;
        for (const Vec3& offset : sampleOffsets)
        {
            Vec4 sampleView = Vec4Make(
                view.x + offset.x,
                view.y + offset.y,
                view.z + offset.z,
                1.0f);
            if (-sampleView.z < kNearPlaneDepth)
            {
                sampleView.z = -kNearPlaneDepth;
            }

            Vec4 sampleClip = Mat4MulVec4(uniforms.proj, sampleView);
            if (std::fabs(sampleClip.w) <= 1e-6f)
            {
                continue;
            }

            float sampleInvW = 1.0f / sampleClip.w;
            float ndcX = sampleClip.x * sampleInvW;
            float ndcY = sampleClip.y * sampleInvW;
            float screenX = (ndcX * 0.5f + 0.5f) * static_cast<float>(m_swapchainExtent.width);
            float screenY = (ndcY * 0.5f + 0.5f) * static_cast<float>(m_swapchainExtent.height);
            minScreenX = std::min(minScreenX, screenX);
            minScreenY = std::min(minScreenY, screenY);
            maxScreenX = std::max(maxScreenX, screenX);
            maxScreenY = std::max(maxScreenY, screenY);
            anyProjected = true;
        }
        if (!anyProjected)
        {
            continue;
        }

        minScreenX = std::max(0.0f, minScreenX - kScreenPadPixels);
        minScreenY = std::max(0.0f, minScreenY - kScreenPadPixels);
        maxScreenX = std::min(static_cast<float>(m_swapchainExtent.width), maxScreenX + kScreenPadPixels);
        maxScreenY = std::min(static_cast<float>(m_swapchainExtent.height), maxScreenY + kScreenPadPixels);

        int minTileX = std::max(0, static_cast<int>(std::floor(minScreenX / static_cast<float>(kTileSize))));
        int maxTileX = std::min(static_cast<int>(tileCountX) - 1, static_cast<int>(std::floor(maxScreenX / static_cast<float>(kTileSize))));
        int minTileY = std::max(0, static_cast<int>(std::floor(minScreenY / static_cast<float>(kTileSize))));
        int maxTileY = std::min(static_cast<int>(tileCountY) - 1, static_cast<int>(std::floor(maxScreenY / static_cast<float>(kTileSize))));
        if (minTileX > maxTileX || minTileY > maxTileY)
        {
            continue;
        }

        for (int ty = minTileY; ty <= maxTileY; ++ty)
        {
            for (int tx = minTileX; tx <= maxTileX; ++tx)
            {
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
