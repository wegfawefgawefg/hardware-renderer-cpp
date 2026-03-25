#include "vulkan_renderer.h"

#include <SDL3/SDL_vulkan.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace
{
constexpr std::string_view kVertShaderPath = HARDWARE_RENDERER_VERT_SHADER_PATH;
constexpr std::string_view kFragShaderPath = HARDWARE_RENDERER_FRAG_SHADER_PATH;
constexpr std::string_view kOverlayVertShaderPath = HARDWARE_RENDERER_OVERLAY_VERT_SHADER_PATH;
constexpr std::string_view kOverlayFragShaderPath = HARDWARE_RENDERER_OVERLAY_FRAG_SHADER_PATH;
constexpr std::string_view kLightVertShaderPath = HARDWARE_RENDERER_LIGHT_VERT_SHADER_PATH;
constexpr std::string_view kLightFragShaderPath = HARDWARE_RENDERER_LIGHT_FRAG_SHADER_PATH;
constexpr std::uint32_t kOverlayTextureWidth = 512;
constexpr std::uint32_t kOverlayTextureHeight = 128;
constexpr VkDeviceSize kOverlayTextureBytes =
    static_cast<VkDeviceSize>(kOverlayTextureWidth) * kOverlayTextureHeight * sizeof(std::uint32_t);

VkShaderModule CreateShaderModule(VkDevice device, std::string_view path)
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

void TransitionImageLayout(
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

void CopyBufferToImage(
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

std::array<OverlayVertex, 6> BuildOverlayQuad(
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

VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

void VulkanRenderer::Initialize(SDL_Window* window, const SceneData& scene)
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
    CreateDescriptorObjects();
    CreateOverlayDescriptorObjects();
    CreateSceneBuffers(scene);
    CreateTextureResources(scene.texture);

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

    DestroyOverlayResources();
    DestroyImage(m_device, m_textureImage);
    DestroyBuffer(m_device, m_overlayVertexBuffer);
    DestroyBuffer(m_device, m_lightMarkerBuffer);
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
}

void VulkanRenderer::Render(
    const SceneUniforms& uniforms,
    std::span<const std::uint32_t> overlayPixels,
    std::uint32_t overlayWidth,
    std::uint32_t overlayHeight
)
{
    if (!m_initialized)
    {
        return;
    }

    CheckVk(vkWaitForFences(m_device, 1, &m_frameFence, VK_TRUE, UINT64_MAX), "vkWaitForFences");
    CheckVk(vkResetFences(m_device, 1, &m_frameFence), "vkResetFences");

    std::memcpy(m_uniformBuffer.mapped, &uniforms, sizeof(uniforms));
    if (overlayPixels.size_bytes() > kOverlayTextureBytes)
    {
        throw std::runtime_error("Overlay buffer too large");
    }
    std::memset(m_overlayUploadBuffer.mapped, 0, static_cast<std::size_t>(kOverlayTextureBytes));
    std::memcpy(m_overlayUploadBuffer.mapped, overlayPixels.data(), overlayPixels.size_bytes());
    m_overlayWidth = overlayWidth;
    m_overlayHeight = overlayHeight;

    std::array<LightMarkerVertex, 4> lightMarkers{};
    for (std::size_t i = 0; i < lightMarkers.size(); ++i)
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

    CheckVk(
        vkResetCommandBuffer(m_commandBuffers[imageIndex], 0),
        "vkResetCommandBuffer"
    );
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

void VulkanRenderer::CreateInstance()
{
    unsigned int extensionCount = 0;
    const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extensionCount);
    if (extensions == nullptr)
    {
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "hardware-renderer-cpp";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "none";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = extensionCount;
    createInfo.ppEnabledExtensionNames = extensions;
    CheckVk(vkCreateInstance(&createInfo, nullptr, &m_instance), "vkCreateInstance");
}

void VulkanRenderer::CreateSurface()
{
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface))
    {
        throw std::runtime_error("SDL_Vulkan_CreateSurface failed");
    }
}

void VulkanRenderer::PickPhysicalDevice()
{
    std::uint32_t deviceCount = 0;
    CheckVk(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr), "vkEnumeratePhysicalDevices(count)");
    if (deviceCount == 0)
    {
        throw std::runtime_error("No Vulkan physical devices found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    CheckVk(vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data()), "vkEnumeratePhysicalDevices(list)");

    for (VkPhysicalDevice device : devices)
    {
        m_physicalDevice = device;
        try
        {
            m_queueFamilyIndex = ChooseQueueFamily();
            return;
        }
        catch (const std::exception&)
        {
        }
    }

    throw std::runtime_error("Failed to find a suitable Vulkan physical device");
}

void VulkanRenderer::CreateDevice()
{
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &queuePriority;

    std::array<const char*, 1> deviceExtensions =
        {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        };

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    createInfo.pEnabledFeatures = &features;
    CheckVk(vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device), "vkCreateDevice");

    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_graphicsQueue);
    m_presentQueue = m_graphicsQueue;
}

void VulkanRenderer::CreateSwapchain(std::uint32_t width, std::uint32_t height)
{
    VkSurfaceCapabilitiesKHR capabilities{};
    CheckVk(
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities),
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"
    );

    std::uint32_t formatCount = 0;
    CheckVk(
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr),
        "vkGetPhysicalDeviceSurfaceFormatsKHR(count)"
    );
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    CheckVk(
        vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data()),
        "vkGetPhysicalDeviceSurfaceFormatsKHR(list)"
    );

    std::uint32_t presentModeCount = 0;
    CheckVk(
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr),
        "vkGetPhysicalDeviceSurfacePresentModesKHR(count)"
    );
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    CheckVk(
        vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data()),
        "vkGetPhysicalDeviceSurfacePresentModesKHR(list)"
    );

    VkSurfaceFormatKHR format = ChooseSurfaceFormat(formats);
    VkPresentModeKHR presentMode = ChoosePresentMode(presentModes);

    VkExtent2D extent{};
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        extent = capabilities.currentExtent;
    }
    else
    {
        extent.width = width;
        extent.height = height;
    }

    std::uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
    {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = format.format;
    createInfo.imageColorSpace = format.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    CheckVk(vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain), "vkCreateSwapchainKHR");

    m_swapchainFormat = format.format;
    m_swapchainExtent = extent;

    CheckVk(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr), "vkGetSwapchainImagesKHR(count)");
    m_swapchainImages.resize(imageCount);
    CheckVk(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data()), "vkGetSwapchainImagesKHR(list)");

    m_swapchainImageViews.resize(imageCount);
    for (std::uint32_t i = 0; i < imageCount; ++i)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_swapchainFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        CheckVk(vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]), "vkCreateImageView(swapchain)");
    }

    CreateDepthResources();

    m_commandBuffers.resize(imageCount);
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = m_commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = imageCount;
    CheckVk(vkAllocateCommandBuffers(m_device, &allocateInfo, m_commandBuffers.data()), "vkAllocateCommandBuffers");
}

