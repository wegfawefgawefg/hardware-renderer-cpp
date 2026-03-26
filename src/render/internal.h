#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <string_view>
#include <vector>

#include "render/renderer.h"

namespace vulkan_renderer_internal
{
constexpr std::string_view kVertShaderPath = HARDWARE_RENDERER_VERT_SHADER_PATH;
constexpr std::string_view kFragShaderPath = HARDWARE_RENDERER_FRAG_SHADER_PATH;
constexpr std::string_view kOverlayVertShaderPath = HARDWARE_RENDERER_OVERLAY_VERT_SHADER_PATH;
constexpr std::string_view kOverlayFragShaderPath = HARDWARE_RENDERER_OVERLAY_FRAG_SHADER_PATH;
constexpr std::string_view kLightVertShaderPath = HARDWARE_RENDERER_LIGHT_VERT_SHADER_PATH;
constexpr std::string_view kLightFragShaderPath = HARDWARE_RENDERER_LIGHT_FRAG_SHADER_PATH;
constexpr std::string_view kShadowVertShaderPath = HARDWARE_RENDERER_SHADOW_VERT_SHADER_PATH;
constexpr std::uint32_t kLightMarkerCount = 6 + kMaxSceneSpotLights + kMaxShadowedSpotLights;
constexpr std::uint32_t kOverlayTextureWidth = 512;
constexpr std::uint32_t kOverlayTextureHeight = 128;
constexpr VkDeviceSize kOverlayTextureBytes =
    static_cast<VkDeviceSize>(kOverlayTextureWidth) * kOverlayTextureHeight * sizeof(std::uint32_t);

inline VkShaderModule CreateShaderModule(VkDevice device, std::string_view path)
{
    std::vector<std::byte> bytes = ReadBinaryFile(path);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = bytes.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(bytes.data());

    VkShaderModule module = VK_NULL_HANDLE;
    CheckVk(vkCreateShaderModule(device, &createInfo, nullptr, &module), "vkCreateShaderModule");
    return module;
}

inline void TransitionImageLayout(
    VkDevice device,
    VkQueue queue,
    VkCommandPool commandPool,
    VkImage image,
    VkImageAspectFlags aspectMask,
    VkImageLayout oldLayout,
    VkImageLayout newLayout
)
{
    VkCommandBuffer commandBuffer = BeginSingleUseCommandBuffer(device, commandPool);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else
    {
        throw std::runtime_error("Unsupported image layout transition");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStage,
        dstStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );

    EndSingleUseCommandBuffer(device, queue, commandPool, commandBuffer);
}

inline void CopyBufferToImage(
    VkDevice device,
    VkQueue queue,
    VkCommandPool commandPool,
    VkBuffer buffer,
    VkImage image,
    std::uint32_t width,
    std::uint32_t height
)
{
    VkCommandBuffer commandBuffer = BeginSingleUseCommandBuffer(device, commandPool);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent.width = width;
    region.imageExtent.height = height;
    region.imageExtent.depth = 1;

    vkCmdCopyBufferToImage(
        commandBuffer,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    EndSingleUseCommandBuffer(device, queue, commandPool, commandBuffer);
}

inline std::array<OverlayVertex, 6> BuildOverlayQuad(
    std::uint32_t targetWidth,
    std::uint32_t targetHeight,
    std::uint32_t overlayWidth,
    std::uint32_t overlayHeight
)
{
    constexpr float padX = 16.0f;
    constexpr float padY = 16.0f;

    float width = static_cast<float>(targetWidth > 0 ? targetWidth : 1);
    float height = static_cast<float>(targetHeight > 0 ? targetHeight : 1);
    float left = -1.0f + 2.0f * padX / width;
    float right = -1.0f + 2.0f * (padX + static_cast<float>(overlayWidth)) / width;
    float top = 1.0f - 2.0f * padY / height;
    float bottom = 1.0f - 2.0f * (padY + static_cast<float>(overlayHeight)) / height;
    return {
        OverlayVertex{Vec2Make(left, top), Vec2Make(0.0f, 0.0f)},
        OverlayVertex{Vec2Make(right, bottom), Vec2Make(
                                                   static_cast<float>(overlayWidth) / kOverlayTextureWidth,
                                                   static_cast<float>(overlayHeight) / kOverlayTextureHeight
                                               )},
        OverlayVertex{Vec2Make(left, bottom), Vec2Make(
                                                  0.0f,
                                                  static_cast<float>(overlayHeight) / kOverlayTextureHeight
                                              )},
        OverlayVertex{Vec2Make(left, top), Vec2Make(0.0f, 0.0f)},
        OverlayVertex{Vec2Make(right, top), Vec2Make(
                                                static_cast<float>(overlayWidth) / kOverlayTextureWidth,
                                                0.0f
                                            )},
        OverlayVertex{Vec2Make(right, bottom), Vec2Make(
                                                   static_cast<float>(overlayWidth) / kOverlayTextureWidth,
                                                   static_cast<float>(overlayHeight) / kOverlayTextureHeight
                                               )},
    };
}
}
