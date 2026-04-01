#include "engine/renderer/vulkan/PbrRenderer.hpp"

#include "engine/core/Log.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"

#include <array>
#include <cstring>
#include <fstream>
#include <vector>

namespace engine {

namespace {

std::vector<char> readFile(const char* path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        return {};
    }
    const auto size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(size));
    return buffer;
}

VkShaderModule createShaderModule(VkDevice device, const std::vector<char>& code) {
    VkShaderModuleCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &ci, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

} // namespace

bool PbrRenderer::init(VulkanContext& ctx) {
    m_ctx = &ctx;
    if (!loadShaderModules()) {
        log(LogLevel::Warn, "PbrRenderer: SPIR-V shaders not found; PBR mesh pass disabled");
        return true;
    }
    if (!createPipelineLayout()) {
        return false;
    }
    if (!createGraphicsPipeline()) {
        return false;
    }
    m_iblReady = true;
    return true;
}

void PbrRenderer::shutdown() {
    if (!m_ctx) {
        return;
    }
    VkDevice device = m_ctx->device();
    destroyPipeline();
    if (m_vertModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_vertModule, nullptr);
        m_vertModule = VK_NULL_HANDLE;
    }
    if (m_fragModule != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_fragModule, nullptr);
        m_fragModule = VK_NULL_HANDLE;
    }
    m_ctx = nullptr;
}

bool PbrRenderer::loadShaderModules() {
    auto vert = readFile(SPV_PBR_VERT_PATH);
    auto frag = readFile(SPV_PBR_FRAG_PATH);
    if (vert.empty() || frag.empty()) {
        return false;
    }
    m_vertModule = createShaderModule(m_ctx->device(), vert);
    m_fragModule = createShaderModule(m_ctx->device(), frag);
    return m_vertModule != VK_NULL_HANDLE && m_fragModule != VK_NULL_HANDLE;
}

bool PbrRenderer::createPipelineLayout() {
    VkPipelineLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    if (vkCreatePipelineLayout(m_ctx->device(), &ci, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        log(LogLevel::Error, "PbrRenderer: vkCreatePipelineLayout failed");
        return false;
    }
    return true;
}

bool PbrRenderer::createGraphicsPipeline() {
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = m_vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = m_fragModule;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.lineWidth = 1.f;
    raster.cullMode = VK_CULL_MODE_BACK_BIT;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo blendState{};
    blendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendState.attachmentCount = 1;
    blendState.pAttachments = &blend;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &msaa;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &blendState;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = m_ctx->renderPass();
    pipelineInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(m_ctx->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                  &m_graphicsPipeline) != VK_SUCCESS) {
        log(LogLevel::Error, "PbrRenderer: vkCreateGraphicsPipelines failed");
        return false;
    }
    return true;
}

void PbrRenderer::destroyPipeline() {
    if (!m_ctx) {
        return;
    }
    VkDevice device = m_ctx->device();
    if (m_graphicsPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_graphicsPipeline, nullptr);
        m_graphicsPipeline = VK_NULL_HANDLE;
    }
    if (m_pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
}

void PbrRenderer::recordMainPass(VkCommandBuffer cmd, uint32_t swapchainImageIndex, float r, float g,
                                 float b) {
    (void)swapchainImageIndex;
    (void)r;
    (void)g;
    (void)b;
    if (!m_ctx || m_graphicsPipeline == VK_NULL_HANDLE) {
        return;
    }
    const VkExtent2D extent = m_ctx->swapchainExtent();
    VkViewport viewport{};
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.maxDepth = 1.f;
    VkRect2D scissor{};
    scissor.extent = extent;
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

} // namespace engine
