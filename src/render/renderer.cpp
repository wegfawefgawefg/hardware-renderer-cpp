#include "render/renderer.h"
#include "render/internal.h"

#include <array>
#include <cstring>

#include "backends/imgui_impl_vulkan.h"

VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

void VulkanRenderer::Initialize(
    SDL_Window* window,
    const SceneData& scene,
    const SkinnedCharacterAsset* characterAsset
)
{
    if (m_initialized)
    {
        return;
    }

    m_window = window;
    CreateInstance();
    CreateSurface();
    PickPhysicalDevice();
    CreateDevice();
    CreateCommandObjects();
    CreateSyncObjects();
    CreateSceneBuffers(scene);
    if (characterAsset != nullptr)
    {
        CreateCharacterResources(*characterAsset);
    }
    CreateTextureResources(scene);
    CreateShadowRenderPass();
    CreateShadowResources();
    CreateDescriptorObjects();
    CreateOverlayDescriptorObjects();

    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    CreateOverlayResources();
    CreateSwapchain(
        static_cast<std::uint32_t>(width > 0 ? width : 1),
        static_cast<std::uint32_t>(height > 0 ? height : 1)
    );
    CreateRenderPass();
    CreatePipeline();
    CreateShadowPipeline();
    CreateLightPipeline();
    CreateOverlayPipeline();
    CreateFramebuffers();
    UpdateDescriptorSet();
    UpdateOverlayDescriptorSet();

    m_initialized = true;
}

void VulkanRenderer::Shutdown()
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_device);
    }

    DestroySwapchain();

    if (m_textureSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_device, m_textureSampler, nullptr);
        m_textureSampler = VK_NULL_HANDLE;
    }
    if (m_overlaySampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_device, m_overlaySampler, nullptr);
        m_overlaySampler = VK_NULL_HANDLE;
    }
    if (m_shadowSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(m_device, m_shadowSampler, nullptr);
        m_shadowSampler = VK_NULL_HANDLE;
    }

    DestroyOverlayResources();
    DestroyShadowResources();
    for (ImageResource& textureImage : m_textureImages)
    {
        DestroyImage(m_device, textureImage);
    }
    m_textureImages.clear();
    DestroyBuffer(m_device, m_overlayVertexBuffer);
    DestroyBuffer(m_device, m_lightMarkerBuffer);
    DestroyBuffer(m_device, m_characterIndexBuffer);
    DestroyBuffer(m_device, m_characterVertexBuffer);
    DestroyBuffer(m_device, m_uniformBuffer);
    DestroyBuffer(m_device, m_indexBuffer);
    DestroyBuffer(m_device, m_vertexBuffer);

    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
    if (m_renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }
    if (m_vertShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_vertShaderModule, nullptr);
        m_vertShaderModule = VK_NULL_HANDLE;
    }
    if (m_fragShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_fragShaderModule, nullptr);
        m_fragShaderModule = VK_NULL_HANDLE;
    }
    if (m_descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }
    if (m_descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
        m_descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_overlayPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_overlayPipeline, nullptr);
        m_overlayPipeline = VK_NULL_HANDLE;
    }
    if (m_overlayPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_overlayPipelineLayout, nullptr);
        m_overlayPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_overlayVertShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_overlayVertShaderModule, nullptr);
        m_overlayVertShaderModule = VK_NULL_HANDLE;
    }
    if (m_overlayFragShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_overlayFragShaderModule, nullptr);
        m_overlayFragShaderModule = VK_NULL_HANDLE;
    }
    if (m_lightPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_lightPipeline, nullptr);
        m_lightPipeline = VK_NULL_HANDLE;
    }
    if (m_lightVertShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_lightVertShaderModule, nullptr);
        m_lightVertShaderModule = VK_NULL_HANDLE;
    }
    if (m_lightFragShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_lightFragShaderModule, nullptr);
        m_lightFragShaderModule = VK_NULL_HANDLE;
    }
    if (m_shadowPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_shadowPipeline, nullptr);
        m_shadowPipeline = VK_NULL_HANDLE;
    }
    if (m_shadowPipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(m_device, m_shadowPipelineLayout, nullptr);
        m_shadowPipelineLayout = VK_NULL_HANDLE;
    }
    if (m_shadowVertShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_shadowVertShaderModule, nullptr);
        m_shadowVertShaderModule = VK_NULL_HANDLE;
    }
    if (m_shadowRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(m_device, m_shadowRenderPass, nullptr);
        m_shadowRenderPass = VK_NULL_HANDLE;
    }
    if (m_overlayDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_overlayDescriptorPool, nullptr);
        m_overlayDescriptorPool = VK_NULL_HANDLE;
    }
    if (m_overlayDescriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(m_device, m_overlayDescriptorSetLayout, nullptr);
        m_overlayDescriptorSetLayout = VK_NULL_HANDLE;
    }
    if (m_imguiInitialized)
    {
        ShutdownImGuiBackend();
    }

    if (m_imageAvailableSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device, m_imageAvailableSemaphore, nullptr);
        m_imageAvailableSemaphore = VK_NULL_HANDLE;
    }
    if (m_renderFinishedSemaphore != VK_NULL_HANDLE)
    {
        vkDestroySemaphore(m_device, m_renderFinishedSemaphore, nullptr);
        m_renderFinishedSemaphore = VK_NULL_HANDLE;
    }
    if (m_frameFence != VK_NULL_HANDLE)
    {
        vkDestroyFence(m_device, m_frameFence, nullptr);
        m_frameFence = VK_NULL_HANDLE;
    }

    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
    }
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }

    m_initialized = false;
}

