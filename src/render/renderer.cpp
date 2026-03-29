#include "render/renderer.h"
#include "render/internal.h"

#include <array>
#include <cmath>
#include <cstring>

void VulkanRenderer::Render(
    const SceneUniforms& uniforms,
    const text::System& text,
    const decals::FlatDecalSystem* flatDecals,
    const CharacterRenderState* characterState,
    const DebugRenderOptions* debugOptions
)
{
    if (!m_initialized)
    {
        return;
    }

    CheckVk(vkWaitForFences(m_device, 1, &m_frameFence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    std::array<std::uint64_t, kGpuTimestampCount> timestamps{};
    VkResult queryResult = vkGetQueryPoolResults(
        m_device,
        m_timestampQueryPool,
        0,
        kGpuTimestampCount,
        sizeof(timestamps),
        timestamps.data(),
        sizeof(std::uint64_t),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT
    );
    if (queryResult == VK_SUCCESS)
    {
        const double nsToMs = 1.0e-6 * static_cast<double>(m_gpuTimestampPeriodNs);
        auto deltaMs = [&](std::size_t a, std::size_t b) -> float {
            if (timestamps[b] <= timestamps[a])
            {
                return 0.0f;
            }
            return static_cast<float>(static_cast<double>(timestamps[b] - timestamps[a]) * nsToMs);
        };
        m_profilingStats.gpuSunShadowMs = deltaMs(0, 1);
        m_profilingStats.gpuSpotShadowMs = deltaMs(1, 2);
        m_profilingStats.gpuShadowMs = deltaMs(0, 2);
        m_profilingStats.gpuMainMs = deltaMs(2, 3);
        m_profilingStats.gpuDebugMs = deltaMs(3, 4);
        m_profilingStats.gpuUiMs = deltaMs(4, 5);
        m_profilingStats.gpuFrameMs = deltaMs(0, 5);
        m_profilingStats.gpuValid = true;
    }
    else
    {
        m_profilingStats.gpuValid = false;
    }
    CheckVk(vkResetFences(m_device, 1, &m_frameFence), "vkResetFences");
    FlushDirtyPaintTextures();

    std::memcpy(m_uniformBuffer.mapped, &uniforms, sizeof(uniforms));
    std::uint32_t shaderFeatureMask = static_cast<std::uint32_t>(std::lround(uniforms.shadowParams.w));
    m_sunShadowsEnabled = (shaderFeatureMask & (1u << 3)) != 0u;
    m_localLightShadowsEnabled = (shaderFeatureMask & (1u << 5)) != 0u;
    std::uint32_t activeSunShadowCascades = m_sunShadowsEnabled ? kSunShadowCascadeCount : 0u;
    m_activeShadowedSpotCount = m_localLightShadowsEnabled
        ? std::min<std::uint32_t>(static_cast<std::uint32_t>(uniforms.sceneLightCounts.y), kMaxShadowedSpotLights)
        : 0u;
    m_activeShadowMapCount = activeSunShadowCascades + m_activeShadowedSpotCount;
    DebugRenderOptions debug = debugOptions != nullptr ? *debugOptions : DebugRenderOptions{};
    m_mainCullDistance = std::max(debug.mainDrawDistance, 1.0f);
    m_shadowCullDistance = m_activeShadowMapCount > 0 ? std::max(debug.shadowDrawDistance, 1.0f) : 1.0f;
    UpdateMainPassVisibility(uniforms);
    UpdateDrawLightMasks(uniforms);
    UpdateFlatDecalGeometry(flatDecals);
    m_characterState = characterState != nullptr ? *characterState : CharacterRenderState{};
    m_clearColor = uniforms.clearColor;
    SyncOverlayAtlases(text);
    UpdateOverlayGeometry(text);

    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightMarkerCount> lightMarkers{};
    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightLineVertexCount> lightLines{};
    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightSolidVertexCount> lightSolids{};
    BuildDebugLightGeometry(uniforms, debug, lightMarkers, lightLines, lightSolids);
    std::memcpy(m_lightMarkerBuffer.mapped, lightMarkers.data(), sizeof(lightMarkers));
    std::memcpy(m_lightLineBuffer.mapped, lightLines.data(), sizeof(lightLines));
    std::memcpy(m_lightSolidBuffer.mapped, lightSolids.data(), sizeof(lightSolids));

    std::uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        m_device,
        m_swapchain,
        UINT64_MAX,
        m_imageAvailableSemaphore,
        VK_NULL_HANDLE,
        &imageIndex
    );
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        Resize(m_swapchainExtent.width, m_swapchainExtent.height);
        return;
    }
    CheckVk(acquireResult, "vkAcquireNextImageKHR");

    CheckVk(vkResetCommandBuffer(m_commandBuffers[imageIndex], 0), "vkResetCommandBuffer");
    RecordCommandBuffer(imageIndex);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &m_imageAvailableSemaphore;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &m_renderFinishedSemaphore;
    CheckVk(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_frameFence), "vkQueueSubmit");

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &m_renderFinishedSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentResult = vkQueuePresentKHR(m_presentQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
    {
        Resize(m_swapchainExtent.width, m_swapchainExtent.height);
        return;
    }
    CheckVk(presentResult, "vkQueuePresentKHR");
}
