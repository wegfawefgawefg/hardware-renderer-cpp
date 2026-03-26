#include "render/internal.h"

#include <array>
#include <cstring>

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
}

void VulkanRenderer::CreateTextureResources(const SceneData& scene)
{
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
                VK_FORMAT_R8G8B8A8_SRGB,
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

            m_textureImages.push_back(image);
            DestroyBuffer(m_device, stagingBuffer);
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
}

void VulkanRenderer::CreateOverlayResources()
{
    m_overlayUploadBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        kOverlayTextureBytes,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    std::memset(m_overlayUploadBuffer.mapped, 0, static_cast<std::size_t>(kOverlayTextureBytes));

    m_overlayImage = CreateImage2D(
        m_physicalDevice,
        m_device,
        kOverlayTextureWidth,
        kOverlayTextureHeight,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    TransitionImageLayout(
        m_device,
        m_graphicsQueue,
        m_commandPool,
        m_overlayImage.image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    CopyBufferToImage(
        m_device,
        m_graphicsQueue,
        m_commandPool,
        m_overlayUploadBuffer.buffer,
        m_overlayImage.image,
        kOverlayTextureWidth,
        kOverlayTextureHeight
    );
    TransitionImageLayout(
        m_device,
        m_graphicsQueue,
        m_commandPool,
        m_overlayImage.image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
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
    CheckVk(vkCreateSampler(m_device, &samplerInfo, nullptr, &m_overlaySampler), "vkCreateSampler(overlay)");
}

void VulkanRenderer::DestroyOverlayResources()
{
    DestroyImage(m_device, m_overlayImage);
    DestroyBuffer(m_device, m_overlayUploadBuffer);
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

        VkDescriptorBufferInfo paintInfo{};
        paintInfo.buffer = m_persistentPaintBuffer.buffer;
        paintInfo.offset = 0;
        paintInfo.range = sizeof(PersistentPaintGpuStamp) * kMaxPersistentPaintStamps;

        VkWriteDescriptorSet paintWrite{};
        paintWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        paintWrite.dstSet = m_descriptorSets[i];
        paintWrite.dstBinding = 3;
        paintWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        paintWrite.descriptorCount = 1;
        paintWrite.pBufferInfo = &paintInfo;

        std::array<VkWriteDescriptorSet, 4> writes = {uniformWrite, imageWrite, shadowWrite, paintWrite};
        vkUpdateDescriptorSets(m_device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void VulkanRenderer::UpdateOverlayDescriptorSet()
{
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = m_overlaySampler;
    imageInfo.imageView = m_overlayImage.view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet imageWrite{};
    imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite.dstSet = m_overlayDescriptorSet;
    imageWrite.dstBinding = 0;
    imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageWrite.descriptorCount = 1;
    imageWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(m_device, 1, &imageWrite, 0, nullptr);
}
