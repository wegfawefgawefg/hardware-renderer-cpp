#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string_view>
#include <vector>

#include <SDL3/SDL_video.h>
#include <vulkan/vulkan.h>

#include "imgui.h"

#include "animation/character.h"
#include "decals/flat_decal_system.h"
#include "gameplay/paint_balls.h"
#include "paint_runtime.h"
#include "scene.h"
#include "text/text_system.h"
#include "vulkan_helpers.h"

constexpr std::uint32_t kMaxSceneSpotLights = 32;
constexpr std::uint32_t kMaxShadowedSpotLights = 4;
constexpr std::uint32_t kMaxPaintSplats = 128;
constexpr std::uint32_t kPaintTextureSize = 64;
constexpr std::uint32_t kSunShadowCascadeCount = 2;
constexpr std::uint32_t kTotalShadowMaps = kSunShadowCascadeCount + kMaxShadowedSpotLights;
constexpr std::uint32_t kGpuTimestampCount = 6;
constexpr std::uint32_t kMaxProcCityDynamicLights = 32768;
constexpr std::uint32_t kMaxProcCityLightRefsPerInstance = 32;
constexpr std::uint32_t kMaxProcCityLightTiles = 16384;
constexpr std::uint32_t kMaxProcCityTileLightRefs = 64 * 1024 * 1024;

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
    Vec4 spotLightPositions[kMaxSceneSpotLights];
    Vec4 spotLightDirections[kMaxSceneSpotLights];
    Vec4 spotLightColors[kMaxSceneSpotLights];
    Vec4 spotLightParams[kMaxSceneSpotLights];
    Vec4 shadowedSpotLightPositions[kMaxShadowedSpotLights];
    Vec4 shadowedSpotLightDirections[kMaxShadowedSpotLights];
    Vec4 shadowedSpotLightColors[kMaxShadowedSpotLights];
    Vec4 shadowedSpotLightParams[kMaxShadowedSpotLights];
    Vec4 sceneLightCounts;
    Mat4 shadowViewProj[kTotalShadowMaps];
    Vec4 shadowParams;
    Mat4 skinJoints[64];
    Vec4 paintSplatPositions[kMaxPaintSplats];
    Vec4 paintSplatNormals[kMaxPaintSplats];
    Vec4 paintSplatColors[kMaxPaintSplats];
    Vec4 surfaceMaskParamsA;
    Vec4 surfaceMaskParamsB;
    Vec4 paintSplatCounts;
};

struct alignas(16) DrawPushConstants
{
    Mat4 model;
    std::uint32_t skinned = 0;
    std::uint32_t shadowCascade = 0;
    std::uint32_t pointLightMask = 0;
    std::uint32_t spotLightMask = 0;
    std::uint32_t shadowedSpotLightMask = 0;
    std::uint32_t materialFlags = 0;
};

struct OverlayVertex
{
    Vec2 position;
    Vec2 uv;
    Vec4 color;
};

struct OverlayBatch
{
    std::uint32_t atlasIndex = 0;
    std::uint32_t firstVertex = 0;
    std::uint32_t vertexCount = 0;
};

struct LightMarkerVertex
{
    Vec3 position;
    Vec3 color;
};

struct alignas(16) StaticInstanceGpu
{
    Mat4 model;
    std::uint32_t pointLightMask = 0;
    std::uint32_t spotLightMask = 0;
    std::uint32_t shadowedSpotLightMask = 0;
    std::uint32_t materialFlags = 0;
    std::uint32_t localLightListOffset = 0;
    std::uint32_t localLightCount = 0;
    std::uint32_t reserved0 = 0;
    std::uint32_t reserved1 = 0;
};

struct alignas(16) DynamicPointLightGpu
{
    Vec4 positionRange;
    Vec4 colorIntensity;
};

struct alignas(16) ProcCityLightTileGpu
{
    std::uint32_t lightOffset = 0;
    std::uint32_t lightCount = 0;
    std::uint32_t reserved0 = 0;
    std::uint32_t reserved1 = 0;
};

