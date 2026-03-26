#include "render/renderer.h"
#include "render/internal.h"

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
    CreateLightLinePipeline();
    CreateLightSolidPipeline();
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
    DestroyBuffer(m_device, m_lightLineBuffer);
    DestroyBuffer(m_device, m_lightSolidBuffer);
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
    if (m_timestampQueryPool != VK_NULL_HANDLE)
    {
        vkDestroyQueryPool(m_device, m_timestampQueryPool, nullptr);
        m_timestampQueryPool = VK_NULL_HANDLE;
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
    if (m_lightLinePipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_lightLinePipeline, nullptr);
        m_lightLinePipeline = VK_NULL_HANDLE;
    }
    if (m_lightSolidPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(m_device, m_lightSolidPipeline, nullptr);
        m_lightSolidPipeline = VK_NULL_HANDLE;
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
    if (m_lightLineFragShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(m_device, m_lightLineFragShaderModule, nullptr);
        m_lightLineFragShaderModule = VK_NULL_HANDLE;
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
