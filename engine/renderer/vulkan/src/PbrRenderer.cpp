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

    if (!createShadowResources()) {
        log(LogLevel::Error, "PbrRenderer: shadow resources failed");
        return false;
    }

    if (!createShadowPipeline()) {
        log(LogLevel::Error, "PbrRenderer: shadow pipeline failed");
        return false;
    }

    createSkyPipeline();

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
    log(LogLevel::Info, "PbrRenderer ready with Shadow Mapping & Atmospheric Sky");
    return true;
}

bool PbrRenderer::initTextureCache(GpuTextureCache& textures) {
    return textures.init(m_descriptorSetLayout, m_descriptorPool, m_sampler);
}

void PbrRenderer::shutdown() {
    destroySkyPipeline();
    destroyShadowPipeline();
    destroyShadowResources();
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
        if (m_shadowVertModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, m_shadowVertModule, nullptr);
            m_shadowVertModule = VK_NULL_HANDLE;
        }
        if (m_skyVertModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, m_skyVertModule, nullptr);
            m_skyVertModule = VK_NULL_HANDLE;
        }
        if (m_skyFragModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device, m_skyFragModule, nullptr);
            m_skyFragModule = VK_NULL_HANDLE;
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
    auto shadowVert = readFile(SPV_SHADOW_VERT_PATH);
    auto skyVert = readFile(SPV_SKY_VERT_PATH);
    auto skyFrag = readFile(SPV_SKY_FRAG_PATH);

    if (vert.empty() || frag.empty()) {
        return false;
    }

    m_vertModule = createShaderModule(m_ctx->device(), vert);
    m_fragModule = createShaderModule(m_ctx->device(), frag);
    if (!shadowVert.empty()) {
        m_shadowVertModule = createShaderModule(m_ctx->device(), shadowVert);
    }
    if (!skyVert.empty()) {
        m_skyVertModule = createShaderModule(m_ctx->device(), skyVert);
    }
    if (!skyFrag.empty()) {
        m_skyFragModule = createShaderModule(m_ctx->device(), skyFrag);
    }

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

    // Shadow descriptor set layout (Set 1)
    VkDescriptorSetLayoutBinding shadowBinding{};
    shadowBinding.binding = 0;
    shadowBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowBinding.descriptorCount = 1;
    shadowBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo shadowLayoutInfo{};
    shadowLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    shadowLayoutInfo.bindingCount = 1;
    shadowLayoutInfo.pBindings = &shadowBinding;

    if (vkCreateDescriptorSetLayout(m_ctx->device(), &shadowLayoutInfo, nullptr,
                                    &m_shadowDescriptorSetLayout) != VK_SUCCESS) {
        log(LogLevel::Error, "PbrRenderer: shadow descriptor set layout failed");
        return false;
    }

    VkDescriptorPoolSize shadowPoolSize{};
    shadowPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowPoolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo shadowPoolInfo{};
    shadowPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    shadowPoolInfo.maxSets = 1;
    shadowPoolInfo.poolSizeCount = 1;
    shadowPoolInfo.pPoolSizes = &shadowPoolSize;

    if (vkCreateDescriptorPool(m_ctx->device(), &shadowPoolInfo, nullptr, &m_shadowDescriptorPool) != VK_SUCCESS) {
        log(LogLevel::Error, "PbrRenderer: shadow descriptor pool failed");
        return false;
    }

    VkDescriptorSetAllocateInfo shadowAllocInfo{};
    shadowAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    shadowAllocInfo.descriptorPool = m_shadowDescriptorPool;
    shadowAllocInfo.descriptorSetCount = 1;
    shadowAllocInfo.pSetLayouts = &m_shadowDescriptorSetLayout;

    if (vkAllocateDescriptorSets(m_ctx->device(), &shadowAllocInfo, &m_shadowDescriptorSet) != VK_SUCCESS) {
        log(LogLevel::Error, "PbrRenderer: shadow descriptor set allocate failed");
        return false;
    }

    VkDescriptorImageInfo shadowImgInfo{};
    shadowImgInfo.sampler = m_shadowSampler;
    shadowImgInfo.imageView = m_shadowImageView;
    shadowImgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet shadowWrite{};
    shadowWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    shadowWrite.dstSet = m_shadowDescriptorSet;
    shadowWrite.dstBinding = 0;
    shadowWrite.dstArrayElement = 0;
    shadowWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadowWrite.descriptorCount = 1;
    shadowWrite.pImageInfo = &shadowImgInfo;

    vkUpdateDescriptorSets(m_ctx->device(), 1, &shadowWrite, 0, nullptr);

    return true;
}

void PbrRenderer::destroyDescriptorResources() {
    if (!m_ctx) {
        return;
    }

    VkDevice device = m_ctx->device();

    if (m_shadowDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_shadowDescriptorPool, nullptr);
        m_shadowDescriptorPool = VK_NULL_HANDLE;
    }

    if (m_shadowDescriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_shadowDescriptorSetLayout, nullptr);
        m_shadowDescriptorSetLayout = VK_NULL_HANDLE;
    }

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



    VkDescriptorSetLayout layouts[] = {m_descriptorSetLayout, m_shadowDescriptorSetLayout};

    VkPipelineLayoutCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.setLayoutCount = 2;
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