void VulkanRenderer::DestroySwapchain()
{
    if (!m_commandBuffers.empty())
    {
        vkFreeCommandBuffers(
            m_device,
            m_commandPool,
            static_cast<std::uint32_t>(m_commandBuffers.size()),
            m_commandBuffers.data()
        );
        m_commandBuffers.clear();
    }

    for (VkFramebuffer framebuffer : m_framebuffers)
    {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    m_framebuffers.clear();

    DestroyImage(m_device, m_depthImage);

    for (VkImageView imageView : m_swapchainImageViews)
    {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    m_swapchainImageViews.clear();
    m_swapchainImages.clear();

    if (m_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::CreateRenderPass()
{
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_swapchainFormat == VK_FORMAT_UNDEFINED ? VK_FORMAT_B8G8R8A8_UNORM : m_swapchainFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = FindDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 1;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
    createInfo.pAttachments = attachments.data();
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = 1;
    createInfo.pDependencies = &dependency;
    CheckVk(vkCreateRenderPass(m_device, &createInfo, nullptr, &m_renderPass), "vkCreateRenderPass");
}

void VulkanRenderer::CreateFramebuffers()
{
    m_framebuffers.resize(m_swapchainImageViews.size());
    for (std::size_t i = 0; i < m_swapchainImageViews.size(); ++i)
    {
        std::array<VkImageView, 2> attachments =
            {
                m_swapchainImageViews[i],
                m_depthImage.view,
            };

        VkFramebufferCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        createInfo.renderPass = m_renderPass;
        createInfo.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        createInfo.pAttachments = attachments.data();
        createInfo.width = m_swapchainExtent.width;
        createInfo.height = m_swapchainExtent.height;
        createInfo.layers = 1;
        CheckVk(vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_framebuffers[i]), "vkCreateFramebuffer");
    }
}

void VulkanRenderer::CreateCommandObjects()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_queueFamilyIndex;
    CheckVk(vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool), "vkCreateCommandPool");
}

void VulkanRenderer::CreateSyncObjects()
{
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    CheckVk(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphore), "vkCreateSemaphore(imageAvailable)");
    CheckVk(vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphore), "vkCreateSemaphore(renderFinished)");

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    CheckVk(vkCreateFence(m_device, &fenceInfo, nullptr, &m_frameFence), "vkCreateFence");
}

