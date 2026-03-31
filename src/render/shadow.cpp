#include "render/internal.h"

#include <array>
#include <cstddef>
#include <cstring>

using namespace vulkan_renderer_internal;

namespace
{
bool ModelHidden(const VulkanRenderer& renderer, std::uint32_t modelIndex)
{
    return modelIndex == renderer.m_hiddenModelIndex || modelIndex == renderer.m_hiddenModelIndexSecondary;
}
}

void VulkanRenderer::CreateShadowRenderPass()
{
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = FindDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depthRef;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &depthAttachment;
    createInfo.subpassCount = 1;
    createInfo.pSubpasses = &subpass;
    createInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
    createInfo.pDependencies = dependencies.data();
    CheckVk(vkCreateRenderPass(m_device, &createInfo, nullptr, &m_shadowRenderPass), "vkCreateRenderPass(shadow)");
}

void VulkanRenderer::CreateShadowPipeline()
{
    m_shadowVertShaderModule = CreateShaderModule(m_device, kShadowVertShaderPath);

    VkPipelineShaderStageCreateInfo vertStage{};
    vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertStage.module = m_shadowVertShaderModule;
    vertStage.pName = "main";

    std::array<VkVertexInputBindingDescription, 2> bindingDescs{};
    bindingDescs[0].binding = 0;
    bindingDescs[0].stride = sizeof(Vertex);
    bindingDescs[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    bindingDescs[1].binding = 1;
    bindingDescs[1].stride = sizeof(StaticInstanceGpu);
    bindingDescs[1].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;

    std::array<VkVertexInputAttributeDescription, 10> attributes{};
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
    attributes[3].location = 3;
    attributes[3].binding = 0;
    attributes[3].format = VK_FORMAT_R16G16B16A16_UINT;
    attributes[3].offset = offsetof(Vertex, jointIndices);
    attributes[4].location = 4;
    attributes[4].binding = 0;
    attributes[4].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributes[4].offset = offsetof(Vertex, jointWeights);
    attributes[5].location = 5;
    attributes[5].binding = 1;
    attributes[5].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributes[5].offset = offsetof(StaticInstanceGpu, model) + sizeof(Vec4) * 0;
    attributes[6].location = 6;
    attributes[6].binding = 1;
    attributes[6].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributes[6].offset = offsetof(StaticInstanceGpu, model) + sizeof(Vec4) * 1;
    attributes[7].location = 7;
    attributes[7].binding = 1;
    attributes[7].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributes[7].offset = offsetof(StaticInstanceGpu, model) + sizeof(Vec4) * 2;
    attributes[8].location = 8;
    attributes[8].binding = 1;
    attributes[8].format = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributes[8].offset = offsetof(StaticInstanceGpu, model) + sizeof(Vec4) * 3;
    attributes[9].location = 9;
    attributes[9].binding = 1;
    attributes[9].format = VK_FORMAT_R32G32B32A32_UINT;
    attributes[9].offset = offsetof(StaticInstanceGpu, pointLightMask);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = static_cast<std::uint32_t>(bindingDescs.size());
    vertexInput.pVertexBindingDescriptions = bindingDescs.data();
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    std::array<VkDynamicState, 2> dynamicStates = {
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
    rasterization.depthBiasEnable = VK_TRUE;
    rasterization.depthBiasConstantFactor = 0.9f;
    rasterization.depthBiasSlopeFactor = 1.35f;
    rasterization.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &m_descriptorSetLayout;
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(DrawPushConstants);
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushConstantRange;

    CheckVk(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_shadowPipelineLayout), "vkCreatePipelineLayout(shadow)");

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 1;
    pipelineInfo.pStages = &vertStage;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterization;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_shadowPipelineLayout;
    pipelineInfo.renderPass = m_shadowRenderPass;
    pipelineInfo.subpass = 0;
    CheckVk(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_shadowPipeline), "vkCreateGraphicsPipelines(shadow)");
}

