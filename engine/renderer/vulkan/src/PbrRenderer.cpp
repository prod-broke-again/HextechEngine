#include "engine/renderer/vulkan/PbrRenderer.hpp"



#include "engine/core/Log.hpp"

#include "engine/renderer/vulkan/ShaderHotReload.hpp"

#include "engine/renderer/vulkan/VulkanContext.hpp"



#include "engine/assets/MeshData.hpp"



#include <glm/gtc/matrix_transform.hpp>

#include <vk_mem_alloc.h>



#include <array>

#include <cstddef>

#include <cstring>

#include <fstream>

#include <string>

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



bool runGlslc(const char* stage, const char* input, const char* output) {

    std::string cmd = std::string("glslc -fshader-stage=") + stage + " -o \"" + output + "\" \"" +

                      input + "\"";

    return std::system(cmd.c_str()) == 0;

}



} // namespace



bool PbrRenderer::compileShaderSources() {

    const std::string shaderRoot = SHADER_DIR;

    const bool vertOk =

        runGlslc("vertex", (shaderRoot + "/pbr.vert").c_str(), SPV_PBR_VERT_PATH);

    const bool fragOk =

        runGlslc("fragment", (shaderRoot + "/pbr.frag").c_str(), SPV_PBR_FRAG_PATH);

    return vertOk && fragOk;

}



bool PbrRenderer::init(VulkanContext& ctx) {

    m_ctx = &ctx;

    if (!loadShaderModules()) {

        log(LogLevel::Warn, "PbrRenderer: shaders missing");

        return false;

    }

    if (!createDescriptorResources()) {

        return false;

    }

    if (!createPipelineLayout()) {

        return false;

    }

    if (!createGraphicsPipeline()) {

        return false;

    }

    m_ready = true;

    log(LogLevel::Info, "PbrRenderer ready");

    return true;

}



bool PbrRenderer::initTextureCache(GpuTextureCache& textures) {

    return textures.init(m_descriptorSetLayout, m_descriptorPool, m_sampler);

}



void PbrRenderer::shutdown() {

    destroyPipeline();

    destroyDescriptorResources();

    if (m_ctx) {

        VkDevice device = m_ctx->device();

        if (m_vertModule != VK_NULL_HANDLE) {

            vkDestroyShaderModule(device, m_vertModule, nullptr);

            m_vertModule = VK_NULL_HANDLE;

        }

        if (m_fragModule != VK_NULL_HANDLE) {

            vkDestroyShaderModule(device, m_fragModule, nullptr);

            m_fragModule = VK_NULL_HANDLE;

        }

    }

    m_ctx = nullptr;

    m_ready = false;

}



