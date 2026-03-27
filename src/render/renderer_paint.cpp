#include "render/internal.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace vulkan_renderer_internal;

namespace
{
std::uint32_t FindDrawItemIndex(
    const std::vector<VulkanRenderer::DrawItem>& drawItems,
    std::uint32_t entityIndex,
    std::uint32_t primitiveIndex
)
{
    for (std::uint32_t i = 0; i < drawItems.size(); ++i)
    {
        const auto& drawItem = drawItems[i];
        if (drawItem.entityIndex == entityIndex && drawItem.primitiveIndex == primitiveIndex)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

float UvDistance(Vec2 lhs, Vec2 rhs)
{
    float dx = lhs.x - rhs.x;
    float dy = lhs.y - rhs.y;
    return std::sqrt(dx * dx + dy * dy);
}

float SmoothStep(float edge0, float edge1, float x)
{
    float t = std::clamp((x - edge0) / std::max(edge1 - edge0, 1e-5f), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

void VulkanRenderer::AppendPersistentPaint(const PaintSplatSpawn& splat)
{
    if (splat.entityIndex == UINT32_MAX || splat.primitiveIndex == UINT32_MAX)
    {
        return;
    }

    std::uint32_t drawIndex = FindDrawItemIndex(m_drawItems, splat.entityIndex, splat.primitiveIndex);
    if (drawIndex == UINT32_MAX || drawIndex >= m_paintLayers.size())
    {
        return;
    }

    PaintLayer& layer = m_paintLayers[drawIndex];
    if (!layer.allocated)
    {
        layer.allocated = true;
        layer.descriptorDirty = true;
        layer.imageInitialized = false;
        layer.pixels.assign(kPaintTextureSize * kPaintTextureSize * 4u, 0u);
        m_paintImages[drawIndex] = CreateImage2D(
            m_physicalDevice,
            m_device,
            kPaintTextureSize,
            kPaintTextureSize,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
    }

    float uvScale = std::max(splat.uvWorldScale, 0.001f);
    float uvRadius = std::clamp(splat.radius / uvScale, 0.0025f, 0.45f);
    std::uint32_t channel = std::min(splat.maskChannel, 3u);
    float strength = std::clamp(splat.maskStrength, 0.0f, 1.0f);
    float centerX = splat.uv.x * static_cast<float>(kPaintTextureSize - 1);
    float centerY = splat.uv.y * static_cast<float>(kPaintTextureSize - 1);
    int minX = std::max(0, static_cast<int>(std::floor(centerX - uvRadius * kPaintTextureSize)));
    int maxX = std::min(static_cast<int>(kPaintTextureSize - 1), static_cast<int>(std::ceil(centerX + uvRadius * kPaintTextureSize)));
    int minY = std::max(0, static_cast<int>(std::floor(centerY - uvRadius * kPaintTextureSize)));
    int maxY = std::min(static_cast<int>(kPaintTextureSize - 1), static_cast<int>(std::ceil(centerY + uvRadius * kPaintTextureSize)));

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(kPaintTextureSize - 1);
            float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(kPaintTextureSize - 1);
            float dist = UvDistance(Vec2Make(u, v), splat.uv);
            if (dist >= uvRadius)
            {
                continue;
            }

            float radial = 1.0f - dist / std::max(uvRadius, 1e-5f);
            float noise = 0.5f + 0.5f * std::sin(
                u * 41.0f +
                v * 29.0f +
                splat.color.x * 7.0f +
                splat.color.y * 11.0f +
                splat.color.z * 13.0f
            );
            float mask = std::clamp(radial + (noise - 0.5f) * 0.35f, 0.0f, 1.0f);
            mask = SmoothStep(0.12f, 0.95f, mask);
            if (mask <= 0.0f)
            {
                continue;
            }

            std::size_t pixelIndex = static_cast<std::size_t>(y) * kPaintTextureSize * 4u + static_cast<std::size_t>(x) * 4u;
            float dst = static_cast<float>(layer.pixels[pixelIndex + channel]) / 255.0f;
            float src = std::clamp(mask * strength, 0.0f, 1.0f);
            float out = std::clamp(std::max(dst, src), 0.0f, 1.0f);
            layer.pixels[pixelIndex + channel] = static_cast<std::uint8_t>(out * 255.0f);
        }
    }

    layer.dirty = true;
    ++layer.hitCount;
    ++m_accumulatedPaintHitCount;
}

void VulkanRenderer::ResetAccumulatedPaint()
{
    for (std::uint32_t i = 0; i < m_paintLayers.size(); ++i)
    {
        PaintLayer& layer = m_paintLayers[i];
        layer = {};
        if (i < m_paintImages.size())
        {
            DestroyImage(m_device, m_paintImages[i]);
            m_paintImages[i] = {};
        }
        layer.descriptorDirty = true;
    }
    m_accumulatedPaintHitCount = 0;
}

std::uint32_t VulkanRenderer::GetAccumulatedPaintHitCount() const
{
    return m_accumulatedPaintHitCount;
}

void VulkanRenderer::FlushDirtyPaintTextures()
{
    if (m_paintUploadBuffer.mapped == nullptr)
    {
        return;
    }

    for (std::uint32_t i = 0; i < m_paintLayers.size(); ++i)
    {
        PaintLayer& layer = m_paintLayers[i];
        if (!layer.allocated || !layer.dirty || i >= m_paintImages.size() || m_paintImages[i].image == VK_NULL_HANDLE)
        {
            if (layer.descriptorDirty)
            {
                UpdatePaintDescriptorSet(i);
                layer.descriptorDirty = false;
            }
            continue;
        }

        if (layer.dirty)
        {
            std::memcpy(m_paintUploadBuffer.mapped, layer.pixels.data(), layer.pixels.size());
            TransitionImageLayout(
                m_device,
                m_graphicsQueue,
                m_commandPool,
                m_paintImages[i].image,
                VK_IMAGE_ASPECT_COLOR_BIT,
                layer.imageInitialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
            );
            CopyBufferToImage(
                m_device,
                m_graphicsQueue,
                m_commandPool,
                m_paintUploadBuffer.buffer,
                m_paintImages[i].image,
                kPaintTextureSize,
                kPaintTextureSize
            );
            TransitionImageLayout(
                m_device,
                m_graphicsQueue,
                m_commandPool,
                m_paintImages[i].image,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
            );
            layer.dirty = false;
            layer.imageInitialized = true;
        }
        if (layer.descriptorDirty)
        {
            UpdatePaintDescriptorSet(i);
            layer.descriptorDirty = false;
        }
    }
}