struct DebugRenderOptions
{
    static constexpr std::uint32_t kMaxSelectionSpheres = 16;
    static constexpr std::uint32_t kMaxCustomCubes = 1024;
    static constexpr std::uint32_t kMaxCustomLines = 32768;
    static constexpr std::uint32_t kMaxCustomSolidVertices = 262144;
    bool drawLightProxies = true;
    bool drawLightMarkers = true;
    bool drawLightDirections = false;
    bool drawLightVolumes = false;
    bool drawActivationVolumes = false;
    bool useProcCityPipeline = false;
    bool useProcCityTiledLighting = false;
    float mainDrawDistance = 160.0f;
    float shadowDrawDistance = 200.0f;
    Vec4 activationVolumeA = {};
    Vec4 activationVolumeB = {};
    std::uint32_t selectionSphereCount = 0;
    std::array<Vec4, kMaxSelectionSpheres> selectionSpheres = {};
    std::array<Vec4, kMaxSelectionSpheres> selectionSphereColors = {};
    std::uint32_t customCubeCount = 0;
    std::array<Vec4, kMaxCustomCubes> customCubes = {};
    std::array<Vec4, kMaxCustomCubes> customCubeColors = {};
    std::uint32_t customLineCount = 0;
    std::array<Vec4, kMaxCustomLines> customLineStarts = {};
    std::array<Vec4, kMaxCustomLines> customLineEnds = {};
    std::array<Vec4, kMaxCustomLines> customLineColors = {};
    std::uint32_t customSolidVertexCount = 0;
    std::array<Vec4, kMaxCustomSolidVertices> customSolidVertices = {};
    std::array<Vec4, kMaxCustomSolidVertices> customSolidColors = {};
};

struct RenderProfilingStats
{
    float gpuSunShadowMs = 0.0f;
    float gpuSpotShadowMs = 0.0f;
    float gpuShadowMs = 0.0f;
    float gpuMainMs = 0.0f;
    float gpuDebugMs = 0.0f;
    float gpuUiMs = 0.0f;
    float gpuFrameMs = 0.0f;
    bool gpuValid = false;
};

struct VulkanRenderer
{
    ~VulkanRenderer();

    void Initialize(
        SDL_Window* window,
        const SceneData& scene,
        const decals::FlatDecalSystem* flatDecals = nullptr,
        const SkinnedCharacterAsset* characterAsset = nullptr,
        const text::System* textSystem = nullptr
    );
    void Shutdown();
    void Resize(std::uint32_t width, std::uint32_t height);
    void UpdateSceneTransforms(const SceneData& scene);
    bool UpdateSceneGeometry(const SceneData& scene);
    void InitializeImGuiBackend();
    void ShutdownImGuiBackend();
    ImTextureID GetShadowDebugTexture(std::uint32_t cascadeIndex) const;
    const RenderProfilingStats& GetProfilingStats() const { return m_profilingStats; }
    std::uint32_t GetProcCityMaxTileLightCount() const { return m_procCityMaxTileLightCount; }
    std::span<const ProcCityLightTileGpu> GetProcCityLightTiles() const { return m_procCityLightTiles; }
    std::uint32_t GetVisibleDrawItemCount() const
    {
        std::uint32_t count = static_cast<std::uint32_t>(m_visibleDrawItems.size());
        for (const std::vector<std::uint32_t>& batchItems : m_visibleStaticBatchDrawItems)
        {
            if (!batchItems.empty())
            {
                ++count;
            }
        }
        return count;
    }
    std::uint32_t GetDrawItemCount() const
    {
        std::uint32_t count = 0;
        std::vector<bool> countedBatches(m_staticBatches.size(), false);
        for (std::size_t drawIndex = 0; drawIndex < m_drawItems.size(); ++drawIndex)
        {
            const DrawItem& drawItem = m_drawItems[drawIndex];
            bool hasUniquePaint = drawIndex < m_paintLayers.size() && m_paintLayers[drawIndex].allocated;
            if (!drawItem.batchedStatic || drawItem.staticBatchIndex >= countedBatches.size())
            {
                ++count;
                continue;
            }
            if (hasUniquePaint)
            {
                ++count;
                continue;
            }
            if (!countedBatches[drawItem.staticBatchIndex])
            {
                countedBatches[drawItem.staticBatchIndex] = true;
                ++count;
            }
        }
        return count;
    }
    void Render(
        const SceneUniforms& uniforms,
        const text::System& text,
        const decals::FlatDecalSystem* flatDecals = nullptr,
        const CharacterRenderState* characterState = nullptr,
        const DebugRenderOptions* debugOptions = nullptr
    );

