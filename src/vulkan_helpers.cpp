#include "vulkan_helpers.h"

#include <fstream>
#include <stdexcept>
#include <string>

void CheckVk(VkResult result, std::string_view context)
{
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error(std::string(context) + " failed with VkResult " + std::to_string(result));
    }
}

std::vector<std::byte> ReadBinaryFile(std::string_view path)
{
    std::ifstream file(std::string(path), std::ios::binary | std::ios::ate);
    if (!file)
    {
        throw std::runtime_error("Failed to open file: " + std::string(path));
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(bytes.data()), size))
    {
        throw std::runtime_error("Failed to read file: " + std::string(path));
    }

    return bytes;
}

std::uint32_t FindMemoryType(
    VkPhysicalDevice physicalDevice,
    std::uint32_t typeBits,
    VkMemoryPropertyFlags properties
)
{
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        bool supported = (typeBits & (1u << i)) != 0;
        bool matches = (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (supported && matches)
        {
            return i;
        }
    }

    throw std::runtime_error("Failed to find compatible Vulkan memory type");
}

BufferResource CreateBuffer(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    bool mapMemory
)
{
    BufferResource resource{};

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CheckVk(vkCreateBuffer(device, &bufferInfo, nullptr, &resource.buffer), "vkCreateBuffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, resource.buffer, &requirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = FindMemoryType(
        physicalDevice,
        requirements.memoryTypeBits,
        properties
    );
    CheckVk(vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory), "vkAllocateMemory(buffer)");
    CheckVk(vkBindBufferMemory(device, resource.buffer, resource.memory, 0), "vkBindBufferMemory");

    if (mapMemory)
    {
        CheckVk(
            vkMapMemory(device, resource.memory, 0, size, 0, &resource.mapped),
            "vkMapMemory(buffer)"
        );
    }

    return resource;
}

void DestroyBuffer(VkDevice device, BufferResource& buffer)
{
    if (buffer.mapped != nullptr)
    {
        vkUnmapMemory(device, buffer.memory);
        buffer.mapped = nullptr;
    }
    if (buffer.buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
    }
    if (buffer.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, buffer.memory, nullptr);
        buffer.memory = VK_NULL_HANDLE;
    }
}

ImageResource CreateImage2D(
    VkPhysicalDevice physicalDevice,
    VkDevice device,
    std::uint32_t width,
    std::uint32_t height,
    VkFormat format,
    VkImageUsageFlags usage,
    VkImageAspectFlags aspectMask
)
{
    ImageResource resource{};

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    CheckVk(vkCreateImage(device, &imageInfo, nullptr, &resource.image), "vkCreateImage");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, resource.image, &requirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = FindMemoryType(
        physicalDevice,
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );
    CheckVk(vkAllocateMemory(device, &allocateInfo, nullptr, &resource.memory), "vkAllocateMemory(image)");
    CheckVk(vkBindImageMemory(device, resource.image, resource.memory, 0), "vkBindImageMemory");

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = resource.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectMask;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    CheckVk(vkCreateImageView(device, &viewInfo, nullptr, &resource.view), "vkCreateImageView");

    return resource;
}

void DestroyImage(VkDevice device, ImageResource& image)
{
    if (image.view != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, image.view, nullptr);
        image.view = VK_NULL_HANDLE;
    }
    if (image.image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, image.image, nullptr);
        image.image = VK_NULL_HANDLE;
    }
    if (image.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, image.memory, nullptr);
        image.memory = VK_NULL_HANDLE;
    }
}

VkCommandBuffer BeginSingleUseCommandBuffer(
    VkDevice device,
    VkCommandPool commandPool
)
{
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    CheckVk(
        vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers(single-use)"
    );

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    CheckVk(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer(single-use)");
    return commandBuffer;
}

void EndSingleUseCommandBuffer(
    VkDevice device,
    VkQueue queue,
    VkCommandPool commandPool,
    VkCommandBuffer commandBuffer
)
{
    CheckVk(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(single-use)");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    CheckVk(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit(single-use)");
    CheckVk(vkQueueWaitIdle(queue), "vkQueueWaitIdle(single-use)");
    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}
