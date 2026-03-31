#include "render/renderer.h"
#include "render/culling.h"

#include <algorithm>

namespace
{
bool SpheresOverlap(Vec3 aCenter, float aRadius, Vec3 bCenter, float bRadius)
{
    Vec3 delta = Vec3Sub(aCenter, bCenter);
    float combined = aRadius + bRadius;
    return Vec3Dot(delta, delta) <= combined * combined;
}
}

void VulkanRenderer::UpdateMainPassVisibility(const SceneUniforms& uniforms)
{
    m_visibleDrawItems.clear();
    ClearStaticBatchVisibility();
    if (m_drawItems.empty())
    {
        return;
    }

    Frustum frustum = ExtractFrustum(Mat4Mul(uniforms.proj, uniforms.view));
    m_cameraCullPosition = Vec3Make(
        uniforms.cameraPosition.x,
        uniforms.cameraPosition.y,
        uniforms.cameraPosition.z
    );
    for (std::uint32_t drawIndex = 0; drawIndex < m_drawItems.size(); ++drawIndex)
    {
        const DrawItem& drawItem = m_drawItems[drawIndex];
        Vec3 worldCenter = TransformPoint(drawItem.model, drawItem.localBoundsCenter);
        float worldRadius = drawItem.localBoundsRadius * MaxAxisScale(drawItem.model);
        Vec3 delta = Vec3Sub(worldCenter, m_cameraCullPosition);
        if (Vec3Dot(delta, delta) > (m_mainCullDistance + worldRadius) * (m_mainCullDistance + worldRadius))
        {
            continue;
        }
        if (SphereIntersectsFrustum(frustum, worldCenter, worldRadius))
        {
            m_visibleDrawItems.push_back(drawIndex);
        }
    }
}

void VulkanRenderer::UpdateDrawLightMasks(const SceneUniforms& uniforms)
{
    for (DrawItem& drawItem : m_drawItems)
    {
        drawItem.pointLightMask = 0;
        drawItem.spotLightMask = 0;
        drawItem.shadowedSpotLightMask = 0;

        Vec3 worldCenter = TransformPoint(drawItem.model, drawItem.localBoundsCenter);
        float worldRadius = drawItem.localBoundsRadius * MaxAxisScale(drawItem.model);
        if (worldRadius <= 0.0001f)
        {
            continue;
        }

        for (std::uint32_t i = 0; i < 4; ++i)
        {
            if (uniforms.lightPositions[i].w <= 0.0001f || uniforms.lightPositions[i].y < -999.0f)
            {
                continue;
            }
            Vec3 lightCenter = Vec3Make(uniforms.lightPositions[i].x, uniforms.lightPositions[i].y, uniforms.lightPositions[i].z);
            if (SpheresOverlap(worldCenter, worldRadius, lightCenter, uniforms.lightPositions[i].w))
            {
                drawItem.pointLightMask |= (1u << i);
            }
        }

        std::uint32_t spotCount = std::min<std::uint32_t>(static_cast<std::uint32_t>(uniforms.sceneLightCounts.x), kMaxSceneSpotLights);
        for (std::uint32_t i = 0; i < spotCount; ++i)
        {
            if (uniforms.spotLightPositions[i].w <= 0.0001f)
            {
                continue;
            }
            Vec3 lightCenter = Vec3Make(uniforms.spotLightPositions[i].x, uniforms.spotLightPositions[i].y, uniforms.spotLightPositions[i].z);
            if (SpheresOverlap(worldCenter, worldRadius, lightCenter, uniforms.spotLightPositions[i].w))
            {
                drawItem.spotLightMask |= (1u << i);
            }
        }

        std::uint32_t shadowedCount =
            std::min<std::uint32_t>(static_cast<std::uint32_t>(uniforms.sceneLightCounts.y), kMaxShadowedSpotLights);
        for (std::uint32_t i = 0; i < shadowedCount; ++i)
        {
            if (uniforms.shadowedSpotLightPositions[i].w <= 0.0001f)
            {
                continue;
            }
            Vec3 lightCenter = Vec3Make(
                uniforms.shadowedSpotLightPositions[i].x,
                uniforms.shadowedSpotLightPositions[i].y,
                uniforms.shadowedSpotLightPositions[i].z
            );
            if (SpheresOverlap(worldCenter, worldRadius, lightCenter, uniforms.shadowedSpotLightPositions[i].w))
            {
                drawItem.shadowedSpotLightMask |= (1u << i);
            }
        }
    }
}

bool VulkanRenderer::ShadowDrawItemVisible(const DrawItem& drawItem, std::uint32_t cascadeIndex) const
{
    Vec3 worldCenter = TransformPoint(drawItem.model, drawItem.localBoundsCenter);
    float worldRadius = drawItem.localBoundsRadius * MaxAxisScale(drawItem.model);
    Vec3 delta = Vec3Sub(worldCenter, m_cameraCullPosition);
    float distance2 = Vec3Dot(delta, delta);
    float maxDistance = m_shadowCullDistance;
    bool procCityBuilding = m_useProcCityPipeline && (drawItem.materialFlags & 8u) != 0u;
    if (procCityBuilding)
    {
        maxDistance = std::min(maxDistance, cascadeIndex < kSunShadowCascadeCount ? 42.0f : 24.0f);
    }
    if (cascadeIndex == 0)
    {
        maxDistance *= 0.75f;
    }
    return distance2 <= (maxDistance + worldRadius) * (maxDistance + worldRadius);
}