    void CreateInstance();
    void CreateSurface();
    void PickPhysicalDevice();
    void CreateDevice();
    void CreateSwapchain(std::uint32_t width, std::uint32_t height);
    void DestroySwapchain();
    void CreateRenderPass();
    void CreateSceneRenderPass();
    void CreateFramebuffers();
    void CreateCommandObjects();
    void CreateSyncObjects();
    void CreateDescriptorObjects();
    void CreatePipeline();
    void CreateProcCityPipeline();
    void CreateFlatDecalPipeline();
    void CreateOverlayDescriptorObjects();
    void CreatePostDescriptorObjects();
    void CreateOverlayPipeline();
    void CreatePostPipeline();
    void CreateLightPipeline();
    void CreateLightLinePipeline();
    void CreateLightSolidPipeline();
    void UpdateMainPassVisibility(const SceneUniforms& uniforms);
    void UpdateDrawLightMasks(const SceneUniforms& uniforms);
    void ClearStaticBatchVisibility();
    void BuildVisibleStaticInstances();
    void BuildShadowVisibleStaticInstances(std::uint32_t cascadeIndex);
    void BuildProcCityTiledLightLists(const SceneUniforms& uniforms);
    void SetProcCityDynamicLights(std::span<const DynamicPointLightGpu> lights);
    void SetProcCityTileContributionCutoff(float cutoff) { m_debugTileContributionCutoff = cutoff; }
    void SetProcCityTiledOccupancyMode(std::uint32_t mode) { m_procCityTiledOccupancyMode = mode; }
    void SetHideSceneMesh(bool hide) { m_hideSceneMesh = hide; }
    void SetHiddenModelIndices(std::uint32_t primary, std::uint32_t secondary = std::numeric_limits<std::uint32_t>::max())
    {
        m_hiddenModelIndex = primary;
        m_hiddenModelIndexSecondary = secondary;
    }
    void AppendPersistentPaint(const PaintSplatSpawn& splat);
    void ResetAccumulatedPaint();
    std::uint32_t GetAccumulatedPaintHitCount() const;
    void FlushDirtyPaintTextures();
    void BuildDebugLightGeometry(
        const SceneUniforms& uniforms,
        const DebugRenderOptions& debug,
        std::span<LightMarkerVertex> lightMarkers,
        std::span<LightMarkerVertex> lightLines,
        std::span<LightMarkerVertex> lightSolids
    );
    void CreateShadowResources();
    void DestroyShadowResources();
    void CreateShadowRenderPass();
    void CreateShadowPipeline();
    void CreateSceneBuffers(const SceneData& scene);
    void CreateTextureResources(const SceneData& scene);
    void CreateFlatDecalResources(const decals::FlatDecalSystem& flatDecals);
    void CreateCharacterResources(const SkinnedCharacterAsset& characterAsset);
    void CreateOverlayResources(const text::System* textSystem);
    void DestroyOverlayResources();
    void SyncOverlayAtlases(const text::System& textSystem);
    void CreateSceneColorResources();
    void DestroySceneColorResources();
    void CreateDepthResources();
    void UpdateDescriptorSet();
    void UpdatePaintDescriptorSet(std::uint32_t descriptorIndex);
    void UpdateOverlayDescriptorSet(std::uint32_t atlasIndex);
    void UpdatePostDescriptorSet();
    void RecordShadowPass(VkCommandBuffer commandBuffer, std::uint32_t cascadeIndex);
    void RecordCommandBuffer(std::uint32_t imageIndex);
    void UpdateFlatDecalGeometry(const decals::FlatDecalSystem* flatDecals);
    void UpdateOverlayGeometry(const text::System& text);

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
    VkFramebuffer m_sceneFramebuffer = VK_NULL_HANDLE;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    VkRenderPass m_sceneRenderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> m_descriptorSets;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipeline m_procCityPipeline = VK_NULL_HANDLE;
    VkPipeline m_flatDecalPipeline = VK_NULL_HANDLE;
    VkShaderModule m_vertShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_fragShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_procCityVertShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_procCityFragShaderModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_overlayDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_postDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_overlayDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorPool m_postDescriptorPool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, text::kMaxAtlases> m_overlayDescriptorSets = {};
    VkDescriptorSet m_postDescriptorSet = VK_NULL_HANDLE;
    VkPipelineLayout m_overlayPipelineLayout = VK_NULL_HANDLE;
    bool m_hideSceneMesh = false;
    std::uint32_t m_hiddenModelIndex = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t m_hiddenModelIndexSecondary = std::numeric_limits<std::uint32_t>::max();
    VkPipelineLayout m_postPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_overlayPipeline = VK_NULL_HANDLE;
    VkPipeline m_postPipeline = VK_NULL_HANDLE;
    VkShaderModule m_overlayVertShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_overlayFragShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_bloomFragShaderModule = VK_NULL_HANDLE;
    VkPipeline m_lightPipeline = VK_NULL_HANDLE;
    VkPipeline m_lightLinePipeline = VK_NULL_HANDLE;
    VkPipeline m_lightSolidPipeline = VK_NULL_HANDLE;
    VkShaderModule m_lightVertShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_lightFragShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_lightLineFragShaderModule = VK_NULL_HANDLE;
    VkShaderModule m_lightSolidFragShaderModule = VK_NULL_HANDLE;
    VkRenderPass m_shadowRenderPass = VK_NULL_HANDLE;
    VkPipelineLayout m_shadowPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_shadowPipeline = VK_NULL_HANDLE;
    VkShaderModule m_shadowVertShaderModule = VK_NULL_HANDLE;
    std::array<VkFramebuffer, kTotalShadowMaps> m_shadowFramebuffers = {};
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, kSunShadowCascadeCount> m_imguiShadowDescriptors = {};

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    VkSemaphore m_imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore m_renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence m_frameFence = VK_NULL_HANDLE;
    VkQueryPool m_timestampQueryPool = VK_NULL_HANDLE;
    float m_gpuTimestampPeriodNs = 1.0f;
    bool m_supportsWideLines = false;