void VulkanRenderer::Resize(std::uint32_t width, std::uint32_t height)
{
    if (!m_initialized || width == 0 || height == 0)
    {
        return;
    }

    vkDeviceWaitIdle(m_device);
    DestroySwapchain();
    CreateSwapchain(width, height);
    CreateFramebuffers();
    if (m_imguiInitialized)
    {
        ImGui_ImplVulkan_SetMinImageCount(static_cast<std::uint32_t>(m_swapchainImages.size()));
    }
}

void VulkanRenderer::Render(
    const SceneUniforms& uniforms,
    std::span<const std::uint32_t> overlayPixels,
    std::uint32_t overlayWidth,
    std::uint32_t overlayHeight,
    const CharacterRenderState* characterState
)
{
    if (!m_initialized)
    {
        return;
    }

    CheckVk(vkWaitForFences(m_device, 1, &m_frameFence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    CheckVk(vkResetFences(m_device, 1, &m_frameFence), "vkResetFences");

    std::memcpy(m_uniformBuffer.mapped, &uniforms, sizeof(uniforms));
    m_characterState = characterState != nullptr ? *characterState : CharacterRenderState{};
    m_clearColor = uniforms.clearColor;
    if (overlayPixels.size_bytes() > vulkan_renderer_internal::kOverlayTextureBytes)
    {
        return;
    }
    std::memset(
        m_overlayUploadBuffer.mapped,
        0,
        static_cast<std::size_t>(vulkan_renderer_internal::kOverlayTextureBytes)
    );
    std::memcpy(m_overlayUploadBuffer.mapped, overlayPixels.data(), overlayPixels.size_bytes());
    m_overlayWidth = overlayWidth;
    m_overlayHeight = overlayHeight;

    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightMarkerCount> lightMarkers{};
    for (std::size_t i = 0; i < 4; ++i)
    {
        lightMarkers[i].position = Vec3Make(
            uniforms.lightPositions[i].x,
            uniforms.lightPositions[i].y,
            uniforms.lightPositions[i].z
        );
        lightMarkers[i].color = Vec3Make(
            uniforms.lightColors[i].x,
            uniforms.lightColors[i].y,
            uniforms.lightColors[i].z
        );
    }
    for (std::size_t i = 0; i < 2; ++i)
    {
        lightMarkers[4 + i].position = Vec3Make(
            uniforms.celestialPositions[i].x,
            uniforms.celestialPositions[i].y,
            uniforms.celestialPositions[i].z
        );
        lightMarkers[4 + i].color = Vec3Make(
            uniforms.celestialColors[i].x,
            uniforms.celestialColors[i].y,
            uniforms.celestialColors[i].z
        );
    }
    std::uint32_t sceneSpotCount = static_cast<std::uint32_t>(uniforms.sceneLightCounts.x);
    for (std::uint32_t i = 0; i < kMaxSceneSpotLights; ++i)
    {
        std::size_t markerIndex = 6 + i;
        if (i < sceneSpotCount)
        {
            lightMarkers[markerIndex].position = Vec3Make(
                uniforms.spotLightPositions[i].x,
                uniforms.spotLightPositions[i].y,
                uniforms.spotLightPositions[i].z
            );
            lightMarkers[markerIndex].color = Vec3Make(0.25f, 1.0f, 1.0f);
        }
        else
        {
            lightMarkers[markerIndex].position = Vec3Make(0.0f, -10000.0f, 0.0f);
            lightMarkers[markerIndex].color = Vec3Make(0.0f, 0.0f, 0.0f);
        }
    }

    std::uint32_t shadowedSpotCount = static_cast<std::uint32_t>(uniforms.sceneLightCounts.y);
    for (std::uint32_t i = 0; i < kMaxShadowedSpotLights; ++i)
    {
        std::size_t markerIndex = 6 + kMaxSceneSpotLights + i;
        if (i < shadowedSpotCount)
        {
            lightMarkers[markerIndex].position = Vec3Make(
                uniforms.shadowedSpotLightPositions[i].x,
                uniforms.shadowedSpotLightPositions[i].y,
                uniforms.shadowedSpotLightPositions[i].z
            );
            lightMarkers[markerIndex].color = Vec3Make(1.0f, 0.35f, 1.0f);
        }
        else
        {
            lightMarkers[markerIndex].position = Vec3Make(0.0f, -10000.0f, 0.0f);
            lightMarkers[markerIndex].color = Vec3Make(0.0f, 0.0f, 0.0f);
        }
    }
    std::memcpy(m_lightMarkerBuffer.mapped, lightMarkers.data(), sizeof(lightMarkers));

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
