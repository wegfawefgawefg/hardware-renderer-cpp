#include "render/internal.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <array>
#include <cstring>
#include <filesystem>

#include "assets/texture_loader.h"

using namespace vulkan_renderer_internal;

namespace
{
TextureData GetPrimitiveTexture(
    const SceneData& scene,
    const EntityData& entity,
    const PrimitiveData& primitive
)
{
    const ModelData& model = scene.models[entity.modelIndex];
    if (primitive.materialIndex < model.materials.size())
    {
        const MaterialData& material = model.materials[primitive.materialIndex];
        if (material.textureIndex >= 0 &&
            static_cast<std::size_t>(material.textureIndex) < model.textures.size())
        {
            return model.textures[static_cast<std::size_t>(material.textureIndex)];
        }
    }

    return MakeSolidTexture(255, 255, 255);
}

TextureData GetPrimitiveNormalTexture(
    const SceneData& scene,
    const EntityData& entity,
    const PrimitiveData& primitive
)
{
    const ModelData& model = scene.models[entity.modelIndex];
    if (primitive.materialIndex < model.materials.size())
    {
        const MaterialData& material = model.materials[primitive.materialIndex];
        if (material.normalTextureIndex >= 0 &&
            static_cast<std::size_t>(material.normalTextureIndex) < model.textures.size())
        {
            return model.textures[static_cast<std::size_t>(material.normalTextureIndex)];
        }
    }

    return MakeSolidTexture(128, 128, 255);
}

ImageResource CreateSolidColorImage(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkQueue graphicsQueue,
    VkCommandPool commandPool,
    std::uint32_t width,
    std::uint32_t height,
    std::uint8_t r,
    std::uint8_t g,
    std::uint8_t b,
    std::uint8_t a
)
{
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0u);
    for (std::size_t i = 0; i < pixels.size(); i += 4)
    {
        pixels[i + 0] = r;
        pixels[i + 1] = g;
        pixels[i + 2] = b;
        pixels[i + 3] = a;
    }

