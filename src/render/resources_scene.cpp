#include "render/internal.h"
#include "render/render_batches.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>

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
    m_staticBatches.clear();
    m_visibleStaticBatchDrawItems.clear();
    m_visibleStaticInstances.clear();
    m_staticBatchFirstInstance.clear();
    m_shadowVisibleStaticBatchDrawItems.clear();
    m_shadowVisibleStaticInstances.clear();
    m_shadowStaticBatchFirstInstance.clear();
    std::unordered_map<std::uint64_t, std::uint32_t> staticBatchMap;

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
            item.modelIndex = entity.modelIndex;
            item.entityIndex = static_cast<std::uint32_t>(&entity - scene.entities.data());
            item.primitiveIndex = static_cast<std::uint32_t>(&primitive - model.primitives.data());
            if (primitive.materialIndex < model.materials.size())
            {
                item.castsShadows = model.materials[primitive.materialIndex].castsShadows;
                item.flipNormalY = model.materials[primitive.materialIndex].flipNormalY;
                if (model.materials[primitive.materialIndex].generatedQuadMaterialUv)
                {
                    item.materialFlags |= 4u;
                }
                if (model.materials[primitive.materialIndex].leanShading)
                {
                    item.materialFlags |= 8u;
                }
            }
            m_drawItems.push_back(item);
            DrawItem& storedItem = m_drawItems.back();
            std::uint64_t batchKey = MakeStaticBatchKey(entity.modelIndex, storedItem.primitiveIndex);
            auto foundBatch = staticBatchMap.find(batchKey);
            if (foundBatch == staticBatchMap.end())
            {
                std::uint32_t batchIndex = static_cast<std::uint32_t>(m_staticBatches.size());
                staticBatchMap.emplace(batchKey, batchIndex);
                m_staticBatches.push_back(StaticBatch{
                    .firstIndex = storedItem.firstIndex,
                    .indexCount = storedItem.indexCount,
                    .descriptorIndex = storedItem.descriptorIndex,
                    .modelIndex = storedItem.modelIndex,
                    .flipNormalY = storedItem.flipNormalY,
                });
                m_visibleStaticBatchDrawItems.emplace_back();
                m_shadowVisibleStaticBatchDrawItems.emplace_back();
                storedItem.staticBatchIndex = batchIndex;
            }
            else
            {
                storedItem.staticBatchIndex = foundBatch->second;
            }
            storedItem.batchedStatic = true;
        }
    }
    m_visibleDrawItems.reserve(m_drawItems.size());
    m_paintLayers.assign(m_drawItems.size(), {});
    m_sceneVertexCount = static_cast<std::uint32_t>(mergedVertices.size());
    m_sceneIndexCount = static_cast<std::uint32_t>(mergedIndices.size());

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
    m_staticInstanceBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(StaticInstanceGpu) * std::max<std::size_t>(1, m_drawItems.size()),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_shadowStaticInstanceBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(StaticInstanceGpu) * std::max<std::size_t>(1, m_drawItems.size()),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_nullInstanceBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(StaticInstanceGpu),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    std::memset(m_nullInstanceBuffer.mapped, 0, sizeof(StaticInstanceGpu));
    m_procCityDynamicLightBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(DynamicPointLightGpu) * kMaxProcCityDynamicLights,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_procCityDynamicLightIndexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(std::uint32_t) * std::max<std::size_t>(1, m_drawItems.size() * kMaxProcCityLightRefsPerInstance),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_procCityLightTileBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(ProcCityLightTileGpu) * kMaxProcCityLightTiles,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_procCityTileLightIndexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(std::uint32_t) * kMaxProcCityTileLightRefs,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
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
        sizeof(OverlayVertex) * vulkan_renderer_internal::kOverlayMaxGlyphs * 6,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_postVertexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(OverlayVertex) * 6,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_paintUploadBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        kPaintTextureSize * kPaintTextureSize * 4,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
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
    m_flatDecalVertexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(Vertex) * decals::FlatDecalSystem::kMaxInstances * 4,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_flatDecalIndexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        sizeof(std::uint32_t) * decals::FlatDecalSystem::kMaxInstances * 6,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
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

    std::array<OverlayVertex, 6> quad = {};
    std::memcpy(m_overlayVertexBuffer.mapped, quad.data(), sizeof(quad));
    std::memcpy(m_postVertexBuffer.mapped, quad.data(), sizeof(quad));
}

