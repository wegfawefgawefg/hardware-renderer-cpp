#include "render/internal.h"

#include <array>
#include <cstring>
#include <stdexcept>
#include <vector>

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

using namespace vulkan_renderer_internal;

namespace
{
bool ModelHidden(const VulkanRenderer& renderer, std::uint32_t modelIndex)
{
    return modelIndex == renderer.m_hiddenModelIndex || modelIndex == renderer.m_hiddenModelIndexSecondary;
}
}

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
    renderPassInfo.renderPass = m_sceneRenderPass;
    renderPassInfo.framebuffer = m_sceneFramebuffer;
    renderPassInfo.renderArea.extent = m_swapchainExtent;
    renderPassInfo.clearValueCount = static_cast<std::uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    if (m_sunShadowsEnabled)
    {
        for (std::uint32_t shadowIndex = 0; shadowIndex < kSunShadowCascadeCount; ++shadowIndex)
        {
            RecordShadowPass(commandBuffer, shadowIndex);
        }
    }
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_timestampQueryPool, 1);
    if (m_localLightShadowsEnabled)
    {
        for (std::uint32_t shadowIndex = 0; shadowIndex < m_activeShadowedSpotCount; ++shadowIndex)
        {
            RecordShadowPass(commandBuffer, kSunShadowCascadeCount + shadowIndex);
        }
    }
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_timestampQueryPool, 2);

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkPipeline batchedPipeline = m_useProcCityPipeline ? m_procCityPipeline : m_pipeline;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, batchedPipeline);

    VkViewport viewport{};
    viewport.width = static_cast<float>(m_swapchainExtent.width);
    viewport.height = static_cast<float>(m_swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = m_swapchainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    VkBuffer instancedBuffers[] = {m_vertexBuffer.buffer, m_staticInstanceBuffer.buffer};
    VkDeviceSize instanceOffsets[] = {0, 0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, instancedBuffers, instanceOffsets);
    if (!m_hideSceneMesh)
    {
        for (std::size_t batchIndex = 0; batchIndex < m_staticBatches.size(); ++batchIndex)
        {
            const std::vector<std::uint32_t>& visibleItems = m_visibleStaticBatchDrawItems[batchIndex];
            if (visibleItems.empty())
            {
                continue;
            }
            const StaticBatch& batch = m_staticBatches[batchIndex];
            if (ModelHidden(*this, batch.modelIndex))
            {
                continue;
            }
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_pipelineLayout,
                0,
                1,
                &m_descriptorSets[batch.descriptorIndex],
                0,
                nullptr
            );
            vkCmdDrawIndexed(
                commandBuffer,
                batch.indexCount,
                static_cast<std::uint32_t>(visibleItems.size()),
                batch.firstIndex,
                0,
                m_staticBatchFirstInstance[batchIndex]);
        }
    }

    VkBuffer singleDrawBuffers[] = {m_vertexBuffer.buffer, m_nullInstanceBuffer.buffer};
    VkDeviceSize singleDrawOffsets[] = {0, 0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, singleDrawBuffers, singleDrawOffsets);
    if (batchedPipeline != m_pipeline)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
    }
    if (!m_hideSceneMesh)
    {
        for (std::uint32_t visibleIndex : m_visibleDrawItems)
        {
            const DrawItem& drawItem = m_drawItems[visibleIndex];
            if (ModelHidden(*this, drawItem.modelIndex))
            {
                continue;
            }
            DrawPushConstants pushConstants{};
            pushConstants.model = drawItem.model;
            pushConstants.skinned = drawItem.skinned;
            pushConstants.pointLightMask = drawItem.pointLightMask;
            pushConstants.spotLightMask = drawItem.spotLightMask;
            pushConstants.shadowedSpotLightMask = drawItem.shadowedSpotLightMask;
            pushConstants.materialFlags = drawItem.materialFlags;
            if (drawItem.flipNormalY)
            {
                pushConstants.materialFlags |= 1u;
            }
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
    }

    if (m_hasCharacter && m_characterState.visible && m_characterIndexCount > 0)
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        DrawPushConstants pushConstants{};
        pushConstants.model = m_characterState.model;
        pushConstants.skinned = 1;
        pushConstants.pointLightMask = 0xFu;
        pushConstants.spotLightMask = 0xFFFFFFFFu;
        pushConstants.shadowedSpotLightMask = (1u << kMaxShadowedSpotLights) - 1u;
        VkBuffer characterBuffers[] = {m_characterVertexBuffer.buffer, m_nullInstanceBuffer.buffer};
        VkDeviceSize characterOffsets[] = {0, 0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, characterBuffers, characterOffsets);
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

        VkBuffer sceneBuffers[] = {m_vertexBuffer.buffer, m_nullInstanceBuffer.buffer};
        VkDeviceSize sceneOffsets[] = {0, 0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, sceneBuffers, sceneOffsets);
        vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    }

    if (m_flatDecalIndexCount > 0 && !m_flatDecalDraws.empty())
    {
        DrawPushConstants pushConstants{};
        pushConstants.model = Mat4Identity();
        pushConstants.pointLightMask = 0xFu;
        pushConstants.spotLightMask = 0xFFFFFFFFu;
        pushConstants.shadowedSpotLightMask = (1u << kMaxShadowedSpotLights) - 1u;

        VkBuffer decalInstanceBuffers[] = {m_flatDecalVertexBuffer.buffer, m_nullInstanceBuffer.buffer};
        VkDeviceSize decalOffsets[] = {0, 0};
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_flatDecalPipeline);
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, decalInstanceBuffers, decalOffsets);
        vkCmdBindIndexBuffer(commandBuffer, m_flatDecalIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        for (const FlatDecalDraw& decalDraw : m_flatDecalDraws)
        {
            pushConstants.materialFlags = 2u;
            if (decalDraw.flipNormalY)
            {
                pushConstants.materialFlags |= 1u;
            }
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
                &m_descriptorSets[decalDraw.descriptorIndex],
                0,
                nullptr
            );
            vkCmdDrawIndexed(commandBuffer, decalDraw.indexCount, 1, decalDraw.firstIndex, 0, 0);
        }

        VkBuffer sceneBuffers[] = {m_vertexBuffer.buffer, m_nullInstanceBuffer.buffer};
        VkDeviceSize sceneOffsets[] = {0, 0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, sceneBuffers, sceneOffsets);
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
        if (m_supportsWideLines)
        {
            vkCmdSetLineWidth(commandBuffer, 2.5f);
        }
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

    vkCmdEndRenderPass(commandBuffer);
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, m_timestampQueryPool, 4);

    renderPassInfo.renderPass = m_renderPass;
    renderPassInfo.framebuffer = m_framebuffers[imageIndex];
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    float bloomStrength = m_useProcCityPipeline ? 0.0f : 1.0f;
    std::array<OverlayVertex, 6> fullscreenQuad =
        {
            OverlayVertex{Vec2Make(-1.0f, -1.0f), Vec2Make(0.0f, 1.0f), Vec4Make(1.0f, 1.0f, 1.0f, bloomStrength)},
            OverlayVertex{Vec2Make(-1.0f, 1.0f), Vec2Make(0.0f, 0.0f), Vec4Make(1.0f, 1.0f, 1.0f, bloomStrength)},
            OverlayVertex{Vec2Make(1.0f, 1.0f), Vec2Make(1.0f, 0.0f), Vec4Make(1.0f, 1.0f, 1.0f, bloomStrength)},
            OverlayVertex{Vec2Make(-1.0f, -1.0f), Vec2Make(0.0f, 1.0f), Vec4Make(1.0f, 1.0f, 1.0f, bloomStrength)},
            OverlayVertex{Vec2Make(1.0f, 1.0f), Vec2Make(1.0f, 0.0f), Vec4Make(1.0f, 1.0f, 1.0f, bloomStrength)},
            OverlayVertex{Vec2Make(1.0f, -1.0f), Vec2Make(1.0f, 1.0f), Vec4Make(1.0f, 1.0f, 1.0f, bloomStrength)},
        };
    std::memcpy(m_postVertexBuffer.mapped, fullscreenQuad.data(), sizeof(fullscreenQuad));

    VkBuffer postBuffers[] = {m_postVertexBuffer.buffer};
    VkDeviceSize overlayOffsets[] = {0};
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postPipeline);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, postBuffers, overlayOffsets);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_postPipelineLayout,
        0,
        1,
        &m_postDescriptorSet,
        0,
        nullptr
    );
    vkCmdDraw(commandBuffer, 6, 1, 0, 0);

    if (m_overlayVertexCount > 0)
    {
        VkBuffer overlayBuffers[] = {m_overlayVertexBuffer.buffer};
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_overlayPipeline);
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, overlayBuffers, overlayOffsets);
        for (std::uint32_t i = 0; i < m_overlayBatchCount; ++i)
        {
            const OverlayBatch& batch = m_overlayBatches[i];
            VkDescriptorSet descriptorSet = m_overlayDescriptorSets[batch.atlasIndex];
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_overlayPipelineLayout,
                0,
                1,
                &descriptorSet,
                0,
                nullptr
            );
            vkCmdDraw(commandBuffer, batch.vertexCount, 1, batch.firstVertex, 0);
        }
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
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(m_physicalDevice, VK_FORMAT_D32_SFLOAT, &properties);
    if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
    {
        return VK_FORMAT_D32_SFLOAT;
    }

    throw std::runtime_error("Failed to find supported pure depth format");
}
