#include "render/internal.h"

#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

using namespace vulkan_renderer_internal;

void VulkanRenderer::RecordCommandBuffer(std::uint32_t imageIndex)
{
    VkCommandBuffer commandBuffer = m_commandBuffers[imageIndex];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CheckVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");
    vkCmdResetQueryPool(commandBuffer, m_timestampQueryPool, 0, kGpuTimestampCount);
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, m_timestampQueryPool, 0);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{m_clearColor.x, m_clearColor.y, m_clearColor.z, m_clearColor.w}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_framebuffers[imageIndex];
    renderPassInfo.renderArea.extent = m_swapchainExtent;
    renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    if (m_overlayWidth > 0 && m_overlayHeight > 0)
    {
        std::array<OverlayVertex, 6> quad = BuildOverlayQuad(
            m_swapchainExtent.width,
            m_swapchainExtent.height,
            m_overlayWidth,
            m_overlayHeight
        );
        std::memcpy(m_overlayVertexBuffer.mapped, quad.data(), sizeof(quad));

        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = m_overlayImage.image;
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toTransfer
        );

        VkBufferImageCopy copyRegion{};
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent.width = kOverlayTextureWidth;
        copyRegion.imageExtent.height = kOverlayTextureHeight;
        copyRegion.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(
            commandBuffer,
            m_overlayUploadBuffer.buffer,
            m_overlayImage.image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &copyRegion
        );

        VkImageMemoryBarrier toSample{};
        toSample.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toSample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toSample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        toSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toSample.image = m_overlayImage.image;
        toSample.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toSample.subresourceRange.levelCount = 1;
        toSample.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &toSample
        );
    }

    std::uint32_t sunShadowCount = std::min<std::uint32_t>(m_activeShadowMapCount, kSunShadowCascadeCount);
    for (std::uint32_t shadowIndex = 0; shadowIndex < sunShadowCount; ++shadowIndex)
    {
        RecordShadowPass(commandBuffer, shadowIndex);
    }
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_timestampQueryPool, 1);
    for (std::uint32_t shadowIndex = sunShadowCount; shadowIndex < m_activeShadowMapCount; ++shadowIndex)
    {
        RecordShadowPass(commandBuffer, shadowIndex);
    }
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_timestampQueryPool, 2);

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    VkViewport viewport{};
    viewport.width = static_cast<float>(m_swapchainExtent.width);
    viewport.height = static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = m_swapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkBuffer vertexBuffers[] = {m_vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    for (std::uint32_t visibleIndex : m_visibleDrawItems)
    {
        const DrawItem& drawItem = m_drawItems[visibleIndex];
        DrawPushConstants pushConstants{};
        pushConstants.model = drawItem.model;
        pushConstants.skinned = drawItem.skinned;
        pushConstants.pointLightMask = drawItem.pointLightMask;
        pushConstants.spotLightMask = drawItem.spotLightMask;
        pushConstants.shadowedSpotLightMask = drawItem.shadowedSpotLightMask;
        pushConstants.persistentPaintOffset = drawItem.persistentPaintOffset;
        pushConstants.persistentPaintCount = drawItem.persistentPaintCount;
        vkCmdPushConstants(
            commandBuffer,
            m_pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(pushConstants),
            &pushConstants
        );
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipelineLayout,
            0,
            1,
            &m_descriptorSets[drawItem.descriptorIndex],
            0,
            nullptr
        );
        vkCmdDrawIndexed(commandBuffer, drawItem.indexCount, 1, drawItem.firstIndex, 0, 0);
    }

    if (m_hasCharacter && m_characterState.visible && m_characterIndexCount > 0)
    {
        DrawPushConstants pushConstants{};
        pushConstants.model = m_characterState.model;
        pushConstants.skinned = 1;
        pushConstants.pointLightMask = 0xFu;
        pushConstants.spotLightMask = 0xFFFFFFFFu;
        pushConstants.shadowedSpotLightMask = (1u << kMaxShadowedSpotLights) - 1u;
        pushConstants.persistentPaintOffset = 0;
        pushConstants.persistentPaintCount = 0;
        VkBuffer characterBuffers[] = {m_characterVertexBuffer.buffer};
        VkDeviceSize characterOffsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, characterBuffers, characterOffsets);
        vkCmdBindIndexBuffer(commandBuffer, m_characterIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdPushConstants(
            commandBuffer,
            m_pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(pushConstants),
            &pushConstants
        );
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipelineLayout,
            0,
            1,
            &m_descriptorSets[m_characterDescriptorIndex],
            0,
            nullptr
        );
        vkCmdDrawIndexed(commandBuffer, m_characterIndexCount, 1, 0, 0, 0);

        VkBuffer vertexBuffers[] = {m_vertexBuffer.buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    }
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_timestampQueryPool, 3);

    VkBuffer lightBuffers[] = {m_lightMarkerBuffer.buffer};
    VkDeviceSize lightOffsets[] = {0};
    if (m_lightSolidVertexCount > 0)
    {
        VkBuffer solidBuffers[] = {m_lightSolidBuffer.buffer};
        VkDeviceSize solidOffsets[] = {0};
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightSolidPipeline);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, solidBuffers, solidOffsets);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipelineLayout,
            0,
            1,
            &m_descriptorSets[0],
            0,
            nullptr
        );
        vkCmdDraw(commandBuffer, m_lightSolidVertexCount, 1, 0, 0);
    }
    if (m_lightLineVertexCount > 0)
    {
        VkBuffer lineBuffers[] = {m_lightLineBuffer.buffer};
        VkDeviceSize lineOffsets[] = {0};
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightLinePipeline);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, lineBuffers, lineOffsets);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_pipelineLayout,
            0,
            1,
            &m_descriptorSets[0],
            0,
            nullptr
        );
        vkCmdDraw(commandBuffer, m_lightLineVertexCount, 1, 0, 0);
    }

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightPipeline);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, lightBuffers, lightOffsets);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineLayout,
        0,
        1,
        &m_descriptorSets[0],
        0,
        nullptr
    );
    vkCmdDraw(commandBuffer, kLightMarkerCount, 1, 0, 0);
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_timestampQueryPool, 4);

    if (m_overlayWidth > 0 && m_overlayHeight > 0)
    {
        VkBuffer overlayBuffers[] = {m_overlayVertexBuffer.buffer};
        VkDeviceSize overlayOffsets[] = {0};
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_overlayPipeline);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, overlayBuffers, overlayOffsets);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_overlayPipelineLayout,
            0,
            1,
            &m_overlayDescriptorSet,
            0,
            nullptr
        );
        vkCmdDraw(commandBuffer, 6, 1, 0, 0);
    }

    if (m_imguiInitialized)
    {
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
    }
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_timestampQueryPool, 5);

    vkCmdEndRenderPass(commandBuffer);
    CheckVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer");
}