    BufferResource m_vertexBuffer;
    BufferResource m_indexBuffer;
    BufferResource m_staticInstanceBuffer;
    BufferResource m_shadowStaticInstanceBuffer;
    BufferResource m_nullInstanceBuffer;
    BufferResource m_procCityDynamicLightBuffer;
    BufferResource m_procCityDynamicLightIndexBuffer;
    BufferResource m_procCityLightTileBuffer;
    BufferResource m_procCityTileLightIndexBuffer;
    BufferResource m_uniformBuffer;
    BufferResource m_overlayUploadBuffer;
    BufferResource m_overlayVertexBuffer;
    BufferResource m_postVertexBuffer;
    BufferResource m_paintUploadBuffer;
    BufferResource m_lightMarkerBuffer;
    BufferResource m_lightLineBuffer;
    BufferResource m_lightSolidBuffer;
    BufferResource m_characterVertexBuffer;
    BufferResource m_characterIndexBuffer;
    BufferResource m_flatDecalVertexBuffer;
    BufferResource m_flatDecalIndexBuffer;
    std::vector<ImageResource> m_textureImages;
    std::vector<ImageResource> m_normalTextureImages;
    VkSampler m_textureSampler = VK_NULL_HANDLE;
    ImageResource m_effectPatternImage;
    ImageResource m_flatNormalImage;
    VkSampler m_effectSampler = VK_NULL_HANDLE;
    std::vector<ImageResource> m_paintImages;
    VkSampler m_paintSampler = VK_NULL_HANDLE;
    ImageResource m_blankPaintImage;
    std::array<ImageResource, text::kMaxAtlases> m_overlayImages = {};
    VkSampler m_overlaySampler = VK_NULL_HANDLE;
    ImageResource m_sceneColorImage;
    VkSampler m_sceneSampler = VK_NULL_HANDLE;
    ImageResource m_depthImage;
    std::array<ImageResource, kTotalShadowMaps> m_shadowImages = {};
    VkSampler m_shadowSampler = VK_NULL_HANDLE;

    struct DrawItem
    {
        Mat4 model;
        Vec3 localBoundsCenter = {};
        float localBoundsRadius = 0.0f;
        std::uint32_t pointLightMask = 0;
        std::uint32_t spotLightMask = 0;
        std::uint32_t shadowedSpotLightMask = 0;
        std::uint32_t firstIndex = 0;
        std::uint32_t indexCount = 0;
        std::uint32_t descriptorIndex = 0;
        std::uint32_t skinned = 0;
        std::uint32_t modelIndex = 0;
        std::uint32_t entityIndex = 0;
        std::uint32_t primitiveIndex = 0;
        std::uint32_t materialFlags = 0;
        std::uint32_t staticBatchIndex = std::numeric_limits<std::uint32_t>::max();
        bool castsShadows = true;
        bool flipNormalY = true;
        bool batchedStatic = false;
    };

