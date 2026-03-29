#include "render/internal.h"

#include <array>
#include <cstring>
#include <filesystem>
#include <vector>

#include "assets/texture_loader.h"

using namespace vulkan_renderer_internal;

namespace
{
TextureData LoadFlatDecalTexture(std::string_view relativePath, bool normalMap)
{
    if (relativePath.empty())
    {
        return normalMap ? MakeSolidTexture(128, 128, 255) : MakeSolidTexture(255, 255, 255);
    }

    std::filesystem::path path = std::filesystem::path(HARDWARE_RENDERER_ASSETS_ROOT) / std::filesystem::path(relativePath);
    if (!std::filesystem::exists(path))
    {
        return normalMap ? MakeSolidTexture(128, 128, 255) : MakeSolidTexture(255, 255, 255);
    }
    return LoadTexture(path.string());
}

ImageResource UploadFlatDecalTexture(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkQueue graphicsQueue,
    VkCommandPool commandPool,
    const TextureData& texture,
    VkFormat format)
{
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(texture.pixels.size());
    BufferResource stagingBuffer = CreateBuffer(
        physicalDevice,
        device,
        imageSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true);
    std::memcpy(stagingBuffer.mapped, texture.pixels.data(), static_cast<std::size_t>(imageSize));

    ImageResource image = CreateImage2D(
        physicalDevice,
        device,
        texture.width,
        texture.height,
        format,
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT);

    TransitionImageLayout(
        device,
        graphicsQueue,
        commandPool,
        image.image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(device, graphicsQueue, commandPool, stagingBuffer.buffer, image.image, texture.width, texture.height);
    TransitionImageLayout(
        device,
        graphicsQueue,
        commandPool,
        image.image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    DestroyBuffer(device, stagingBuffer);
    return image;
}

Vec3 TransformPoint(Mat4 m, Vec3 p)
{
    Vec4 out = Mat4MulVec4(m, Vec4Make(p.x, p.y, p.z, 1.0f));
    return Vec3Make(out.x, out.y, out.z);
}
}

void VulkanRenderer::CreateFlatDecalResources(const decals::FlatDecalSystem& flatDecals)
{
    m_flatDecalTemplates.clear();
    for (std::uint32_t i = 0; i < flatDecals.templateCount; ++i)
    {
        const decals::FlatDecalTemplate& decalTemplate = flatDecals.templates[i];
        std::uint32_t descriptorIndex = static_cast<std::uint32_t>(m_textureImages.size());
        m_textureImages.push_back(UploadFlatDecalTexture(
            m_physicalDevice,
            m_device,
            m_graphicsQueue,
            m_commandPool,
            LoadFlatDecalTexture(decalTemplate.albedoAssetPath, false),
            VK_FORMAT_R8G8B8A8_SRGB));
        m_normalTextureImages.push_back(UploadFlatDecalTexture(
            m_physicalDevice,
            m_device,
            m_graphicsQueue,
            m_commandPool,
            LoadFlatDecalTexture(decalTemplate.normalAssetPath, true),
            VK_FORMAT_R8G8B8A8_UNORM));
        m_flatDecalTemplates.push_back(FlatDecalTemplateGpu{
            .descriptorIndex = descriptorIndex,
            .flipNormalY = decalTemplate.flipNormalY,
        });
    }
}

void VulkanRenderer::UpdateFlatDecalGeometry(const decals::FlatDecalSystem* flatDecals)
{
    m_flatDecalDraws.clear();
    m_flatDecalVertexCount = 0;
    m_flatDecalIndexCount = 0;
    if (flatDecals == nullptr || flatDecals->templateCount == 0)
    {
        return;
    }

    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    vertices.reserve(decals::FlatDecalSystem::kMaxInstances * 4);
    indices.reserve(decals::FlatDecalSystem::kMaxInstances * 6);
    const std::array<Vec3, 4> localPositions = {
        Vec3Make(-0.5f, -0.5f, 0.0f),
        Vec3Make(0.5f, -0.5f, 0.0f),
        Vec3Make(0.5f, 0.5f, 0.0f),
        Vec3Make(-0.5f, 0.5f, 0.0f),
    };
    const std::array<Vec2, 4> uvs = {
        Vec2Make(0.0f, 1.0f),
        Vec2Make(1.0f, 1.0f),
        Vec2Make(1.0f, 0.0f),
        Vec2Make(0.0f, 0.0f),
    };

    std::uint32_t vertexCursor = 0;
    std::uint32_t indexCursor = 0;
    for (std::uint32_t templateIndex = 0; templateIndex < flatDecals->templateCount; ++templateIndex)
    {
        std::uint32_t firstIndex = indexCursor;
        for (const decals::FlatDecalInstance& instance : flatDecals->instances)
        {
            if (!instance.active || instance.templateId != templateIndex)
            {
                continue;
            }

            Vec3 worldNormal = Vec3Normalize(Vec3Make(
                instance.transform.m[8],
                instance.transform.m[9],
                instance.transform.m[10]));
            std::uint32_t baseVertex = vertexCursor;
            for (std::uint32_t v = 0; v < 4; ++v)
            {
                vertices.push_back(Vertex{
                    .position = TransformPoint(instance.transform, localPositions[v]),
                    .normal = worldNormal,
                    .uv = uvs[v],
                });
                ++vertexCursor;
            }

            indices.push_back(baseVertex + 0);
            indices.push_back(baseVertex + 1);
            indices.push_back(baseVertex + 2);
            indices.push_back(baseVertex + 0);
            indices.push_back(baseVertex + 2);
            indices.push_back(baseVertex + 3);
            indexCursor += 6;
        }

        if (indexCursor > firstIndex)
        {
            m_flatDecalDraws.push_back(FlatDecalDraw{
                .firstIndex = firstIndex,
                .indexCount = indexCursor - firstIndex,
                .descriptorIndex = m_flatDecalTemplates[templateIndex].descriptorIndex,
                .flipNormalY = m_flatDecalTemplates[templateIndex].flipNormalY,
            });
        }
    }

    m_flatDecalVertexCount = vertexCursor;
    m_flatDecalIndexCount = indexCursor;
    if (vertexCursor > 0)
    {
        std::memcpy(m_flatDecalVertexBuffer.mapped, vertices.data(), sizeof(Vertex) * vertexCursor);
    }
    if (indexCursor > 0)
    {
        std::memcpy(m_flatDecalIndexBuffer.mapped, indices.data(), sizeof(std::uint32_t) * indexCursor);
    }
}