VkSurfaceFormatKHR VulkanRenderer::ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const
{
    for (const VkSurfaceFormatKHR& format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }
    for (const VkSurfaceFormatKHR& format : formats)
    {
        if (format.format == VK_FORMAT_R8G8B8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }
    return formats.front();
}

VkPresentModeKHR VulkanRenderer::ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) const
{
    for (VkPresentModeKHR mode : modes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

std::uint32_t VulkanRenderer::ChooseQueueFamily() const
{
    std::uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &familyCount, families.data());

    for (std::uint32_t i = 0; i < familyCount; ++i)
    {
        VkBool32 presentSupported = VK_FALSE;
        CheckVk(
            vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, i, m_surface, &presentSupported),
            "vkGetPhysicalDeviceSurfaceSupportKHR"
        );
        if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && presentSupported)
        {
            return i;
        }
    }

    throw std::runtime_error("Failed to find graphics+present queue family");
}

VkFormat VulkanRenderer::FindDepthFormat() const
{
    std::array<VkFormat, 3> candidates =
        {
            VK_FORMAT_D32_SFLOAT,
            VK_FORMAT_D32_SFLOAT_S8_UINT,
            VK_FORMAT_D24_UNORM_S8_UINT,
        };

    for (VkFormat format : candidates)
    {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, format, &properties);
        if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        {
            return format;
        }
    }

    throw std::runtime_error("Failed to find a supported depth format");
}