void VulkanRenderer::CreateDescriptorObjects()
{
    VkDescriptorSetLayoutBinding uniformBinding{};
    uniformBinding.binding = 0;
    uniformBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformBinding.descriptorCount = 1;
    uniformBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 1;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uniformBinding, samplerBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    CheckVk(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout), "vkCreateDescriptorSetLayout");

    std::array<VkDescriptorPoolSize, 2> poolSizes =
        {
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1},
        };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    CheckVk(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool), "vkCreateDescriptorPool");

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    CheckVk(vkAllocateDescriptorSets(m_device, &allocInfo, &m_descriptorSet), "vkAllocateDescriptorSets");
}

void VulkanRenderer::CreateOverlayDescriptorObjects()
{
    VkDescriptorSetLayoutBinding samplerBinding{};
    samplerBinding.binding = 0;
    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerBinding.descriptorCount = 1;
    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &samplerBinding;
    CheckVk(
        vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_overlayDescriptorSetLayout),
        "vkCreateDescriptorSetLayout(overlay)"
    );

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    CheckVk(
        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_overlayDescriptorPool),
        "vkCreateDescriptorPool(overlay)"
    );

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_overlayDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_overlayDescriptorSetLayout;
    CheckVk(
        vkAllocateDescriptorSets(m_device, &allocInfo, &m_overlayDescriptorSet),
        "vkAllocateDescriptorSets(overlay)"
    );
}

void VulkanRenderer::CreatePipeline()
{
    m_vertShaderModule = CreateShaderModule(m_device, kVertShaderPath);
    m_fragShaderModule = CreateShaderModule(m_device, kFragShaderPath);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = m_vertShaderModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = m_fragShaderModule;
    fragStage.pName = "main";

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(Vertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(Vertex, position);
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(Vertex, normal);
    attributes[2].location = 2;
    attributes[2].binding = 0;
    attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[2].offset = offsetof(Vertex, uv);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    std::array<VkDynamicState, 2> dynamicStates =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState colorAttachment{};
    colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                     VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT |
                                     VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorAttachment;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    CheckVk(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_pipelineLayout), "vkCreatePipelineLayout");

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertStage, fragStage};

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<std::uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    CheckVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline), "vkCreateGraphicsPipelines");
}

void VulkanRenderer::CreateOverlayPipeline()
{
    m_overlayVertShaderModule = CreateShaderModule(m_device, kOverlayVertShaderPath);
    m_overlayFragShaderModule = CreateShaderModule(m_device, kOverlayFragShaderPath);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = m_overlayVertShaderModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = m_overlayFragShaderModule;
    fragStage.pName = "main";

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(OverlayVertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attributes{};
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[0].offset = offsetof(OverlayVertex, position);
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = offsetof(OverlayVertex, uv);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    std::array<VkDynamicState, 2> dynamicStates =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorAttachment{};
    colorAttachment.blendEnable = VK_TRUE;
    colorAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                     VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT |
                                     VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorAttachment;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_overlayDescriptorSetLayout;
    CheckVk(
        vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_overlayPipelineLayout),
        "vkCreatePipelineLayout(overlay)"
    );

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertStage, fragStage};

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<std::uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_overlayPipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    CheckVk(
        vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_overlayPipeline),
        "vkCreateGraphicsPipelines(overlay)"
    );
}

