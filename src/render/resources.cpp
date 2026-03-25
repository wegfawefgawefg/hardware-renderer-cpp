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

void VulkanRenderer::CreateSceneBuffers(const SceneData& scene)
{
    std::vector<Vertex> mergedVertices;
    std::vector<std::uint32_t> mergedIndices;
    m_drawItems.clear();

    for (const EntityData& entity : scene.entities)
    {
        if (entity.modelIndex >= scene.models.size())
        {
            continue;
        }

        const ModelData& model = scene.models[entity.modelIndex];
        std::uint32_t baseVertex = static_cast<std::uint32_t>(mergedVertices.size());
        std::uint32_t baseIndex = static_cast<std::uint32_t>(mergedIndices.size());

        mergedVertices.insert(mergedVertices.end(), model.mesh.vertices.begin(), model.mesh.vertices.end());
        for (std::uint32_t index : model.mesh.indices)
        {
            mergedIndices.push_back(index + baseVertex);
        }

        for (const PrimitiveData& primitive : model.primitives)
        {
            DrawItem item{};
            item.model = entity.transform;
            item.firstIndex = baseIndex + primitive.firstIndex;
            item.indexCount = primitive.indexCount;
            item.descriptorIndex = static_cast<std::uint32_t>(m_drawItems.size());
            m_drawItems.push_back(item);
        }
    }

    VkDeviceSize vertexSize = sizeof(Vertex) * mergedVertices.size();
    VkDeviceSize indexSize = sizeof(std::uint32_t) * mergedIndices.size();
    VkDeviceSize uniformSize = sizeof(SceneUniforms);

    m_vertexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        vertexSize > 0 ? vertexSize : sizeof(Vertex),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_indexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        indexSize > 0 ? indexSize : sizeof(std::uint32_t),
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_uniformBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        uniformSize,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );

    m_overlayVertexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(OverlayVertex) * 6,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_lightMarkerBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(LightMarkerVertex) * 4,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );

    if (!mergedVertices.empty())
    {
        std::memcpy(m_vertexBuffer.mapped, mergedVertices.data(), static_cast<std::size_t>(vertexSize));
    }
    if (!mergedIndices.empty())
    {
        std::memcpy(m_indexBuffer.mapped, mergedIndices.data(), static_cast<std::size_t>(indexSize));
    }

    std::array<OverlayVertex, 6> quad = BuildOverlayQuad(
        m_swapchainExtent.width,
        m_swapchainExtent.height,
        1,
        1
    );
    std::memcpy(m_overlayVertexBuffer.mapped, quad.data(), sizeof(quad));
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

void VulkanRenderer::CreateCharacterResources(const SkinnedCharacterAsset& characterAsset)
{
    if (characterAsset.mesh.vertices.empty() || characterAsset.mesh.indices.empty())
    {
        return;
    }

    m_hasCharacter = true;
    m_characterIndexCount = static_cast<std::uint32_t>(characterAsset.mesh.indices.size());

    VkDeviceSize vertexSize = sizeof(Vertex) * characterAsset.mesh.vertices.size();
    VkDeviceSize indexSize = sizeof(std::uint32_t) * characterAsset.mesh.indices.size();
    m_characterVertexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        vertexSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_characterIndexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        indexSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    std::memcpy(m_characterVertexBuffer.mapped, characterAsset.mesh.vertices.data(), static_cast<std::size_t>(vertexSize));
    std::memcpy(m_characterIndexBuffer.mapped, characterAsset.mesh.indices.data(), static_cast<std::size_t>(indexSize));

    const TextureData& texture = characterAsset.texture;
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

    m_characterDescriptorIndex = static_cast<std::uint32_t>(m_textureImages.size());
    m_textureImages.push_back(image);
    DestroyBuffer(m_device, stagingBuffer);
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

        std::array<VkWriteDescriptorSet, 2> writes = {uniformWrite, imageWrite};
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