    VkDeviceSize imageSize = static_cast<VkDeviceSize>(pixels.size());
    BufferResource stagingBuffer = CreateBuffer(
        physicalDevice,
        device,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    std::memcpy(stagingBuffer.mapped, pixels.data(), static_cast<std::size_t>(imageSize));

    ImageResource image = CreateImage2D(
        physicalDevice,
        device,
        width,
        height,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    TransitionImageLayout(
        device,
        graphicsQueue,
        commandPool,
        image.image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    CopyBufferToImage(device, graphicsQueue, commandPool, stagingBuffer.buffer, image.image, width, height);
    TransitionImageLayout(
        device,
        graphicsQueue,
        commandPool,
        image.image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

    DestroyBuffer(device, stagingBuffer);
    return image;
}

}

void VulkanRenderer::CreateTextureResources(const SceneData& scene)
{
    auto uploadTexture = [&](const TextureData& texture, VkFormat format) -> ImageResource {
        VkDeviceSize imageSize = static_cast<VkDeviceSize>(texture.pixels.size());

        BufferResource stagingBuffer = CreateBuffer(
            m_physicalDevice,
            m_device,
            imageSize,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            true
        );
        std::memcpy(stagingBuffer.mapped, texture.pixels.data(), static_cast<std::size_t>(imageSize));

        ImageResource image = CreateImage2D(
            m_physicalDevice,
            m_device,
            texture.width,
            texture.height,
            format,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        TransitionImageLayout(
            m_device,
            m_graphicsQueue,
            m_commandPool,
            image.image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        CopyBufferToImage(
            m_device,
            m_graphicsQueue,
            m_commandPool,
            stagingBuffer.buffer,
            image.image,
            texture.width,
            texture.height
        );
        TransitionImageLayout(
            m_device,
            m_graphicsQueue,
            m_commandPool,
            image.image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );

        DestroyBuffer(m_device, stagingBuffer);
        return image;
    };

    for (const EntityData& entity : scene.entities)
    {
        if (entity.modelIndex >= scene.models.size())
        {
            continue;
        }

        const ModelData& model = scene.models[entity.modelIndex];
        for (const PrimitiveData& primitive : model.primitives)
        {
            TextureData texture = GetPrimitiveTexture(scene, entity, primitive);
            m_textureImages.push_back(uploadTexture(texture, VK_FORMAT_R8G8B8A8_SRGB));
            TextureData normalTexture = GetPrimitiveNormalTexture(scene, entity, primitive);
            m_normalTextureImages.push_back(uploadTexture(normalTexture, VK_FORMAT_R8G8B8A8_UNORM));
        }
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.maxAnisotropy = 8.0f;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    CheckVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_textureSampler), "vkCreateSampler");

    VkSamplerCreateInfo effectSamplerInfo{};
    effectSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    effectSamplerInfo.magFilter = VK_FILTER_LINEAR;
    effectSamplerInfo.minFilter = VK_FILTER_LINEAR;
    effectSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    effectSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    effectSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    effectSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    effectSamplerInfo.maxAnisotropy = 1.0f;
    effectSamplerInfo.minLod = 0.0f;
    effectSamplerInfo.maxLod = 0.0f;
    CheckVk(vkCreateSampler(m_device, &effectSamplerInfo, nullptr, &m_effectSampler), "vkCreateSampler(effect)");

    TextureData effectPattern = LoadTexture(std::string(HARDWARE_RENDERER_ASSETS_ROOT) + "/waterdrops.png");
    m_effectPatternImage = uploadTexture(effectPattern, VK_FORMAT_R8G8B8A8_UNORM);
    m_flatNormalImage = CreateSolidColorImage(
        m_physicalDevice,
        m_device,
        m_graphicsQueue,
        m_commandPool,
        1,
        1,
        128,
        128,
        255,
        255
    );

    VkSamplerCreateInfo paintSamplerInfo{};
    paintSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    paintSamplerInfo.magFilter = VK_FILTER_LINEAR;
    paintSamplerInfo.minFilter = VK_FILTER_LINEAR;
    paintSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    paintSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    paintSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    paintSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    paintSamplerInfo.maxAnisotropy = 1.0f;
    paintSamplerInfo.minLod = 0.0f;
    paintSamplerInfo.maxLod = 0.0f;
    CheckVk(vkCreateSampler(m_device, &paintSamplerInfo, nullptr, &m_paintSampler), "vkCreateSampler(paint)");

    m_blankPaintImage = CreateSolidColorImage(
        m_physicalDevice,
        m_device,
        m_graphicsQueue,
        m_commandPool,
        1,
        1,
        0,
        0,
        0,
        0
    );
    m_paintImages.assign(m_drawItems.size(), {});
}

void VulkanRenderer::CreateOverlayResources(const text::System* textSystem)
{
    if (textSystem == nullptr || textSystem->atlasCount == 0)
    {
        throw std::runtime_error("CreateOverlayResources requires initialized text system");
    }
    std::size_t maxBytes = 0;
    for (std::uint32_t i = 0; i < textSystem->atlasCount; ++i)
    {
        maxBytes = std::max(maxBytes, textSystem->atlases[i].texture.pixels.size());
    }
    m_overlayUploadBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        static_cast<VkDeviceSize>(std::max<std::size_t>(maxBytes, 4)),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_overlayAtlasCount = textSystem->atlasCount;
    for (std::uint32_t i = 0; i < m_overlayAtlasCount; ++i)
    {
        const TextureData& atlas = textSystem->atlases[i].texture;
        std::memcpy(m_overlayUploadBuffer.mapped, atlas.pixels.data(), atlas.pixels.size());
        m_overlayImages[i] = CreateImage2D(
            m_physicalDevice,
            m_device,
            atlas.width,
            atlas.height,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );

        TransitionImageLayout(
            m_device,
            m_graphicsQueue,
            m_commandPool,
            m_overlayImages[i].image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        CopyBufferToImage(
            m_device,
            m_graphicsQueue,
            m_commandPool,
            m_overlayUploadBuffer.buffer,
            m_overlayImages[i].image,
            atlas.width,
            atlas.height
        );
        TransitionImageLayout(
            m_device,
            m_graphicsQueue,
            m_commandPool,
            m_overlayImages[i].image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        const_cast<text::Atlas&>(textSystem->atlases[i]).dirty = false;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxAnisotropy = 1.0f;
    CheckVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_overlaySampler), "vkCreateSampler(overlay)");
}

void VulkanRenderer::DestroyOverlayResources()
{
    for (std::uint32_t i = 0; i < text::kMaxAtlases; ++i)
    {
        DestroyImage(m_device, m_overlayImages[i]);
    }
    m_overlayAtlasCount = 0;
    DestroyBuffer(m_device, m_overlayUploadBuffer);
}

void VulkanRenderer::UpdateOverlayGeometry(const text::System& text)
{
    std::vector<OverlayVertex> vertices;
    vertices.reserve(kOverlayMaxGlyphs * 6);
    m_overlayBatchCount = 0;

    float width = static_cast<float>(m_swapchainExtent.width > 0 ? m_swapchainExtent.width : 1);
    float height = static_cast<float>(m_swapchainExtent.height > 0 ? m_swapchainExtent.height : 1);

    auto toNdcX = [&](float px) { return -1.0f + 2.0f * px / width; };
    auto toNdcY = [&](float py) { return -1.0f + 2.0f * py / height; };

    auto appendText = [&](const text::Entry& entry) {
        const text::Atlas& atlas = text.atlases[entry.atlasIndex];
        float cursorX = entry.x;
        std::size_t count = std::min<std::size_t>(entry.length, kOverlayMaxGlyphs);
        if (m_overlayBatchCount == 0 ||
            m_overlayBatches[m_overlayBatchCount - 1].atlasIndex != entry.atlasIndex)
        {
            if (m_overlayBatchCount >= m_overlayBatches.size())
            {
                return;
            }
            m_overlayBatches[m_overlayBatchCount++] = OverlayBatch{
                .atlasIndex = entry.atlasIndex,
                .firstVertex = static_cast<std::uint32_t>(vertices.size()),
                .vertexCount = 0,
            };
        }
        OverlayBatch& batch = m_overlayBatches[m_overlayBatchCount - 1];
        for (std::size_t i = 0; i < count && vertices.size() + 6 <= kOverlayMaxGlyphs * 6; ++i)
        {
            unsigned char c = static_cast<unsigned char>(entry.text[i]);
            if (c < text::kFirstGlyph || c >= text::kFirstGlyph + text::kGlyphCount)
            {
                cursorX += atlas.lineHeight * 0.5f;
                continue;
            }

            const text::Glyph& glyph = atlas.glyphs[c - text::kFirstGlyph];
            float leftPx = cursorX + glyph.minX;
            float topPx = entry.y + (atlas.ascent - glyph.maxY);
            float rightPx = leftPx + glyph.width;
            float bottomPx = topPx + glyph.height;

            vertices.push_back({Vec2Make(toNdcX(leftPx), toNdcY(topPx)), Vec2Make(glyph.u0, glyph.v0), entry.color});
            vertices.push_back({Vec2Make(toNdcX(rightPx), toNdcY(bottomPx)), Vec2Make(glyph.u1, glyph.v1), entry.color});
            vertices.push_back({Vec2Make(toNdcX(leftPx), toNdcY(bottomPx)), Vec2Make(glyph.u0, glyph.v1), entry.color});
            vertices.push_back({Vec2Make(toNdcX(leftPx), toNdcY(topPx)), Vec2Make(glyph.u0, glyph.v0), entry.color});
            vertices.push_back({Vec2Make(toNdcX(rightPx), toNdcY(topPx)), Vec2Make(glyph.u1, glyph.v0), entry.color});
            vertices.push_back({Vec2Make(toNdcX(rightPx), toNdcY(bottomPx)), Vec2Make(glyph.u1, glyph.v1), entry.color});
            batch.vertexCount += 6;

            cursorX += glyph.advance;
        }
    };

    for (std::uint32_t i = 0; i < text.entryCount; ++i)
    {
        appendText(text.entries[i]);
    }

    m_overlayVertexCount = static_cast<std::uint32_t>(vertices.size());
    if (m_overlayVertexCount > 0)
    {
        std::memcpy(m_overlayVertexBuffer.mapped, vertices.data(), sizeof(OverlayVertex) * vertices.size());
    }
}

void VulkanRenderer::SyncOverlayAtlases(const text::System& textSystem)
{
    m_overlayAtlasCount = textSystem.atlasCount;
    for (std::uint32_t i = 0; i < textSystem.atlasCount; ++i)
    {
        const text::Atlas& atlas = textSystem.atlases[i];
        if (!atlas.valid || !atlas.dirty)
        {
            continue;
        }

        if (m_overlayImages[i].image != VK_NULL_HANDLE)
        {
            DestroyImage(m_device, m_overlayImages[i]);
        }

        std::memcpy(m_overlayUploadBuffer.mapped, atlas.texture.pixels.data(), atlas.texture.pixels.size());
        m_overlayImages[i] = CreateImage2D(
            m_physicalDevice,
            m_device,
            atlas.texture.width,
            atlas.texture.height,
            VK_FORMAT_R8G8B8A8_SRGB,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT
        );
        TransitionImageLayout(
            m_device,
            m_graphicsQueue,
            m_commandPool,
            m_overlayImages[i].image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        );
        CopyBufferToImage(
            m_device,
            m_graphicsQueue,
            m_commandPool,
            m_overlayUploadBuffer.buffer,
            m_overlayImages[i].image,
            atlas.texture.width,
            atlas.texture.height
        );
        TransitionImageLayout(
            m_device,
            m_graphicsQueue,
            m_commandPool,
            m_overlayImages[i].image,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        UpdateOverlayDescriptorSet(i);
        const_cast<text::Atlas&>(atlas).dirty = false;
    }
}

void VulkanRenderer::CreateSceneColorResources()
{
    m_sceneColorImage = CreateImage2D(
        m_physicalDevice,
        m_device,
        m_swapchainExtent.width,
        m_swapchainExtent.height,
        m_swapchainFormat,
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    CheckVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sceneSampler), "vkCreateSampler(scene)");
}

void VulkanRenderer::DestroySceneColorResources()
{
    DestroyImage(m_device, m_sceneColorImage);
}

void VulkanRenderer::CreateDepthResources()
{
    VkFormat depthFormat = FindDepthFormat();
    m_depthImage = CreateImage2D(
        m_physicalDevice,
        m_device,
        m_swapchainExtent.width,
        m_swapchainExtent.height,
        depthFormat,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );

    TransitionImageLayout(
        m_device,
        m_graphicsQueue,
        m_commandPool,
        m_depthImage.image,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    );
}

void VulkanRenderer::CreateShadowResources()
{
    VkFormat depthFormat = FindDepthFormat();
    for (std::size_t i = 0; i < m_shadowImages.size(); ++i)
    {
        m_shadowImages[i] = CreateImage2D(
            m_physicalDevice,
            m_device,
            m_shadowMapSize,
            m_shadowMapSize,
            depthFormat,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT
        );

        TransitionImageLayout(
            m_device,
            m_graphicsQueue,
            m_commandPool,
            m_shadowImages[i].image,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        );

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_shadowRenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &m_shadowImages[i].view;
        framebufferInfo.width = m_shadowMapSize;
        framebufferInfo.height = m_shadowMapSize;
        framebufferInfo.layers = 1;
        CheckVk(vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_shadowFramebuffers[i]), "vkCreateFramebuffer(shadow)");
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    CheckVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_shadowSampler), "vkCreateSampler(shadow)");
}

void VulkanRenderer::DestroyShadowResources()
{
    for (VkFramebuffer& framebuffer : m_shadowFramebuffers)
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(m_device, framebuffer, nullptr);
            framebuffer = VK_NULL_HANDLE;
        }
    }
    for (ImageResource& image : m_shadowImages)
    {
        DestroyImage(m_device, image);
    }
}

void VulkanRenderer::UpdateDescriptorSet()
{
    if (m_textureImages.empty())
    {
        return;
    }

    for (std::size_t i = 0; i < m_descriptorSets.size(); ++i)
    {
        VkDescriptorBufferInfo uniformInfo{};
        uniformInfo.buffer = m_uniformBuffer.buffer;
        uniformInfo.offset = 0;
        uniformInfo.range = sizeof(SceneUniforms);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.sampler = m_textureSampler;
        imageInfo.imageView = m_textureImages[i].view;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::array<VkDescriptorImageInfo, kTotalShadowMaps> shadowInfos{};
        for (std::size_t shadowIndex = 0; shadowIndex < shadowInfos.size(); ++shadowIndex)
        {
            shadowInfos[shadowIndex].sampler = m_shadowSampler;
            shadowInfos[shadowIndex].imageView = m_shadowImages[shadowIndex].view;
            shadowInfos[shadowIndex].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }

        VkWriteDescriptorSet uniformWrite{};
        uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        uniformWrite.dstSet = m_descriptorSets[i];
        uniformWrite.dstBinding = 0;
        uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uniformWrite.descriptorCount = 1;
        uniformWrite.pBufferInfo = &uniformInfo;

        VkWriteDescriptorSet imageWrite{};
        imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        imageWrite.dstSet = m_descriptorSets[i];
        imageWrite.dstBinding = 1;
        imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        imageWrite.descriptorCount = 1;
        imageWrite.pImageInfo = &imageInfo;

        VkWriteDescriptorSet shadowWrite{};
        shadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        shadowWrite.dstSet = m_descriptorSets[i];
        shadowWrite.dstBinding = 2;
        shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        shadowWrite.descriptorCount = static_cast<std::uint32_t>(shadowInfos.size());
        shadowWrite.pImageInfo = shadowInfos.data();

        VkDescriptorImageInfo paintInfo{};
        paintInfo.sampler = m_paintSampler;
        if (i < m_paintImages.size() && m_paintImages[i].view != VK_NULL_HANDLE)
        {
            paintInfo.imageView = m_paintImages[i].view;
        }
        else
        {
            paintInfo.imageView = m_blankPaintImage.view;
        }
        paintInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet paintWrite{};
        paintWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        paintWrite.dstSet = m_descriptorSets[i];
        paintWrite.dstBinding = 3;
        paintWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        paintWrite.descriptorCount = 1;
        paintWrite.pImageInfo = &paintInfo;

        VkDescriptorImageInfo effectInfo{};
        effectInfo.sampler = m_effectSampler;
        effectInfo.imageView = m_effectPatternImage.view;
        effectInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo normalInfo{};
        normalInfo.sampler = m_textureSampler;
        if (i < m_normalTextureImages.size())
        {
            normalInfo.imageView = m_normalTextureImages[i].view;
        }
        else
        {
            normalInfo.imageView = m_flatNormalImage.view;
        }
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet effectWrite{};
        effectWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        effectWrite.dstSet = m_descriptorSets[i];
        effectWrite.dstBinding = 4;
        effectWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        effectWrite.descriptorCount = 1;
        effectWrite.pImageInfo = &effectInfo;

        VkWriteDescriptorSet normalWrite{};
        normalWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        normalWrite.dstSet = m_descriptorSets[i];
        normalWrite.dstBinding = 5;
        normalWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        normalWrite.descriptorCount = 1;
        normalWrite.pImageInfo = &normalInfo;

        VkDescriptorBufferInfo procCityLightInfo{};
        procCityLightInfo.buffer = m_procCityDynamicLightBuffer.buffer;
        procCityLightInfo.offset = 0;
        procCityLightInfo.range = sizeof(DynamicPointLightGpu) * kMaxProcCityDynamicLights;

        VkWriteDescriptorSet procCityLightWrite{};
        procCityLightWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        procCityLightWrite.dstSet = m_descriptorSets[i];
        procCityLightWrite.dstBinding = 6;
        procCityLightWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        procCityLightWrite.descriptorCount = 1;
        procCityLightWrite.pBufferInfo = &procCityLightInfo;

        VkDescriptorBufferInfo procCityLightIndexInfo{};
        procCityLightIndexInfo.buffer = m_procCityDynamicLightIndexBuffer.buffer;
        procCityLightIndexInfo.offset = 0;
        procCityLightIndexInfo.range = sizeof(std::uint32_t) * std::max<std::size_t>(1, m_drawItems.size() * kMaxProcCityLightRefsPerInstance);

        VkWriteDescriptorSet procCityLightIndexWrite{};
        procCityLightIndexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        procCityLightIndexWrite.dstSet = m_descriptorSets[i];
        procCityLightIndexWrite.dstBinding = 7;
        procCityLightIndexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        procCityLightIndexWrite.descriptorCount = 1;
        procCityLightIndexWrite.pBufferInfo = &procCityLightIndexInfo;

        VkDescriptorBufferInfo procCityTileInfo{};
        procCityTileInfo.buffer = m_procCityLightTileBuffer.buffer;
        procCityTileInfo.offset = 0;
        procCityTileInfo.range = sizeof(ProcCityLightTileGpu) * kMaxProcCityLightTiles;

        VkWriteDescriptorSet procCityTileWrite{};
        procCityTileWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        procCityTileWrite.dstSet = m_descriptorSets[i];
        procCityTileWrite.dstBinding = 8;
        procCityTileWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        procCityTileWrite.descriptorCount = 1;
        procCityTileWrite.pBufferInfo = &procCityTileInfo;

        VkDescriptorBufferInfo procCityTileLightIndexInfo{};
        procCityTileLightIndexInfo.buffer = m_procCityTileLightIndexBuffer.buffer;
        procCityTileLightIndexInfo.offset = 0;
        procCityTileLightIndexInfo.range = sizeof(std::uint32_t) * kMaxProcCityTileLightRefs;

        VkWriteDescriptorSet procCityTileLightIndexWrite{};
        procCityTileLightIndexWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        procCityTileLightIndexWrite.dstSet = m_descriptorSets[i];
        procCityTileLightIndexWrite.dstBinding = 9;
        procCityTileLightIndexWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        procCityTileLightIndexWrite.descriptorCount = 1;
        procCityTileLightIndexWrite.pBufferInfo = &procCityTileLightIndexInfo;

        std::array<VkWriteDescriptorSet, 10> writes = {
            uniformWrite,
            imageWrite,
            shadowWrite,
            paintWrite,
            effectWrite,
            normalWrite,
            procCityLightWrite,
            procCityLightIndexWrite,
            procCityTileWrite,
            procCityTileLightIndexWrite};
        vkUpdateDescriptorSets(m_device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void VulkanRenderer::UpdatePaintDescriptorSet(std::uint32_t descriptorIndex)
{
    if (descriptorIndex >= m_descriptorSets.size())
    {
        return;
    }

    VkDescriptorImageInfo paintInfo{};
    paintInfo.sampler = m_paintSampler;
    if (descriptorIndex < m_paintImages.size() && m_paintImages[descriptorIndex].view != VK_NULL_HANDLE)
    {
        paintInfo.imageView = m_paintImages[descriptorIndex].view;
    }
    else
    {
        paintInfo.imageView = m_blankPaintImage.view;
    }
    paintInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet paintWrite{};
    paintWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    paintWrite.dstSet = m_descriptorSets[descriptorIndex];
    paintWrite.dstBinding = 3;
    paintWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    paintWrite.descriptorCount = 1;
    paintWrite.pImageInfo = &paintInfo;
    vkUpdateDescriptorSets(m_device, 1, &paintWrite, 0, nullptr);
}

void VulkanRenderer::UpdateOverlayDescriptorSet(std::uint32_t atlasIndex)
{
    if (atlasIndex >= m_overlayDescriptorSets.size() || m_overlayImages[atlasIndex].view == VK_NULL_HANDLE)
    {
        return;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = m_overlaySampler;
    imageInfo.imageView = m_overlayImages[atlasIndex].view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet imageWrite{};
    imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite.dstSet = m_overlayDescriptorSets[atlasIndex];
    imageWrite.dstBinding = 0;
    imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageWrite.descriptorCount = 1;
    imageWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device, 1, &imageWrite, 0, nullptr);
}

void VulkanRenderer::UpdatePostDescriptorSet()
{
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = m_sceneSampler;
    imageInfo.imageView = m_sceneColorImage.view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet imageWrite{};
    imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite.dstSet = m_postDescriptorSet;
    imageWrite.dstBinding = 0;
    imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageWrite.descriptorCount = 1;
    imageWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device, 1, &imageWrite, 0, nullptr);
}