void PbrRenderer::tryReloadShaders() {

    if (!m_ctx) {

        return;

    }

    const std::string vertSource = std::string(SHADER_DIR) + "/pbr.vert";

    const std::string fragSource = std::string(SHADER_DIR) + "/pbr.frag";

    if (!shaderHotReloadPollChanged(vertSource.c_str()) &&

        !shaderHotReloadPollChanged(fragSource.c_str())) {

        return;

    }

    if (!compileShaderSources()) {

        log(LogLevel::Warn, "PbrRenderer: shader recompile failed");

        return;

    }

    vkDeviceWaitIdle(m_ctx->device());

    destroyPipeline();

    if (m_vertModule != VK_NULL_HANDLE) {

        vkDestroyShaderModule(m_ctx->device(), m_vertModule, nullptr);

        m_vertModule = VK_NULL_HANDLE;

    }

    if (m_fragModule != VK_NULL_HANDLE) {

        vkDestroyShaderModule(m_ctx->device(), m_fragModule, nullptr);

        m_fragModule = VK_NULL_HANDLE;

    }

    if (!loadShaderModules() || !createPipelineLayout() || !createGraphicsPipeline()) {

        log(LogLevel::Error, "PbrRenderer: pipeline reload failed");

        m_ready = false;

        return;

    }

    m_ready = true;

    log(LogLevel::Info, "PbrRenderer: shaders reloaded");

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



bool PbrRenderer::createDescriptorResources() {

    VkDescriptorSetLayoutBinding samplerBinding{};

    samplerBinding.binding = 0;

    samplerBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    samplerBinding.descriptorCount = 1;

    samplerBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;



    VkDescriptorSetLayoutCreateInfo layoutInfo{};

    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;

    layoutInfo.bindingCount = 1;

    layoutInfo.pBindings = &samplerBinding;

    if (vkCreateDescriptorSetLayout(m_ctx->device(), &layoutInfo, nullptr,

                                    &m_descriptorSetLayout) != VK_SUCCESS) {

        log(LogLevel::Error, "PbrRenderer: descriptor set layout failed");

        return false;

    }



    VkDescriptorPoolSize poolSize{};

    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;

    poolSize.descriptorCount = 256;



    VkDescriptorPoolCreateInfo poolInfo{};

    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;

    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    poolInfo.maxSets = 256;

    poolInfo.poolSizeCount = 1;

    poolInfo.pPoolSizes = &poolSize;

    if (vkCreateDescriptorPool(m_ctx->device(), &poolInfo, nullptr, &m_descriptorPool) !=

        VK_SUCCESS) {

        log(LogLevel::Error, "PbrRenderer: descriptor pool failed");

        return false;

    }



    VkSamplerCreateInfo samplerInfo{};

    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

    samplerInfo.magFilter = VK_FILTER_LINEAR;

    samplerInfo.minFilter = VK_FILTER_LINEAR;

    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.f;

    samplerInfo.maxLod = 1.f;

    if (vkCreateSampler(m_ctx->device(), &samplerInfo, nullptr, &m_sampler) != VK_SUCCESS) {

        log(LogLevel::Error, "PbrRenderer: sampler failed");

        return false;

    }



    return true;

}



void PbrRenderer::destroyDescriptorResources() {

    if (!m_ctx) {

        return;

    }

    VkDevice device = m_ctx->device();

    if (m_sampler != VK_NULL_HANDLE) {

        vkDestroySampler(device, m_sampler, nullptr);

        m_sampler = VK_NULL_HANDLE;

    }

    if (m_descriptorPool != VK_NULL_HANDLE) {

        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);

        m_descriptorPool = VK_NULL_HANDLE;

    }

    if (m_descriptorSetLayout != VK_NULL_HANDLE) {

        vkDestroyDescriptorSetLayout(device, m_descriptorSetLayout, nullptr);

        m_descriptorSetLayout = VK_NULL_HANDLE;

    }

}



bool PbrRenderer::createPipelineLayout() {

    VkPushConstantRange pushRange{};

    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    pushRange.offset = 0;

    pushRange.size = sizeof(DrawPushConstants);



    VkDescriptorSetLayout layouts[] = {m_descriptorSetLayout};

    VkPipelineLayoutCreateInfo ci{};

    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    ci.setLayoutCount = 1;

    ci.pSetLayouts = layouts;

    ci.pushConstantRangeCount = 1;

    ci.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(m_ctx->device(), &ci, nullptr, &m_pipelineLayout) != VK_SUCCESS) {

        log(LogLevel::Error, "PbrRenderer: pipeline layout failed");

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



    VkVertexInputBindingDescription binding{};

    binding.binding = 0;

    binding.stride = sizeof(MeshVertex);

    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;



    VkVertexInputAttributeDescription attrs[4]{};

    attrs[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, position)};

    attrs[1] = {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, normal)};

    attrs[2] = {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(MeshVertex, uv)};

    attrs[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, color)};



    VkPipelineVertexInputStateCreateInfo vertexInput{};

    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    vertexInput.vertexBindingDescriptionCount = 1;

    vertexInput.pVertexBindingDescriptions = &binding;

    vertexInput.vertexAttributeDescriptionCount = 4;

    vertexInput.pVertexAttributeDescriptions = attrs;



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

    raster.cullMode = m_cullMode;

    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;



    VkPipelineMultisampleStateCreateInfo msaa{};

    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;



    VkPipelineDepthStencilStateCreateInfo depthStencil{};

    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    depthStencil.depthTestEnable = VK_TRUE;

    depthStencil.depthWriteEnable = VK_TRUE;

    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;



    VkPipelineColorBlendAttachmentState blendAttachment{};

    blendAttachment.colorWriteMask = 0xF;

    VkPipelineColorBlendStateCreateInfo blendState{};

    blendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;

    blendState.attachmentCount = 1;

    blendState.pAttachments = &blendAttachment;



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

        log(LogLevel::Error, "PbrRenderer: graphics pipeline failed");

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