void VulkanRenderer::CreateLightPipeline()
{
    m_lightVertShaderModule = CreateShaderModule(m_device, kLightVertShaderPath);
    m_lightFragShaderModule = CreateShaderModule(m_device, kLightFragShaderPath);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = m_lightVertShaderModule;
    vertStage.pName = "main";

    VkPipelineShaderStageCreateInfo fragStage{};
    fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragStage.module = m_lightFragShaderModule;
    fragStage.pName = "main";

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = sizeof(LightMarkerVertex);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attributes{};
    attributes[0].location = 0;
    attributes[0].binding = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(LightMarkerVertex, position);
    attributes[1].location = 1;
    attributes[1].binding = 0;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(LightMarkerVertex, color);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &bindingDesc;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    std::array<VkDynamicState, 2> dynamicStates =
        {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineRasterizationStateCreateInfo rasterization{};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_FALSE;
    depthStencil.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorAttachment{};
    colorAttachment.blendEnable = VK_TRUE;
    colorAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                     VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT |
                                     VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorAttachment;

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {vertStage, fragStage};

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = static_cast<std::uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_renderPass;
    pipelineInfo.subpass = 0;
    CheckVk(
        vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_lightPipeline),
        "vkCreateGraphicsPipelines(light)"
    );
}

void VulkanRenderer::CreateSceneBuffers(const SceneData& scene)
{
    m_indexCount = static_cast<std::uint32_t>(scene.mesh.indices.size());

    VkDeviceSize vertexSize = sizeof(Vertex) * scene.mesh.vertices.size();
    VkDeviceSize indexSize = sizeof(std::uint32_t) * scene.mesh.indices.size();
    VkDeviceSize uniformSize = sizeof(SceneUniforms);

    m_vertexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        vertexSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        true
    );
    m_indexBuffer = CreateBuffer(
        m_physicalDevice,
        m_device,
        indexSize,
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

    std::memcpy(m_vertexBuffer.mapped, scene.mesh.vertices.data(), static_cast<std::size_t>(vertexSize));
    std::memcpy(m_indexBuffer.mapped, scene.mesh.indices.data(), static_cast<std::size_t>(indexSize));
    std::array<OverlayVertex, 6> quad = BuildOverlayQuad(
        m_swapchainExtent.width,
        m_swapchainExtent.height,
        1,
        1
    );
    std::memcpy(m_overlayVertexBuffer.mapped, quad.data(), sizeof(quad));
}

void VulkanRenderer::CreateTextureResources(const TextureData& texture)
{
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

    m_textureImage = CreateImage2D(
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
        m_textureImage.image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    );
    CopyBufferToImage(
        m_device,
        m_graphicsQueue,
        m_commandPool,
        stagingBuffer.buffer,
        m_textureImage.image,
        texture.width,
        texture.height
    );
    TransitionImageLayout(
        m_device,
        m_graphicsQueue,
        m_commandPool,
        m_textureImage.image,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    );

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
    VkDescriptorBufferInfo uniformInfo{};
    uniformInfo.buffer = m_uniformBuffer.buffer;
    uniformInfo.offset = 0;
    uniformInfo.range = sizeof(SceneUniforms);

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = m_textureSampler;
    imageInfo.imageView = m_textureImage.view;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet uniformWrite{};
    uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    uniformWrite.dstSet = m_descriptorSet;
    uniformWrite.dstBinding = 0;
    uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformWrite.descriptorCount = 1;
    uniformWrite.pBufferInfo = &uniformInfo;

    VkWriteDescriptorSet imageWrite{};
    imageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    imageWrite.dstSet = m_descriptorSet;
    imageWrite.dstBinding = 1;
    imageWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    imageWrite.descriptorCount = 1;
    imageWrite.pImageInfo = &imageInfo;

    std::array<VkWriteDescriptorSet, 2> writes = {uniformWrite, imageWrite};
    vkUpdateDescriptorSets(m_device, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
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

void VulkanRenderer::RecordCommandBuffer(std::uint32_t imageIndex)
{
    VkCommandBuffer commandBuffer = m_commandBuffers[imageIndex];

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    CheckVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer");

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.09f, 0.10f, 0.12f, 1.0f}};
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
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineLayout,
        0,
        1,
        &m_descriptorSet,
        0,
        nullptr
    );
    vkCmdDrawIndexed(commandBuffer, m_indexCount, 1, 0, 0, 0);

    VkBuffer lightBuffers[] = {m_lightMarkerBuffer.buffer};
    VkDeviceSize lightOffsets[] = {0};
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightPipeline);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, lightBuffers, lightOffsets);
    vkCmdBindDescriptorSets(
        commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_pipelineLayout,
        0,
        1,
        &m_descriptorSet,
        0,
        nullptr
    );
    vkCmdDraw(commandBuffer, 4, 1, 0, 0);

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

    throw std::runtime_error("Failed to find depth format");
}