void VulkanRenderer::RecordShadowPass(VkCommandBuffer commandBuffer, std::uint32_t cascadeIndex)
{
    VkViewport viewport{};
    viewport.width = static_cast<float>(m_shadowMapSize);
    viewport.height = static_cast<float>(m_shadowMapSize);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = {m_shadowMapSize, m_shadowMapSize};
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    VkClearValue clearValue{};
    clearValue.depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo passInfo{};
    passInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    passInfo.renderPass = m_shadowRenderPass;
    passInfo.framebuffer = m_shadowFramebuffers[cascadeIndex];
    passInfo.renderArea.extent = {m_shadowMapSize, m_shadowMapSize};
    passInfo.clearValueCount = 1;
    passInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(commandBuffer, &passInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);

    VkBuffer vertexBuffers[] = {m_vertexBuffer.buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    BuildShadowVisibleStaticInstances(cascadeIndex);
    if (!m_shadowVisibleStaticInstances.empty())
    {
        VkBuffer instancedBuffers[] = {m_vertexBuffer.buffer, m_shadowStaticInstanceBuffer.buffer};
        VkDeviceSize instanceOffsets[] = {0, 0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, instancedBuffers, instanceOffsets);
        for (std::size_t batchIndex = 0; batchIndex < m_staticBatches.size(); ++batchIndex)
        {
            const std::vector<std::uint32_t>& visibleItems = m_shadowVisibleStaticBatchDrawItems[batchIndex];
            if (visibleItems.empty())
            {
                continue;
            }
            const StaticBatch& batch = m_staticBatches[batchIndex];
            if (ModelHidden(*this, batch.modelIndex))
            {
                continue;
            }
            DrawPushConstants pushConstants{};
            pushConstants.model = Mat4Identity();
            pushConstants.shadowCascade = cascadeIndex;
            vkCmdPushConstants(commandBuffer, m_shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);
            vkCmdBindDescriptorSets(
                commandBuffer,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                m_shadowPipelineLayout,
                0,
                1,
                &m_descriptorSets[batch.descriptorIndex],
                0,
                nullptr
            );
            vkCmdDrawIndexed(
                commandBuffer,
                batch.indexCount,
                static_cast<std::uint32_t>(visibleItems.size()),
                batch.firstIndex,
                0,
                m_shadowStaticBatchFirstInstance[batchIndex]);
        }
    }

    VkBuffer singleDrawBuffers[] = {m_vertexBuffer.buffer, m_nullInstanceBuffer.buffer};
    VkDeviceSize singleDrawOffsets[] = {0, 0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 2, singleDrawBuffers, singleDrawOffsets);

    for (std::uint32_t drawIndex = 0; drawIndex < m_drawItems.size(); ++drawIndex)
    {
        const DrawItem& drawItem = m_drawItems[drawIndex];
        if (ModelHidden(*this, drawItem.modelIndex))
        {
            continue;
        }
        if (!drawItem.castsShadows)
        {
            continue;
        }
        bool hasUniquePaint = drawIndex < m_paintLayers.size() && m_paintLayers[drawIndex].allocated;
        if (drawItem.batchedStatic && !hasUniquePaint)
        {
            continue;
        }
        if (!ShadowDrawItemVisible(drawItem, cascadeIndex))
        {
            continue;
        }
        DrawPushConstants pushConstants{};
        pushConstants.model = drawItem.model;
        pushConstants.skinned = drawItem.skinned;
        pushConstants.shadowCascade = cascadeIndex;
        vkCmdPushConstants(commandBuffer, m_shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_shadowPipelineLayout,
            0,
            1,
            &m_descriptorSets[drawItem.descriptorIndex],
            0,
            nullptr
        );
        vkCmdDrawIndexed(commandBuffer, drawItem.indexCount, 1, drawItem.firstIndex, 0, 0);
    }

    if (m_hasCharacter && m_characterState.visible && m_characterIndexCount > 0)
    {
        DrawPushConstants pushConstants{};
        pushConstants.model = m_characterState.model;
        pushConstants.skinned = 1;
        pushConstants.shadowCascade = cascadeIndex;
        VkBuffer characterBuffers[] = {m_characterVertexBuffer.buffer, m_nullInstanceBuffer.buffer};
        VkDeviceSize characterOffsets[] = {0, 0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 2, characterBuffers, characterOffsets);
        vkCmdBindIndexBuffer(commandBuffer, m_characterIndexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdPushConstants(commandBuffer, m_shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(pushConstants), &pushConstants);
        vkCmdBindDescriptorSets(
            commandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_shadowPipelineLayout,
            0,
            1,
            &m_descriptorSets[m_characterDescriptorIndex],
            0,
            nullptr
        );
        vkCmdDrawIndexed(commandBuffer, m_characterIndexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(commandBuffer);
}
