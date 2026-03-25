#include "render/internal.h"

#include <array>

#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"

namespace
{
void CheckImGuiVkResult(VkResult result)
{
    CheckVk(result, "imgui_vulkan");
}
}

void VulkanRenderer::InitializeImGuiBackend()
{
    if (m_imguiInitialized)
    {
        return;
    }

    std::array<VkDescriptorPoolSize, 11> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_SAMPLER, 128},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 128},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 128},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 128},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 128},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 128},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 128},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 128},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 128},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 128},
    }};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 128;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    CheckVk(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_imguiDescriptorPool), "vkCreateDescriptorPool(imgui)");

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.ApiVersion = VK_API_VERSION_1_3;
    initInfo.Instance = m_instance;
    initInfo.PhysicalDevice = m_physicalDevice;
    initInfo.Device = m_device;
    initInfo.QueueFamily = m_queueFamilyIndex;
    initInfo.Queue = m_graphicsQueue;
    initInfo.DescriptorPool = m_imguiDescriptorPool;
    initInfo.MinImageCount = static_cast<std::uint32_t>(m_swapchainImages.size());
    initInfo.ImageCount = static_cast<std::uint32_t>(m_swapchainImages.size());
    initInfo.PipelineInfoMain.RenderPass = m_renderPass;
    initInfo.PipelineInfoMain.Subpass = 0;
    initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.CheckVkResultFn = CheckImGuiVkResult;
    ImGui_ImplVulkan_Init(&initInfo);

    for (std::size_t i = 0; i < m_imguiShadowDescriptors.size(); ++i)
    {
        m_imguiShadowDescriptors[i] = ImGui_ImplVulkan_AddTexture(
            m_shadowSampler,
            m_shadowImages[i].view,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        );
    }

    m_imguiInitialized = true;
}

ImTextureID VulkanRenderer::GetShadowDebugTexture(std::uint32_t cascadeIndex) const
{
    if (cascadeIndex >= m_imguiShadowDescriptors.size())
    {
        return {};
    }
    return static_cast<ImTextureID>(reinterpret_cast<std::uintptr_t>(m_imguiShadowDescriptors[cascadeIndex]));
}

void VulkanRenderer::ShutdownImGuiBackend()
{
    if (!m_imguiInitialized)
    {
        return;
    }

    vkDeviceWaitIdle(m_device);
    for (VkDescriptorSet& descriptor : m_imguiShadowDescriptors)
    {
        if (descriptor != VK_NULL_HANDLE)
        {
            ImGui_ImplVulkan_RemoveTexture(descriptor);
            descriptor = VK_NULL_HANDLE;
        }
    }
    ImGui_ImplVulkan_Shutdown();
    if (m_imguiDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(m_device, m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }
    m_imguiInitialized = false;
}
