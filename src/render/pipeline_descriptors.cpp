#include "render/internal.h"

#include <array>

using namespace vulkan_renderer_internal;

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

    VkDescriptorSetLayoutBinding shadowBinding{};
    shadowBinding.binding = 2;
    shadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowBinding.descriptorCount = kTotalShadowMaps;
    shadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding paintBinding{};
    paintBinding.binding = 3;
    paintBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    paintBinding.descriptorCount = 1;
    paintBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding effectBinding{};
    effectBinding.binding = 4;
    effectBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    effectBinding.descriptorCount = 1;
    effectBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding normalBinding{};
    normalBinding.binding = 5;
    normalBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    normalBinding.descriptorCount = 1;
    normalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding procCityLightBinding{};
    procCityLightBinding.binding = 6;
    procCityLightBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    procCityLightBinding.descriptorCount = 1;
    procCityLightBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding procCityLightIndexBinding{};
    procCityLightIndexBinding.binding = 7;
    procCityLightIndexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    procCityLightIndexBinding.descriptorCount = 1;
    procCityLightIndexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding procCityTileBinding{};
    procCityTileBinding.binding = 8;
    procCityTileBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    procCityTileBinding.descriptorCount = 1;
    procCityTileBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding procCityTileLightIndexBinding{};
    procCityTileLightIndexBinding.binding = 9;
    procCityTileLightIndexBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    procCityTileLightIndexBinding.descriptorCount = 1;
    procCityTileLightIndexBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::array<VkDescriptorSetLayoutBinding, 10> bindings = {
        uniformBinding,
        samplerBinding,
        shadowBinding,
        paintBinding,
        effectBinding,
        normalBinding,
        procCityLightBinding,
        procCityLightIndexBinding,
        procCityTileBinding,
        procCityTileLightIndexBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    CheckVk(vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_descriptorSetLayout), "vkCreateDescriptorSetLayout");

    std::uint32_t descriptorCount = static_cast<std::uint32_t>(m_textureImages.size());
    std::array<VkDescriptorPoolSize, 10> poolSizes =
        {
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptorCount},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount * kTotalShadowMaps},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, descriptorCount},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount},
            VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, descriptorCount},
        };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = descriptorCount;
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    CheckVk(vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_descriptorPool), "vkCreateDescriptorPool");

    m_descriptorSets.resize(descriptorCount);
    std::vector<VkDescriptorSetLayout> layouts(descriptorCount, m_descriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = descriptorCount;
    allocInfo.pSetLayouts = layouts.data();
    CheckVk(vkAllocateDescriptorSets(m_device, &allocInfo, m_descriptorSets.data()), "vkAllocateDescriptorSets");
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

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, text::kMaxAtlases};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = text::kMaxAtlases;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    CheckVk(
        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_overlayDescriptorPool),
        "vkCreateDescriptorPool(overlay)"
    );

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_overlayDescriptorPool;
    std::array<VkDescriptorSetLayout, text::kMaxAtlases> layouts{};
    layouts.fill(m_overlayDescriptorSetLayout);
    allocInfo.descriptorSetCount = text::kMaxAtlases;
    allocInfo.pSetLayouts = layouts.data();
    CheckVk(
        vkAllocateDescriptorSets(m_device, &allocInfo, m_overlayDescriptorSets.data()),
        "vkAllocateDescriptorSets(overlay)"
    );
}

void VulkanRenderer::CreatePostDescriptorObjects()
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
        vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &m_postDescriptorSetLayout),
        "vkCreateDescriptorSetLayout(post)"
    );

    VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    CheckVk(
        vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_postDescriptorPool),
        "vkCreateDescriptorPool(post)"
    );

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_postDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_postDescriptorSetLayout;
    CheckVk(
        vkAllocateDescriptorSets(m_device, &allocInfo, &m_postDescriptorSet),
        "vkAllocateDescriptorSets(post)"
    );
}