bool PbrRenderer::createShadowResources() {
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = kShadowMapResolution;
    imageInfo.extent.height = kShadowMapResolution;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_D32_SFLOAT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
    allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    VmaAllocator allocator = static_cast<VmaAllocator>(m_ctx->vma().get());
    if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &m_shadowImage,
                       reinterpret_cast<VmaAllocation*>(&m_shadowAllocation), nullptr) != VK_SUCCESS) {
        log(LogLevel::Error, "PbrRenderer: vmaCreateImage (shadow) failed");
        return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_shadowImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_D32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_ctx->device(), &viewInfo, nullptr, &m_shadowImageView) != VK_SUCCESS) {
        log(LogLevel::Error, "PbrRenderer: vkCreateImageView (shadow) failed");
        return false;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable = VK_FALSE;

    if (vkCreateSampler(m_ctx->device(), &samplerInfo, nullptr, &m_shadowSampler) != VK_SUCCESS) {
        log(LogLevel::Error, "PbrRenderer: vkCreateSampler (shadow) failed");
        return false;
    }

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pColorAttachments = nullptr;
    subpass.pDepthStencilAttachment = &depthRef;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &depthAttachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    rpInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(m_ctx->device(), &rpInfo, nullptr, &m_shadowRenderPass) != VK_SUCCESS) {
        log(LogLevel::Error, "PbrRenderer: vkCreateRenderPass (shadow) failed");
        return false;
    }

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = m_shadowRenderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &m_shadowImageView;
    fbInfo.width = kShadowMapResolution;
    fbInfo.height = kShadowMapResolution;
    fbInfo.layers = 1;

    if (vkCreateFramebuffer(m_ctx->device(), &fbInfo, nullptr, &m_shadowFramebuffer) != VK_SUCCESS) {
        log(LogLevel::Error, "PbrRenderer: vkCreateFramebuffer (shadow) failed");
        return false;
    }

    return true;
}

void PbrRenderer::destroyShadowResources() {
    if (!m_ctx) return;
    VkDevice device = m_ctx->device();

    if (m_shadowFramebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device, m_shadowFramebuffer, nullptr);
        m_shadowFramebuffer = VK_NULL_HANDLE;
    }
    if (m_shadowRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_shadowRenderPass, nullptr);
        m_shadowRenderPass = VK_NULL_HANDLE;
    }
    if (m_shadowSampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_shadowSampler, nullptr);
        m_shadowSampler = VK_NULL_HANDLE;
    }
    if (m_shadowImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, m_shadowImageView, nullptr);
        m_shadowImageView = VK_NULL_HANDLE;
    }
    if (m_shadowImage != VK_NULL_HANDLE) {
        vmaDestroyImage(static_cast<VmaAllocator>(m_ctx->vma().get()), m_shadowImage,
                        static_cast<VmaAllocation>(m_shadowAllocation));
        m_shadowImage = VK_NULL_HANDLE;
        m_shadowAllocation = nullptr;
    }
}

bool PbrRenderer::createShadowPipeline() {
    if (m_shadowVertModule == VK_NULL_HANDLE) {
        return false;
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(ShadowPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(m_ctx->device(), &layoutInfo, nullptr, &m_shadowPipelineLayout) != VK_SUCCESS) {
        return false;
    }

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    stage.module = m_shadowVertModule;
    stage.pName = "main";

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
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.depthBiasEnable = VK_TRUE;
    raster.depthBiasConstantFactor = 1.25f;
    raster.depthBiasSlopeFactor = 1.75f;

    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    VkPipelineColorBlendStateCreateInfo blendState{};
    blendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blendState.attachmentCount = 0;
    blendState.pAttachments = nullptr;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 1;
    pipelineInfo.pStages = &stage;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &msaa;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &blendState;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_shadowPipelineLayout;
    pipelineInfo.renderPass = m_shadowRenderPass;
    pipelineInfo.subpass = 0;

    return vkCreateGraphicsPipelines(m_ctx->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                     &m_shadowPipeline) == VK_SUCCESS;
}

void PbrRenderer::destroyShadowPipeline() {
    if (!m_ctx) return;
    VkDevice device = m_ctx->device();
    if (m_shadowPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_shadowPipeline, nullptr);
        m_shadowPipeline = VK_NULL_HANDLE;
    }
    if (m_shadowPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_shadowPipelineLayout, nullptr);
        m_shadowPipelineLayout = VK_NULL_HANDLE;
    }
}

bool PbrRenderer::createSkyPipeline() {
    if (m_skyVertModule == VK_NULL_HANDLE || m_skyFragModule == VK_NULL_HANDLE) {
        return false;
    }

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(SkyPushConstants);

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(m_ctx->device(), &layoutInfo, nullptr, &m_skyPipelineLayout) != VK_SUCCESS) {
        return false;
    }

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = m_skyVertModule;
    stages[0].pName = "main";

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = m_skyFragModule;
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
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

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
    pipelineInfo.layout = m_skyPipelineLayout;
    pipelineInfo.renderPass = m_ctx->renderPass();
    pipelineInfo.subpass = 0;

    return vkCreateGraphicsPipelines(m_ctx->device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                     &m_skyPipeline) == VK_SUCCESS;
}