void PbrRenderer::recordScene(VkCommandBuffer cmd, entt::registry& registry, GpuMeshCache& meshes,

                              GpuTextureCache& textures, const CameraState& camera) {

    if (!m_ready || m_graphicsPipeline == VK_NULL_HANDLE) {

        return;

    }



    const VkExtent2D extent = m_ctx->swapchainExtent();

    VkViewport viewport{};

    viewport.width = static_cast<float>(extent.width);

    viewport.height = static_cast<float>(extent.height);

    viewport.maxDepth = 1.f;

    VkRect2D scissor{{0, 0}, extent};



    vkCmdSetViewport(cmd, 0, 1, &viewport);

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);



    auto view = registry.view<const TransformWorld, const MeshComponent, const RenderableTag>();

    for (const auto entity : view) {

        const auto& world = view.get<const TransformWorld>(entity);

        const auto& meshComp = view.get<const MeshComponent>(entity);

        const GpuMesh* mesh = meshes.get(meshComp.mesh);

        if (!mesh || mesh->indexCount == 0) {

            continue;

        }



        DrawPushConstants push{};
        push.mvp = camera.viewProj * world.matrix;
        const glm::vec3 effectiveTint = meshComp.tint * glm::vec3(meshComp.baseColorFactor);
        push.tint = {effectiveTint, meshComp.baseColorFactor.a};
        const glm::vec3 normLight = glm::length(m_lightDir) > 0.001f ? glm::normalize(m_lightDir) : glm::vec3(0.f, -1.f, 0.f);
        push.lightDir = glm::vec4(normLight, 0.f);
        push.cameraPos = {camera.position, 1.f};

        const bool useTexture = meshComp.baseColorTexture != kInvalidGpuTexture;

        push.material = {meshComp.metallic, meshComp.roughness, useTexture ? 1.f : 0.f, 0.f};



        VkDescriptorSet descriptorSet = textures.defaultDescriptorSet();

        if (useTexture) {

            if (const GpuTexture* texture = textures.get(meshComp.baseColorTexture)) {

                descriptorSet = texture->descriptorSet;

            }

        }

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,

                                &descriptorSet, 0, nullptr);



        vkCmdPushConstants(cmd, m_pipelineLayout,

                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,

                           sizeof(DrawPushConstants), &push);



        const VkDeviceSize offset = 0;

        vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertexBuffer, &offset);

        vkCmdBindIndexBuffer(cmd, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd, mesh->indexCount, 1, 0, 0, 0);

    }

}

void PbrRenderer::setCullMode(VkCullModeFlags cullMode) {
    if (m_cullMode == cullMode) {
        return;
    }
    m_cullMode = cullMode;
    if (m_ctx && m_ready) {
        vkDeviceWaitIdle(m_ctx->device());
        destroyPipeline();
        if (!createPipelineLayout() || !createGraphicsPipeline()) {
            log(LogLevel::Error, "PbrRenderer: setCullMode pipeline recreation failed");
        }
    }
}

} // namespace engine