    struct StaticBatch
    {
        std::uint32_t firstIndex = 0;
        std::uint32_t indexCount = 0;
        std::uint32_t descriptorIndex = 0;
        std::uint32_t modelIndex = 0;
        bool flipNormalY = true;
    };

    struct PaintLayer
    {
        bool allocated = false;
        bool dirty = false;
        bool descriptorDirty = false;
        bool imageInitialized = false;
        std::uint32_t hitCount = 0;
        std::vector<std::uint8_t> pixels;
    };

    struct FlatDecalTemplateGpu
    {
        std::uint32_t descriptorIndex = 0;
        bool flipNormalY = true;
    };

    struct FlatDecalDraw
    {
        std::uint32_t firstIndex = 0;
        std::uint32_t indexCount = 0;
        std::uint32_t descriptorIndex = 0;
        bool flipNormalY = true;
    };

    bool ShadowDrawItemVisible(const DrawItem& drawItem, std::uint32_t cascadeIndex) const;

    std::vector<DrawItem> m_drawItems;
    std::vector<std::uint32_t> m_visibleDrawItems;
    std::vector<StaticBatch> m_staticBatches;
    std::vector<std::vector<std::uint32_t>> m_visibleStaticBatchDrawItems;
    std::vector<StaticInstanceGpu> m_visibleStaticInstances;
    std::vector<std::uint32_t> m_staticBatchFirstInstance;
    std::vector<std::vector<std::uint32_t>> m_shadowVisibleStaticBatchDrawItems;
    std::vector<StaticInstanceGpu> m_shadowVisibleStaticInstances;
    std::vector<std::uint32_t> m_shadowStaticBatchFirstInstance;
    std::vector<DynamicPointLightGpu> m_procCityDynamicLights;
    std::vector<std::uint32_t> m_visibleProcCityDynamicLightIndices;
    std::vector<ProcCityLightTileGpu> m_procCityLightTiles;
    std::vector<std::uint32_t> m_procCityTileLightIndices;
    std::uint32_t m_procCityMaxTileLightCount = 0;
    std::vector<FlatDecalTemplateGpu> m_flatDecalTemplates;
    std::vector<FlatDecalDraw> m_flatDecalDraws;
    std::vector<PaintLayer> m_paintLayers;
    Vec3 m_cameraCullPosition = {};
    float m_mainCullDistance = 160.0f;
    float m_shadowCullDistance = 200.0f;
    CharacterRenderState m_characterState = {};
    std::uint32_t m_characterIndexCount = 0;
    std::uint32_t m_characterDescriptorIndex = std::numeric_limits<std::uint32_t>::max();
    std::uint32_t m_flatDecalVertexCount = 0;
    std::uint32_t m_flatDecalIndexCount = 0;
    std::uint32_t m_sceneVertexCount = 0;
    std::uint32_t m_sceneIndexCount = 0;
    bool m_hasCharacter = false;
    std::uint32_t m_overlayTextureWidth = 0;
    std::uint32_t m_overlayTextureHeight = 0;
    std::uint32_t m_overlayAtlasCount = 0;
    std::uint32_t m_overlayVertexCount = 0;
    std::array<OverlayBatch, text::kMaxAtlases> m_overlayBatches = {};
    std::uint32_t m_overlayBatchCount = 0;
    std::uint32_t m_shadowMapSize = 2048;
    std::uint32_t m_activeShadowMapCount = kSunShadowCascadeCount;
    std::uint32_t m_activeShadowedSpotCount = 0;
    std::uint32_t m_accumulatedPaintHitCount = 0;
    std::uint32_t m_lightLineVertexCount = 0;
    std::uint32_t m_lightSolidVertexCount = 0;
    Vec4 m_clearColor = {0.09f, 0.10f, 0.12f, 1.0f};
    RenderProfilingStats m_profilingStats = {};
    bool m_sunShadowsEnabled = true;
    bool m_localLightShadowsEnabled = true;
    bool m_useProcCityPipeline = false;
    bool m_useProcCityTiledLighting = false;
    float m_debugTileContributionCutoff = 0.06f;
    std::uint32_t m_procCityTiledOccupancyMode = 0;
    bool m_imguiInitialized = false;
    bool m_initialized = false;
};
