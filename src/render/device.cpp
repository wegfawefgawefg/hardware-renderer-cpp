#include "render/internal.h"

#include <SDL3/SDL_vulkan.h>

#include <array>
#include <stdexcept>
#include <vector>

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

    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
    m_gpuTimestampPeriodNs = properties.limits.timestampPeriod;

    VkQueryPoolCreateInfo queryInfo{};
    queryInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    queryInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    queryInfo.queryCount = kGpuTimestampCount;
    CheckVk(vkCreateQueryPool(m_device, &queryInfo, nullptr, &m_timestampQueryPool), "vkCreateQueryPool");
}
