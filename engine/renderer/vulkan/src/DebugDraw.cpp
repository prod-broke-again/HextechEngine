#include "engine/renderer/vulkan/DebugDraw.hpp"

#include "engine/assets/MeshData.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"

#include <glm/gtc/matrix_transform.hpp>
#include <vk_mem_alloc.h>

#include <cstring>
#include <fstream>
#include <vector>

namespace engine {

namespace {

struct LinePush {
    glm::mat4 mvp{1.f};
};

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
    vkCreateShaderModule(device, &ci, nullptr, &module);
    return module;
}

void appendAxis(std::vector<MeshVertex>& lines, const glm::vec3& origin, float length) {
    const auto seg = [&](const glm::vec3& end, const glm::vec3& color) {
        lines.push_back({origin, {0.f, 1.f, 0.f}, color});
        lines.push_back({end, {0.f, 1.f, 0.f}, color});
    };
    seg(origin + glm::vec3(length, 0.f, 0.f), {1.f, 0.25f, 0.25f});
    seg(origin + glm::vec3(0.f, length, 0.f), {0.25f, 1.f, 0.25f});
    seg(origin + glm::vec3(0.f, 0.f, length), {0.25f, 0.5f, 1.f});
}

} // namespace

bool DebugDraw::init(VulkanContext& ctx) {
    m_ctx = &ctx;
    return createPipeline();
}

void DebugDraw::shutdown() {
    if (!m_ctx) {
        return;
    }
    VkDevice device = m_ctx->device();
    if (m_pipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }
    if (m_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_layout, nullptr);
        m_layout = VK_NULL_HANDLE;
    }
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

bool DebugDraw::createPipeline() {
    auto vertCode = readFile(SPV_DEBUG_VERT_PATH);
    auto fragCode = readFile(SPV_DEBUG_FRAG_PATH);
    if (vertCode.empty() || fragCode.empty()) {
        return false;
    }

    m_vertModule = createShaderModule(m_ctx->device(), vertCode);
    m_fragModule = createShaderModule(m_ctx->device(), fragCode);
    if (!m_vertModule || !m_fragModule) {
        return false;
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.size = sizeof(LinePush);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    if (vkCreatePipelineLayout(m_ctx->device(), &layoutInfo, nullptr, &m_layout) != VK_SUCCESS) {
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = m_vertModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = m_fragModule;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binding{0, sizeof(MeshVertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, color)};

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 2;
    vertexInput.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.f;

    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_TRUE;
    depth.depthWriteEnable = VK_FALSE;
    depth.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;

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
    pipelineInfo.pDepthStencilState = &depth;
    pipelineInfo.pColorBlendState = &blend;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_layout;
    pipelineInfo.renderPass = m_ctx->renderPass();
    pipelineInfo.subpass = 0;

    return vkCreateGraphicsPipelines(m_ctx->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                   &m_pipeline) == VK_SUCCESS;
}

void DebugDraw::record(VkCommandBuffer cmd, entt::registry& registry, const CameraState& camera) {
    if (m_pipeline == VK_NULL_HANDLE) {
        return;
    }

    std::vector<MeshVertex> lines;
    appendAxis(lines, {0.f, 0.f, 0.f}, 2.f);

    auto view = registry.view<const TransformLocal, const RigidBodyComponent>();
    for (const auto entity : view) {
        const auto& transform = view.get<const TransformLocal>(entity);
        appendAxis(lines, transform.translation, 0.5f);
    }

    if (lines.size() < 2) {
        return;
    }

    // Per-frame debug geometry upload (small enough for sandbox).
    const VmaAllocator allocator = static_cast<VmaAllocator>(m_ctx->vma().get());
    VmaAllocationCreateInfo stagingInfo{};
    stagingInfo.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    stagingInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    const VkDeviceSize bytes = static_cast<VkDeviceSize>(lines.size() * sizeof(MeshVertex));
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bytes;
    bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocInfo{};
    if (vmaCreateBuffer(allocator, &bufferInfo, &stagingInfo, &buffer, &allocation, &allocInfo) !=
        VK_SUCCESS) {
        return;
    }
    std::memcpy(allocInfo.pMappedData, lines.data(), static_cast<size_t>(bytes));

    const VkExtent2D extent = m_ctx->swapchainExtent();
    VkViewport viewport{0.f, 0.f, static_cast<float>(extent.width),
                        static_cast<float>(extent.height), 0.f, 1.f};
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    LinePush push{};
    push.mvp = camera.viewProj;
    vkCmdPushConstants(cmd, m_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(LinePush), &push);

    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &buffer, &offset);
    vkCmdDraw(cmd, static_cast<uint32_t>(lines.size()), 1, 0, 0);

    vmaDestroyBuffer(allocator, buffer, allocation);
}

} // namespace engine
