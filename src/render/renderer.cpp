#include "render/renderer.h"
#include "render/culling.h"
#include "render/internal.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "backends/imgui_impl_vulkan.h"

#include <cmath>

namespace
{
constexpr float kPi = 3.1415926535f;

bool SpheresOverlap(Vec3 aCenter, float aRadius, Vec3 bCenter, float bRadius)
{
    Vec3 delta = Vec3Sub(aCenter, bCenter);
    float combined = aRadius + bRadius;
    return Vec3Dot(delta, delta) <= combined * combined;
}

void AppendLine(
    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightLineVertexCount>& lines,
    std::uint32_t& count,
    Vec3 a,
    Vec3 b,
    Vec3 color)
{
    if (count + 1 >= vulkan_renderer_internal::kLightLineVertexCount)
    {
        return;
    }
    lines[count++] = LightMarkerVertex{a, color};
    lines[count++] = LightMarkerVertex{b, color};
}

void AppendCircle(
    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightLineVertexCount>& lines,
    std::uint32_t& count,
    Vec3 center,
    Vec3 axisA,
    Vec3 axisB,
    float radius,
    Vec3 color,
    int segments)
{
    for (int i = 0; i < segments; ++i)
    {
        float t0 = static_cast<float>(i) / static_cast<float>(segments);
        float t1 = static_cast<float>(i + 1) / static_cast<float>(segments);
        float a0 = t0 * 2.0f * kPi;
        float a1 = t1 * 2.0f * kPi;
        Vec3 p0 = Vec3Add(center, Vec3Add(Vec3Scale(axisA, std::cos(a0) * radius), Vec3Scale(axisB, std::sin(a0) * radius)));
        Vec3 p1 = Vec3Add(center, Vec3Add(Vec3Scale(axisA, std::cos(a1) * radius), Vec3Scale(axisB, std::sin(a1) * radius)));
        AppendLine(lines, count, p0, p1, color);
    }
}

void AppendSphere(
    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightLineVertexCount>& lines,
    std::uint32_t& count,
    Vec3 center,
    float radius,
    Vec3 color)
{
    constexpr int kSegments = 24;
    AppendCircle(lines, count, center, Vec3Make(1.0f, 0.0f, 0.0f), Vec3Make(0.0f, 1.0f, 0.0f), radius, color, kSegments);
    AppendCircle(lines, count, center, Vec3Make(1.0f, 0.0f, 0.0f), Vec3Make(0.0f, 0.0f, 1.0f), radius, color, kSegments);
    AppendCircle(lines, count, center, Vec3Make(0.0f, 1.0f, 0.0f), Vec3Make(0.0f, 0.0f, 1.0f), radius, color, kSegments);
}

void AppendCone(
    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightLineVertexCount>& lines,
    std::uint32_t& count,
    Vec3 apex,
    Vec3 direction,
    float range,
    float outerCos,
    Vec3 color)
{
    constexpr int kSegments = 20;
    Vec3 dir = Vec3Normalize(direction);
    if (Vec3Length(dir) <= 0.0001f || range <= 0.0001f)
    {
        return;
    }

    float outerAngle = std::acos(std::clamp(outerCos, -1.0f, 1.0f));
    float baseRadius = std::tan(outerAngle) * range;
    Vec3 center = Vec3Add(apex, Vec3Scale(dir, range));
    Vec3 up = std::fabs(dir.y) < 0.95f ? Vec3Make(0.0f, 1.0f, 0.0f) : Vec3Make(1.0f, 0.0f, 0.0f);
    Vec3 right = Vec3Normalize(Vec3Cross(dir, up));
    Vec3 forward = Vec3Normalize(Vec3Cross(right, dir));

    AppendCircle(lines, count, center, right, forward, baseRadius, color, kSegments);
    AppendLine(lines, count, apex, center, color);
    constexpr std::array<float, 4> kAngles = {0.0f, 0.5f * kPi, kPi, 1.5f * kPi};
    for (float angle : kAngles)
    {
        Vec3 rim = Vec3Add(center, Vec3Add(Vec3Scale(right, std::cos(angle) * baseRadius), Vec3Scale(forward, std::sin(angle) * baseRadius)));
        AppendLine(lines, count, apex, rim, color);
    }
}

void AppendCylinder(
    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightLineVertexCount>& lines,
    std::uint32_t& count,
    Vec3 baseCenter,
    float radius,
    float height,
    Vec3 color)
{
    constexpr int kSegments = 24;
    Vec3 topCenter = Vec3Add(baseCenter, Vec3Make(0.0f, height, 0.0f));
    AppendCircle(lines, count, baseCenter, Vec3Make(1.0f, 0.0f, 0.0f), Vec3Make(0.0f, 0.0f, 1.0f), radius, color, kSegments);
    AppendCircle(lines, count, topCenter, Vec3Make(1.0f, 0.0f, 0.0f), Vec3Make(0.0f, 0.0f, 1.0f), radius, color, kSegments);
    constexpr std::array<float, 4> kAngles = {0.0f, 0.5f * kPi, kPi, 1.5f * kPi};
    for (float angle : kAngles)
    {
        Vec3 offset = Vec3Make(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
        AppendLine(lines, count, Vec3Add(baseCenter, offset), Vec3Add(topCenter, offset), color);
    }
}

void AppendTriangle(
    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightSolidVertexCount>& solids,
    std::uint32_t& count,
    Vec3 a,
    Vec3 b,
    Vec3 c,
    Vec3 color)
{
    if (count + 2 >= vulkan_renderer_internal::kLightSolidVertexCount)
    {
        return;
    }
    solids[count++] = LightMarkerVertex{a, color};
    solids[count++] = LightMarkerVertex{b, color};
    solids[count++] = LightMarkerVertex{c, color};
}

void AppendCube(
    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightSolidVertexCount>& solids,
    std::uint32_t& count,
    Vec3 center,
    float halfExtent,
    Vec3 color)
{
    Vec3 p000 = Vec3Add(center, Vec3Make(-halfExtent, -halfExtent, -halfExtent));
    Vec3 p001 = Vec3Add(center, Vec3Make(-halfExtent, -halfExtent, halfExtent));
    Vec3 p010 = Vec3Add(center, Vec3Make(-halfExtent, halfExtent, -halfExtent));
    Vec3 p011 = Vec3Add(center, Vec3Make(-halfExtent, halfExtent, halfExtent));
    Vec3 p100 = Vec3Add(center, Vec3Make(halfExtent, -halfExtent, -halfExtent));
    Vec3 p101 = Vec3Add(center, Vec3Make(halfExtent, -halfExtent, halfExtent));
    Vec3 p110 = Vec3Add(center, Vec3Make(halfExtent, halfExtent, -halfExtent));
    Vec3 p111 = Vec3Add(center, Vec3Make(halfExtent, halfExtent, halfExtent));

    AppendTriangle(solids, count, p001, p101, p111, color);
    AppendTriangle(solids, count, p001, p111, p011, color);
    AppendTriangle(solids, count, p100, p000, p010, color);
    AppendTriangle(solids, count, p100, p010, p110, color);
    AppendTriangle(solids, count, p000, p001, p011, color);
    AppendTriangle(solids, count, p000, p011, p010, color);
    AppendTriangle(solids, count, p101, p100, p110, color);
    AppendTriangle(solids, count, p101, p110, p111, color);
    AppendTriangle(solids, count, p010, p011, p111, color);
    AppendTriangle(solids, count, p010, p111, p110, color);
    AppendTriangle(solids, count, p000, p100, p101, color);
    AppendTriangle(solids, count, p000, p101, p001, color);
}
}

VulkanRenderer::~VulkanRenderer()
{
    Shutdown();
}

void VulkanRenderer::UpdateMainPassVisibility(const SceneUniforms& uniforms)
{
    m_visibleDrawItems.clear();
    if (m_drawItems.empty())
    {
        return;
    }

    Frustum frustum = ExtractFrustum(Mat4Mul(uniforms.proj, uniforms.view));
    m_cameraCullPosition = Vec3Make(
        uniforms.cameraPosition.x,
        uniforms.cameraPosition.y,
        uniforms.cameraPosition.z
    );
    for (std::uint32_t drawIndex = 0; drawIndex < m_drawItems.size(); ++drawIndex)
    {
        const DrawItem& drawItem = m_drawItems[drawIndex];
        Vec3 worldCenter = TransformPoint(drawItem.model, drawItem.localBoundsCenter);
        float worldRadius = drawItem.localBoundsRadius * MaxAxisScale(drawItem.model);
        Vec3 delta = Vec3Sub(worldCenter, m_cameraCullPosition);
        if (Vec3Dot(delta, delta) > (m_mainCullDistance + worldRadius) * (m_mainCullDistance + worldRadius))
        {
            continue;
        }
        if (SphereIntersectsFrustum(frustum, worldCenter, worldRadius))
        {
            m_visibleDrawItems.push_back(drawIndex);
        }
    }
}

void VulkanRenderer::UpdateDrawLightMasks(const SceneUniforms& uniforms)
{
    for (DrawItem& drawItem : m_drawItems)
    {
        drawItem.pointLightMask = 0;
        drawItem.spotLightMask = 0;
        drawItem.shadowedSpotLightMask = 0;

        Vec3 worldCenter = TransformPoint(drawItem.model, drawItem.localBoundsCenter);
        float worldRadius = drawItem.localBoundsRadius * MaxAxisScale(drawItem.model);
        if (worldRadius <= 0.0001f)
        {
            continue;
        }

        for (std::uint32_t i = 0; i < 4; ++i)
        {
            if (uniforms.lightPositions[i].w <= 0.0001f || uniforms.lightPositions[i].y < -999.0f)
            {
                continue;
            }
            Vec3 lightCenter = Vec3Make(uniforms.lightPositions[i].x, uniforms.lightPositions[i].y, uniforms.lightPositions[i].z);
            if (SpheresOverlap(worldCenter, worldRadius, lightCenter, uniforms.lightPositions[i].w))
            {
                drawItem.pointLightMask |= (1u << i);
            }
        }

        std::uint32_t spotCount = std::min<std::uint32_t>(static_cast<std::uint32_t>(uniforms.sceneLightCounts.x), kMaxSceneSpotLights);
        for (std::uint32_t i = 0; i < spotCount; ++i)
        {
            if (uniforms.spotLightPositions[i].w <= 0.0001f)
            {
                continue;
            }
            Vec3 lightCenter = Vec3Make(uniforms.spotLightPositions[i].x, uniforms.spotLightPositions[i].y, uniforms.spotLightPositions[i].z);
            if (SpheresOverlap(worldCenter, worldRadius, lightCenter, uniforms.spotLightPositions[i].w))
            {
                drawItem.spotLightMask |= (1u << i);
            }
        }

        std::uint32_t shadowedCount =
            std::min<std::uint32_t>(static_cast<std::uint32_t>(uniforms.sceneLightCounts.y), kMaxShadowedSpotLights);
        for (std::uint32_t i = 0; i < shadowedCount; ++i)
        {
            if (uniforms.shadowedSpotLightPositions[i].w <= 0.0001f)
            {
                continue;
            }
            Vec3 lightCenter = Vec3Make(
                uniforms.shadowedSpotLightPositions[i].x,
                uniforms.shadowedSpotLightPositions[i].y,
                uniforms.shadowedSpotLightPositions[i].z
            );
            if (SpheresOverlap(worldCenter, worldRadius, lightCenter, uniforms.shadowedSpotLightPositions[i].w))
            {
                drawItem.shadowedSpotLightMask |= (1u << i);
            }
        }
    }
}

bool VulkanRenderer::ShadowDrawItemVisible(const DrawItem& drawItem, std::uint32_t cascadeIndex) const
{
    Vec3 worldCenter = TransformPoint(drawItem.model, drawItem.localBoundsCenter);
    float worldRadius = drawItem.localBoundsRadius * MaxAxisScale(drawItem.model);
    Vec3 delta = Vec3Sub(worldCenter, m_cameraCullPosition);
    float distance2 = Vec3Dot(delta, delta);
    float maxDistance = m_shadowCullDistance;
    if (cascadeIndex == 0)
    {
        maxDistance *= 0.75f;
    }
    return distance2 <= (maxDistance + worldRadius) * (maxDistance + worldRadius);
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

void VulkanRenderer::Render(
    const SceneUniforms& uniforms,
    std::span<const std::uint32_t> overlayPixels,
    std::uint32_t overlayWidth,
    std::uint32_t overlayHeight,
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

    std::memcpy(m_uniformBuffer.mapped, &uniforms, sizeof(uniforms));
    std::uint32_t activeShadowedSpots =
        std::min<std::uint32_t>(static_cast<std::uint32_t>(uniforms.sceneLightCounts.y), kMaxShadowedSpotLights);
    m_activeShadowMapCount = kSunShadowCascadeCount + activeShadowedSpots;
    DebugRenderOptions debug = debugOptions != nullptr ? *debugOptions : DebugRenderOptions{};
    m_mainCullDistance = std::max(debug.mainDrawDistance, 1.0f);
    m_shadowCullDistance = std::max(debug.shadowDrawDistance, 1.0f);
    UpdateMainPassVisibility(uniforms);
    UpdateDrawLightMasks(uniforms);
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
    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightLineVertexCount> lightLines{};
    std::array<LightMarkerVertex, vulkan_renderer_internal::kLightSolidVertexCount> lightSolids{};
    m_lightLineVertexCount = 0;
    m_lightSolidVertexCount = 0;
    for (std::size_t i = 0; i < 4; ++i)
    {
        const bool showProxy = debug.drawLightProxies && uniforms.lightPositions[i].w > 0.0001f && uniforms.lightPositions[i].y > -999.0f;
        lightMarkers[i].position = debug.drawLightMarkers ? Vec3Make(
            uniforms.lightPositions[i].x,
            uniforms.lightPositions[i].y,
            uniforms.lightPositions[i].z
        ) : Vec3Make(0.0f, -10000.0f, 0.0f);
        lightMarkers[i].color = Vec3Make(
            uniforms.lightColors[i].x,
            uniforms.lightColors[i].y,
            uniforms.lightColors[i].z
        );
        if (debug.drawLightVolumes && uniforms.lightPositions[i].w > 0.0001f && uniforms.lightPositions[i].y > -999.0f)
        {
            AppendSphere(
                lightLines,
                m_lightLineVertexCount,
                Vec3Make(uniforms.lightPositions[i].x, uniforms.lightPositions[i].y, uniforms.lightPositions[i].z),
                uniforms.lightPositions[i].w,
                lightMarkers[i].color
            );
        }
        if (showProxy)
        {
            AppendCube(
                lightSolids,
                m_lightSolidVertexCount,
                Vec3Make(uniforms.lightPositions[i].x, uniforms.lightPositions[i].y, uniforms.lightPositions[i].z),
                0.10f,
                lightMarkers[i].color
            );
        }
    }
    for (std::size_t i = 0; i < 2; ++i)
    {
        lightMarkers[4 + i].position = debug.drawLightMarkers ? Vec3Make(
            uniforms.celestialPositions[i].x,
            uniforms.celestialPositions[i].y,
            uniforms.celestialPositions[i].z
        ) : Vec3Make(0.0f, -10000.0f, 0.0f);
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
            Vec3 source = Vec3Make(
                uniforms.spotLightPositions[i].x,
                uniforms.spotLightPositions[i].y,
                uniforms.spotLightPositions[i].z
            );
            Vec3 color = Vec3Make(
                uniforms.spotLightColors[i].x,
                uniforms.spotLightColors[i].y,
                uniforms.spotLightColors[i].z
            );
            lightMarkers[markerIndex].position = debug.drawLightMarkers ? source : Vec3Make(0.0f, -10000.0f, 0.0f);
            lightMarkers[markerIndex].color = color;
            if (debug.drawLightProxies)
            {
                AppendCube(lightSolids, m_lightSolidVertexCount, source, 0.08f, color);
            }
            if (debug.drawLightDirections && m_lightLineVertexCount + 1 < vulkan_renderer_internal::kLightLineVertexCount)
            {
                Vec3 dir = Vec3Make(
                    uniforms.spotLightDirections[i].x,
                    uniforms.spotLightDirections[i].y,
                    uniforms.spotLightDirections[i].z
                );
                float range = std::min(3.0f, uniforms.spotLightParams[i].z * 0.35f);
                lightLines[m_lightLineVertexCount++] = LightMarkerVertex{source, color};
                lightLines[m_lightLineVertexCount++] =
                    LightMarkerVertex{Vec3Add(source, Vec3Scale(dir, range)), color};
            }
            if (debug.drawLightVolumes)
            {
                AppendCone(
                    lightLines,
                    m_lightLineVertexCount,
                    source,
                    Vec3Make(
                        uniforms.spotLightDirections[i].x,
                        uniforms.spotLightDirections[i].y,
                        uniforms.spotLightDirections[i].z
                    ),
                    uniforms.spotLightPositions[i].w,
                    uniforms.spotLightParams[i].y,
                    color
                );
            }
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
            Vec3 source = Vec3Make(
                uniforms.shadowedSpotLightPositions[i].x,
                uniforms.shadowedSpotLightPositions[i].y,
                uniforms.shadowedSpotLightPositions[i].z
            );
            Vec3 color = Vec3Make(
                uniforms.shadowedSpotLightColors[i].x,
                uniforms.shadowedSpotLightColors[i].y,
                uniforms.shadowedSpotLightColors[i].z
            );
            lightMarkers[markerIndex].position = debug.drawLightMarkers ? source : Vec3Make(0.0f, -10000.0f, 0.0f);
            lightMarkers[markerIndex].color = color;
            if (debug.drawLightProxies)
            {
                AppendCube(lightSolids, m_lightSolidVertexCount, source, 0.08f, color);
            }
            if (debug.drawLightDirections && m_lightLineVertexCount + 1 < vulkan_renderer_internal::kLightLineVertexCount)
            {
                Vec3 dir = Vec3Make(
                    uniforms.shadowedSpotLightDirections[i].x,
                    uniforms.shadowedSpotLightDirections[i].y,
                    uniforms.shadowedSpotLightDirections[i].z
                );
                float range = std::min(3.0f, uniforms.shadowedSpotLightParams[i].z * 0.35f);
                lightLines[m_lightLineVertexCount++] = LightMarkerVertex{source, color};
                lightLines[m_lightLineVertexCount++] =
                    LightMarkerVertex{Vec3Add(source, Vec3Scale(dir, range)), color};
            }
            if (debug.drawLightVolumes)
            {
                AppendCone(
                    lightLines,
                    m_lightLineVertexCount,
                    source,
                    Vec3Make(
                        uniforms.shadowedSpotLightDirections[i].x,
                        uniforms.shadowedSpotLightDirections[i].y,
                        uniforms.shadowedSpotLightDirections[i].z
                    ),
                    uniforms.shadowedSpotLightPositions[i].w,
                    uniforms.shadowedSpotLightParams[i].y,
                    color
                );
            }
        }
        else
        {
            lightMarkers[markerIndex].position = Vec3Make(0.0f, -10000.0f, 0.0f);
            lightMarkers[markerIndex].color = Vec3Make(0.0f, 0.0f, 0.0f);
        }
    }
    if (debug.drawActivationVolumes)
    {
        if (debug.activationVolumeA.w > 0.0001f)
        {
            AppendCylinder(
                lightLines,
                m_lightLineVertexCount,
                Vec3Make(debug.activationVolumeA.x, debug.activationVolumeA.y, debug.activationVolumeA.z),
                debug.activationVolumeA.w,
                18.0f,
                Vec3Make(0.0f, 1.0f, 1.0f)
            );
        }
        if (debug.activationVolumeB.w > 0.0001f)
        {
            AppendCylinder(
                lightLines,
                m_lightLineVertexCount,
                Vec3Make(debug.activationVolumeB.x, debug.activationVolumeB.y, debug.activationVolumeB.z),
                debug.activationVolumeB.w,
                18.0f,
                Vec3Make(1.0f, 0.35f, 1.0f)
            );
        }
    }
    for (std::uint32_t i = 0; i < debug.selectionSphereCount && i < DebugRenderOptions::kMaxSelectionSpheres; ++i)
    {
        Vec4 sphere = debug.selectionSpheres[i];
        Vec4 color = debug.selectionSphereColors[i];
        if (sphere.w > 0.0001f)
        {
            AppendSphere(
                lightLines,
                m_lightLineVertexCount,
                Vec3Make(sphere.x, sphere.y, sphere.z),
                sphere.w,
                Vec3Make(color.x, color.y, color.z)
            );
        }
    }
    for (std::uint32_t i = 0; i < debug.customCubeCount && i < DebugRenderOptions::kMaxCustomCubes; ++i)
    {
        Vec4 cube = debug.customCubes[i];
        Vec4 color = debug.customCubeColors[i];
        if (cube.w > 0.0001f)
        {
            AppendCube(
                lightSolids,
                m_lightSolidVertexCount,
                Vec3Make(cube.x, cube.y, cube.z),
                cube.w,
                Vec3Make(color.x, color.y, color.z)
            );
        }
    }
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
