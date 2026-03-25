#pragma once

#include <cstdint>
#include <vector>

#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>

#include "scene.h"
#include "vulkan_helpers.h"

struct alignas(16) SceneUniforms
{
    Mat4 model;
    Mat4 view;
    Mat4 proj;
    Vec4 cameraPosition;
    Vec4 pointLightPosition;
    Vec4 pointLightColor;
    Vec4 ambientColor;
};

struct VulkanRenderer
{
    ~VulkanRenderer();

    void Initialize(SDL_Window* window, const SceneData& scene);
    void Shutdown();
    void Resize(std::uint32_t width, std::uint32_t height);
    void Render(const SceneUniforms& uniforms);

    void CreateInstance();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateDevice();
    void CreateSwapchain(std::uint32_t width, std::uint32_t height);
    void DestroySwapchain();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateCommandObjects();
    void CreateSyncObjects();
    void CreateDescriptorObjects();
    void CreatePipeline();
    void CreateSceneBuffers(const SceneData& scene);
    void CreateTextureResources(const TextureData& texture);
    void CreateDepthResources();
    void UpdateDescriptorSet();
    void RecordCommandBuffer(std::uint32_t imageIndex);

    VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes) const;
    std::uint32_t ChooseQueueFamily() const;
    VkFormat FindDepthFormat() const;

    SDL_Window* m_window = nullptr;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    std::uint32_t m_queueFamilyIndex = 0;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent = {};
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    std::vector<VkFramebuffer> m_framebuffers;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkShaderModule m_vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_fragShaderModule = VK_NULL_HANDLE;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_frameFence = VK_NULL_HANDLE;

    BufferResource m_vertexBuffer;
    BufferResource m_indexBuffer;
    BufferResource m_uniformBuffer;
    ImageResource m_textureImage;
    VkSampler m_textureSampler = VK_NULL_HANDLE;
    ImageResource m_depthImage;

    std::uint32_t m_indexCount = 0;
    bool m_initialized = false;
};
