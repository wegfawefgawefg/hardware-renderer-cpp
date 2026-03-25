#pragma once

#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>

#include "imgui.h"

#include "animation/character.h"
#include "scene.h"
#include "vulkan_helpers.h"

struct alignas(16) SceneUniforms
{
    Mat4 view;
    Mat4 proj;
    Vec4 cameraPosition;
    Vec4 lightPositions[4];
    Vec4 lightColors[4];
    Vec4 sunDirection;
    Vec4 sunColor;
    Vec4 ambientColor;
    Vec4 celestialPositions[2];
    Vec4 celestialColors[2];
    Vec4 clearColor;
    Mat4 shadowViewProj;
    Vec4 shadowParams;
    Mat4 skinJoints[64];
};

struct alignas(16) DrawPushConstants
{
    Mat4 model;
    std::uint32_t skinned = 0;
    std::uint32_t padding0 = 0;
    std::uint32_t padding1 = 0;
    std::uint32_t padding2 = 0;
};

struct OverlayVertex
{
    Vec2 position;
    Vec2 uv;
};

struct LightMarkerVertex
{
    Vec3 position;
    Vec3 color;
};

struct VulkanRenderer
{
    ~VulkanRenderer();

    void Initialize(
        SDL_Window* window,
        const SceneData& scene,
        const SkinnedCharacterAsset* characterAsset = nullptr
    );
    void Shutdown();
    void Resize(std::uint32_t width, std::uint32_t height);
    void UpdateSceneTransforms(const SceneData& scene);
    void InitializeImGuiBackend();
    void ShutdownImGuiBackend();
    ImTextureID GetShadowDebugTexture() const;
    void Render(
        const SceneUniforms& uniforms,
        std::span<const std::uint32_t> overlayPixels,
        std::uint32_t overlayWidth,
        std::uint32_t overlayHeight,
        const CharacterRenderState* characterState = nullptr
    );

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
    void CreateOverlayDescriptorObjects();
    void CreateOverlayPipeline();
    void CreateLightPipeline();
    void CreateShadowResources();
    void DestroyShadowResources();
    void CreateShadowRenderPass();
    void CreateShadowPipeline();
    void CreateSceneBuffers(const SceneData& scene);
    void CreateTextureResources(const SceneData& scene);
    void CreateCharacterResources(const SkinnedCharacterAsset& characterAsset);
    void CreateOverlayResources();
    void DestroyOverlayResources();
    void CreateDepthResources();
    void UpdateDescriptorSet();
    void UpdateOverlayDescriptorSet();
    void RecordShadowPass(VkCommandBuffer commandBuffer);
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
    std::vector<VkDescriptorSet> m_descriptorSets;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkShaderModule m_vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_fragShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_overlayDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_overlayDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_overlayDescriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_overlayPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_overlayPipeline = VK_NULL_HANDLE;
    VkShaderModule m_overlayVertShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_overlayFragShaderModule = VK_NULL_HANDLE;
    VkPipeline m_lightPipeline = VK_NULL_HANDLE;
    VkShaderModule m_lightVertShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_lightFragShaderModule = VK_NULL_HANDLE;
    VkRenderPass m_shadowRenderPass = VK_NULL_HANDLE;
    VkPipelineLayout m_shadowPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_shadowPipeline = VK_NULL_HANDLE;
    VkShaderModule m_shadowVertShaderModule = VK_NULL_HANDLE;
    VkFramebuffer m_shadowFramebuffer = VK_NULL_HANDLE;
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_imguiShadowDescriptor = VK_NULL_HANDLE;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_frameFence = VK_NULL_HANDLE;

    BufferResource m_vertexBuffer;
    BufferResource m_indexBuffer;
    BufferResource m_uniformBuffer;
    BufferResource m_overlayUploadBuffer;
    BufferResource m_overlayVertexBuffer;
    BufferResource m_lightMarkerBuffer;
    BufferResource m_characterVertexBuffer;
    BufferResource m_characterIndexBuffer;
    std::vector<ImageResource> m_textureImages;
    VkSampler m_textureSampler = VK_NULL_HANDLE;
    ImageResource m_overlayImage;
    VkSampler m_overlaySampler = VK_NULL_HANDLE;
    ImageResource m_depthImage;
    ImageResource m_shadowImage;
    VkSampler m_shadowSampler = VK_NULL_HANDLE;

    struct DrawItem
    {
        Mat4 model;
        std::uint32_t firstIndex = 0;
        std::uint32_t indexCount = 0;
        std::uint32_t descriptorIndex = 0;
        std::uint32_t skinned = 0;
        std::uint32_t entityIndex = 0;
    };

    std::vector<DrawItem> m_drawItems;
    CharacterRenderState m_characterState = {};
    std::uint32_t m_characterIndexCount = 0;
    std::uint32_t m_characterDescriptorIndex = std::numeric_limits<std::uint32_t>::max();
    bool m_hasCharacter = false;
    std::uint32_t m_overlayTextureWidth = 0;
    std::uint32_t m_overlayTextureHeight = 0;
    std::uint32_t m_overlayWidth = 0;
    std::uint32_t m_overlayHeight = 0;
    std::uint32_t m_shadowMapSize = 2048;
    Vec4 m_clearColor = {0.09f, 0.10f, 0.12f, 1.0f};
    bool m_imguiInitialized = false;
    bool m_initialized = false;
};
