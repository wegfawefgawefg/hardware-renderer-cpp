#include "render/internal.h"

#include <algorithm>
#include <array>
#include <cstring>

using namespace vulkan_renderer_internal;

namespace
{
struct BoundsSphere
{
    Vec3 center = {};
    float radius = 0.0f;
};

BoundsSphere ComputeMeshBounds(const MeshData& mesh)
{
    if (mesh.vertices.empty())
    {
        return {};
    }

    Vec3 min = mesh.vertices.front().position;
    Vec3 max = mesh.vertices.front().position;
    for (const Vertex& vertex : mesh.vertices)
    {
        min.x = std::min(min.x, vertex.position.x);
        min.y = std::min(min.y, vertex.position.y);
        min.z = std::min(min.z, vertex.position.z);
        max.x = std::max(max.x, vertex.position.x);
        max.y = std::max(max.y, vertex.position.y);
        max.z = std::max(max.z, vertex.position.z);
    }

    Vec3 center = Vec3Scale(Vec3Add(min, max), 0.5f);
    float radius = 0.0f;
    for (const Vertex& vertex : mesh.vertices)
    {
        radius = std::max(radius, Vec3Length(Vec3Sub(vertex.position, center)));
    }
    return BoundsSphere{center, radius};
}
}

void VulkanRenderer::CreateSceneBuffers(const SceneData& scene)
{
    std::vector<Vertex> mergedVertices;
    std::vector<std::uint32_t> mergedIndices;
    std::vector<BoundsSphere> modelBounds(scene.models.size());
    for (std::size_t modelIndex = 0; modelIndex < scene.models.size(); ++modelIndex)
    {
        modelBounds[modelIndex] = ComputeMeshBounds(scene.models[modelIndex].mesh);
    }
    m_drawItems.clear();
    m_visibleDrawItems.clear();

    for (const EntityData& entity : scene.entities)
    {
        if (entity.modelIndex >= scene.models.size())
        {
            continue;
        }

        const ModelData& model = scene.models[entity.modelIndex];
        const BoundsSphere& bounds = modelBounds[entity.modelIndex];
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
            item.localBoundsCenter = bounds.center;
            item.localBoundsRadius = bounds.radius;
            item.firstIndex = baseIndex + primitive.firstIndex;
            item.indexCount = primitive.indexCount;
            item.descriptorIndex = static_cast<std::uint32_t>(m_drawItems.size());
            item.entityIndex = static_cast<std::uint32_t>(&entity - scene.entities.data());
            m_drawItems.push_back(item);
        }
    }
    m_visibleDrawItems.reserve(m_drawItems.size());

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
        sizeof(LightMarkerVertex) * kLightMarkerCount,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_lightLineBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(LightMarkerVertex) * kLightLineVertexCount,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_lightSolidBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(LightMarkerVertex) * kLightSolidVertexCount,
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

void VulkanRenderer::UpdateSceneTransforms(const SceneData& scene)
{
    for (DrawItem& drawItem : m_drawItems)
    {
        if (drawItem.entityIndex < scene.entities.size())
        {
            drawItem.model = scene.entities[drawItem.entityIndex].transform;
        }
    }
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