bool VulkanRenderer::UpdateSceneGeometry(const SceneData& scene)
{
    std::vector<Vertex> mergedVertices;
    std::vector<std::uint32_t> mergedIndices;
    std::vector<BoundsSphere> modelBounds(scene.models.size());
    for (std::size_t modelIndex = 0; modelIndex < scene.models.size(); ++modelIndex)
    {
        modelBounds[modelIndex] = ComputeMeshBounds(scene.models[modelIndex].mesh);
    }

    std::vector<DrawItem> newDrawItems;
    newDrawItems.reserve(m_drawItems.size());
    std::vector<StaticBatch> newStaticBatches;
    std::vector<std::vector<std::uint32_t>> newVisibleStaticBatchDrawItems;
    std::unordered_map<std::uint64_t, std::uint32_t> staticBatchMap;
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
            item.descriptorIndex = static_cast<std::uint32_t>(newDrawItems.size());
            item.modelIndex = entity.modelIndex;
            item.entityIndex = static_cast<std::uint32_t>(&entity - scene.entities.data());
            item.primitiveIndex = static_cast<std::uint32_t>(&primitive - model.primitives.data());
            if (primitive.materialIndex < model.materials.size())
            {
                item.castsShadows = model.materials[primitive.materialIndex].castsShadows;
                item.flipNormalY = model.materials[primitive.materialIndex].flipNormalY;
                if (model.materials[primitive.materialIndex].generatedQuadMaterialUv)
                {
                    item.materialFlags |= 4u;
                }
                if (model.materials[primitive.materialIndex].leanShading)
                {
                    item.materialFlags |= 8u;
                }
            }
            newDrawItems.push_back(item);
            DrawItem& storedItem = newDrawItems.back();
            std::uint64_t batchKey = MakeStaticBatchKey(entity.modelIndex, storedItem.primitiveIndex);
            auto foundBatch = staticBatchMap.find(batchKey);
            if (foundBatch == staticBatchMap.end())
            {
                std::uint32_t batchIndex = static_cast<std::uint32_t>(newStaticBatches.size());
                staticBatchMap.emplace(batchKey, batchIndex);
                newStaticBatches.push_back(StaticBatch{
                    .firstIndex = storedItem.firstIndex,
                    .indexCount = storedItem.indexCount,
                    .descriptorIndex = storedItem.descriptorIndex,
                    .modelIndex = storedItem.modelIndex,
                    .flipNormalY = storedItem.flipNormalY,
                });
                newVisibleStaticBatchDrawItems.emplace_back();
                storedItem.staticBatchIndex = batchIndex;
            }
            else
            {
                storedItem.staticBatchIndex = foundBatch->second;
            }
            storedItem.batchedStatic = true;
        }
    }

    if (mergedVertices.size() != m_sceneVertexCount ||
        mergedIndices.size() != m_sceneIndexCount ||
        newDrawItems.size() != m_drawItems.size())
    {
        return false;
    }

    if (!mergedVertices.empty())
    {
        std::memcpy(m_vertexBuffer.mapped, mergedVertices.data(), sizeof(Vertex) * mergedVertices.size());
    }
    if (!mergedIndices.empty())
    {
        std::memcpy(m_indexBuffer.mapped, mergedIndices.data(), sizeof(std::uint32_t) * mergedIndices.size());
    }

    m_drawItems = std::move(newDrawItems);
    m_staticBatches = std::move(newStaticBatches);
    m_visibleStaticBatchDrawItems = std::move(newVisibleStaticBatchDrawItems);
    m_visibleStaticInstances.clear();
    m_staticBatchFirstInstance.clear();
    m_shadowVisibleStaticBatchDrawItems = std::vector<std::vector<std::uint32_t>>(m_staticBatches.size());
    m_shadowVisibleStaticInstances.clear();
    m_shadowStaticBatchFirstInstance.clear();
    return true;
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