void PbrRenderer::destroySkyPipeline() {
    if (!m_ctx) return;
    VkDevice device = m_ctx->device();
    if (m_skyPipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_skyPipeline, nullptr);
        m_skyPipeline = VK_NULL_HANDLE;
    }
    if (m_skyPipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_skyPipelineLayout, nullptr);
        m_skyPipelineLayout = VK_NULL_HANDLE;
    }
}

glm::mat4 PbrRenderer::computeLightViewProj(const glm::vec3& lightDir) const {
    const glm::vec3 normLight = glm::length(lightDir) > 0.001f ? glm::normalize(lightDir) : glm::vec3(0.f, -1.f, 0.f);
    const glm::vec3 sceneCenter(0.f, 0.f, 0.f);
    const glm::vec3 lightPos = sceneCenter - normLight * 35.f;
    const glm::vec3 up = std::abs(normLight.y) > 0.99f ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f);
    const glm::mat4 lightView = glm::lookAt(lightPos, sceneCenter, up);

    const float orthoExtent = 22.f;
    glm::mat4 lightProj = glm::ortho(-orthoExtent, orthoExtent, -orthoExtent, orthoExtent, 1.0f, 75.f);
    lightProj[1][1] *= -1.f;
    return lightProj * lightView;
}

glm::mat4 PbrRenderer::computeLightSpaceMatrix(const glm::mat4& lightViewProj) const {
    const glm::mat4 clipToUv(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f
    );
    return clipToUv * lightViewProj;
}

void PbrRenderer::recordShadowPass(VkCommandBuffer cmd, entt::registry& registry, GpuMeshCache& meshes) {
    if (m_shadowPipeline == VK_NULL_HANDLE || m_shadowFramebuffer == VK_NULL_HANDLE) {
        return;
    }

    const glm::mat4 lightViewProj = computeLightViewProj(m_lightDir);

    VkClearValue clearValue{};
    clearValue.depthStencil = {1.f, 0};

    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = m_shadowRenderPass;
    rpInfo.framebuffer = m_shadowFramebuffer;
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = {kShadowMapResolution, kShadowMapResolution};
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{0.f, 0.f, static_cast<float>(kShadowMapResolution),
                        static_cast<float>(kShadowMapResolution), 0.f, 1.f};
    VkRect2D scissor{{0, 0}, {kShadowMapResolution, kShadowMapResolution}};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipeline);

    auto view = registry.view<const TransformWorld, const MeshComponent, const RenderableTag>();
    for (const auto entity : view) {
        const auto& world = view.get<const TransformWorld>(entity);
        const auto& meshComp = view.get<const MeshComponent>(entity);
        const GpuMesh* mesh = meshes.get(meshComp.mesh);
        if (!mesh || mesh->indexCount == 0) continue;

        ShadowPushConstants push{};
        push.lightMvp = lightViewProj * world.matrix;

        vkCmdPushConstants(cmd, m_shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(ShadowPushConstants), &push);

        const VkDeviceSize offset = 0;
        vkCmdBindVertexBuffers(cmd, 0, 1, &mesh->vertexBuffer, &offset);
        vkCmdBindIndexBuffer(cmd, mesh->indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, mesh->indexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
}

void PbrRenderer::recordSkyPass(VkCommandBuffer cmd, const CameraState& camera) {
    if (m_skyPipeline == VK_NULL_HANDLE) return;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_skyPipeline);

    SkyPushConstants push{};
    glm::mat4 viewNoTrans = glm::mat4(glm::mat3(camera.view));
    push.invViewProj = glm::inverse(camera.proj * viewNoTrans);
    push.sunDir = glm::vec4(m_lightDir, 0.f);

    vkCmdPushConstants(cmd, m_skyPipelineLayout,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(SkyPushConstants), &push);

    vkCmdDraw(cmd, 3, 1, 0, 0);
}

void PbrRenderer::recordScene(VkCommandBuffer cmd, entt::registry& registry, GpuMeshCache& meshes,
                              GpuTextureCache& textures, const CameraState& camera) {
    if (!m_ready || m_graphicsPipeline == VK_NULL_HANDLE) {
        return;
    }

    const VkExtent2D extent = m_ctx->swapchainExtent();
    VkViewport viewport{0.f, 0.f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 1.f};
    VkRect2D scissor{{0, 0}, extent};

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // 1. Draw atmospheric sky
    recordSkyPass(cmd, camera);

    // 2. Bind PBR pipeline and shadow map descriptor set (Set 1)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 1, 1,
                            &m_shadowDescriptorSet, 0, nullptr);

    const glm::mat4 lightSpaceMatrix = computeLightSpaceMatrix(computeLightViewProj(m_lightDir));

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
        push.lightSpaceMvp = lightSpaceMatrix * world.matrix;
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

