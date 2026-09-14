#include "engine/renderer/vulkan/PostProcessPipeline.hpp"

#include "engine/core/Log.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"

#include <vk_mem_alloc.h>

#include <algorithm>
#include <array>
#include <cmath>
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
    if (code.empty()) {
        return VK_NULL_HANDLE;
    }
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

bool PostProcessPipeline::init(VulkanContext& ctx) {
    m_ctx = &ctx;

    if (!createShaders()) {
        log(LogLevel::Error, "PostProcessPipeline: failed to load shaders");
        return false;
    }
    if (!createSampler()) {
        log(LogLevel::Error, "PostProcessPipeline: failed to create sampler");
        return false;
    }
    if (!createDescriptorLayouts()) {
        log(LogLevel::Error, "PostProcessPipeline: failed to create descriptor layouts");
        return false;
    }
    if (!createBloomRenderPass()) {
        log(LogLevel::Error, "PostProcessPipeline: failed to create bloom render pass");
        return false;
    }
    if (!createPipelines()) {
        log(LogLevel::Error, "PostProcessPipeline: failed to create pipelines");
        return false;
    }

    const VkExtent2D extent = m_ctx->swapchainExtent();
    if (!createBloomImages(extent.width, extent.height)) {
        log(LogLevel::Error, "PostProcessPipeline: failed to create bloom images");
        return false;
    }

    m_ready = true;
    log(LogLevel::Info, "PostProcessPipeline initialized successfully (HDR, Dual-Kawase Bloom, Tone Mapping)");
    return true;
}

void PostProcessPipeline::shutdown() {
    if (!m_ctx) return;
    VkDevice device = m_ctx->device();

    destroyBloomImages();

    if (m_compositePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_compositePipeline, nullptr);
        m_compositePipeline = VK_NULL_HANDLE;
    }
    if (m_compositePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_compositePipelineLayout, nullptr);
        m_compositePipelineLayout = VK_NULL_HANDLE;
    }

    if (m_upsamplePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_upsamplePipeline, nullptr);
        m_upsamplePipeline = VK_NULL_HANDLE;
    }
    if (m_upsamplePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_upsamplePipelineLayout, nullptr);
        m_upsamplePipelineLayout = VK_NULL_HANDLE;
    }

    if (m_downsamplePipeline != VK_NULL_HANDLE) {
        vkDestroyPipeline(device, m_downsamplePipeline, nullptr);
        m_downsamplePipeline = VK_NULL_HANDLE;
    }
    if (m_downsamplePipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device, m_downsamplePipelineLayout, nullptr);
        m_downsamplePipelineLayout = VK_NULL_HANDLE;
    }

    if (m_bloomRenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, m_bloomRenderPass, nullptr);
        m_bloomRenderPass = VK_NULL_HANDLE;
    }

    if (m_singleTextureLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_singleTextureLayout, nullptr);
        m_singleTextureLayout = VK_NULL_HANDLE;
    }
    if (m_twoTextureLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, m_twoTextureLayout, nullptr);
        m_twoTextureLayout = VK_NULL_HANDLE;
    }

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    if (m_sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device, m_sampler, nullptr);
        m_sampler = VK_NULL_HANDLE;
    }

    if (m_vertShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_vertShader, nullptr);
        m_vertShader = VK_NULL_HANDLE;
    }
    if (m_downsampleFragShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_downsampleFragShader, nullptr);
        m_downsampleFragShader = VK_NULL_HANDLE;
    }
    if (m_upsampleFragShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_upsampleFragShader, nullptr);
        m_upsampleFragShader = VK_NULL_HANDLE;
    }
    if (m_compositeFragShader != VK_NULL_HANDLE) {
        vkDestroyShaderModule(device, m_compositeFragShader, nullptr);
        m_compositeFragShader = VK_NULL_HANDLE;
    }

    m_ready = false;
    m_ctx = nullptr;
}

void PostProcessPipeline::handleResize(uint32_t width, uint32_t height) {
    if (!m_ctx || width == 0 || height == 0) return;
    destroyBloomImages();
    createBloomImages(width, height);
}

bool PostProcessPipeline::createShaders() {
    VkDevice device = m_ctx->device();
    m_vertShader = createShaderModule(device, readFile(SPV_POSTPROCESS_VERT_PATH));
    m_downsampleFragShader = createShaderModule(device, readFile(SPV_BLOOM_DOWNSAMPLE_FRAG_PATH));
    m_upsampleFragShader = createShaderModule(device, readFile(SPV_BLOOM_UPSAMPLE_FRAG_PATH));
    m_compositeFragShader = createShaderModule(device, readFile(SPV_POSTPROCESS_COMPOSITE_FRAG_PATH));

    return m_vertShader != VK_NULL_HANDLE &&
           m_downsampleFragShader != VK_NULL_HANDLE &&
           m_upsampleFragShader != VK_NULL_HANDLE &&
           m_compositeFragShader != VK_NULL_HANDLE;
}

bool PostProcessPipeline::createSampler() {
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = VK_FILTER_LINEAR;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.minLod = 0.0f;
    info.maxLod = 1.0f;

    return vkCreateSampler(m_ctx->device(), &info, nullptr, &m_sampler) == VK_SUCCESS;
}

bool PostProcessPipeline::createDescriptorLayouts() {
    VkDevice device = m_ctx->device();

    // 1. Single texture layout (binding 0: src)
    VkDescriptorSetLayoutBinding singleBinding{};
    singleBinding.binding = 0;
    singleBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    singleBinding.descriptorCount = 1;
    singleBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo singleInfo{};
    singleInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    singleInfo.bindingCount = 1;
    singleInfo.pBindings = &singleBinding;

    if (vkCreateDescriptorSetLayout(device, &singleInfo, nullptr, &m_singleTextureLayout) != VK_SUCCESS) {
        return false;
    }

    // 2. Two texture layout (binding 0, binding 1)
    std::array<VkDescriptorSetLayoutBinding, 2> twoBindings{};
    twoBindings[0].binding = 0;
    twoBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    twoBindings[0].descriptorCount = 1;
    twoBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    twoBindings[1].binding = 1;
    twoBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    twoBindings[1].descriptorCount = 1;
    twoBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo twoInfo{};
    twoInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    twoInfo.bindingCount = static_cast<uint32_t>(twoBindings.size());
    twoInfo.pBindings = twoBindings.data();

    if (vkCreateDescriptorSetLayout(device, &twoInfo, nullptr, &m_twoTextureLayout) != VK_SUCCESS) {
        return false;
    }

    // 3. Pool for all post-process descriptor sets
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 64;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 32;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;

    return vkCreateDescriptorPool(device, &poolInfo, nullptr, &m_descriptorPool) == VK_SUCCESS;
}

bool PostProcessPipeline::createBloomRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    std::array<VkSubpassDependency, 2> dependencies{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &colorAttachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    rpInfo.pDependencies = dependencies.data();

    return vkCreateRenderPass(m_ctx->device(), &rpInfo, nullptr, &m_bloomRenderPass) == VK_SUCCESS;
}

bool PostProcessPipeline::createPipelines() {
    VkDevice device = m_ctx->device();

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
    raster.lineWidth = 1.0f;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo msaa{};
    msaa.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    msaa.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth{};
    depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth.depthTestEnable = VK_FALSE;
    depth.depthWriteEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;

    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    // 1. Downsample Pipeline
    VkPushConstantRange downRange{};
    downRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    downRange.offset = 0;
    downRange.size = sizeof(DownsamplePushConstants);

    VkPipelineLayoutCreateInfo downLayoutInfo{};
    downLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    downLayoutInfo.setLayoutCount = 1;
    downLayoutInfo.pSetLayouts = &m_singleTextureLayout;
    downLayoutInfo.pushConstantRangeCount = 1;
    downLayoutInfo.pPushConstantRanges = &downRange;

    if (vkCreatePipelineLayout(device, &downLayoutInfo, nullptr, &m_downsamplePipelineLayout) != VK_SUCCESS) {
        return false;
    }

    std::array<VkPipelineShaderStageCreateInfo, 2> downStages{};
    downStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    downStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    downStages[0].module = m_vertShader;
    downStages[0].pName = "main";
    downStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    downStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    downStages[1].module = m_downsampleFragShader;
    downStages[1].pName = "main";

    VkGraphicsPipelineCreateInfo downPipeInfo{};
    downPipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    downPipeInfo.stageCount = static_cast<uint32_t>(downStages.size());
    downPipeInfo.pStages = downStages.data();
    downPipeInfo.pVertexInputState = &vertexInput;
    downPipeInfo.pInputAssemblyState = &inputAssembly;
    downPipeInfo.pViewportState = &viewportState;
    downPipeInfo.pRasterizationState = &raster;
    downPipeInfo.pMultisampleState = &msaa;
    downPipeInfo.pDepthStencilState = &depth;
    downPipeInfo.pColorBlendState = &blend;
    downPipeInfo.pDynamicState = &dynamicState;
    downPipeInfo.layout = m_downsamplePipelineLayout;
    downPipeInfo.renderPass = m_bloomRenderPass;
    downPipeInfo.subpass = 0;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &downPipeInfo, nullptr, &m_downsamplePipeline) != VK_SUCCESS) {
        return false;
    }

    // 2. Upsample Pipeline
    VkPushConstantRange upRange{};
    upRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    upRange.offset = 0;
    upRange.size = sizeof(UpsamplePushConstants);

    VkPipelineLayoutCreateInfo upLayoutInfo{};
    upLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    upLayoutInfo.setLayoutCount = 1;
    upLayoutInfo.pSetLayouts = &m_twoTextureLayout;
    upLayoutInfo.pushConstantRangeCount = 1;
    upLayoutInfo.pPushConstantRanges = &upRange;

    if (vkCreatePipelineLayout(device, &upLayoutInfo, nullptr, &m_upsamplePipelineLayout) != VK_SUCCESS) {
        return false;
    }

    std::array<VkPipelineShaderStageCreateInfo, 2> upStages{};
    upStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    upStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    upStages[0].module = m_vertShader;
    upStages[0].pName = "main";
    upStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    upStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    upStages[1].module = m_upsampleFragShader;
    upStages[1].pName = "main";

    VkGraphicsPipelineCreateInfo upPipeInfo = downPipeInfo;
    upPipeInfo.pStages = upStages.data();
    upPipeInfo.layout = m_upsamplePipelineLayout;
    upPipeInfo.renderPass = m_bloomRenderPass;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &upPipeInfo, nullptr, &m_upsamplePipeline) != VK_SUCCESS) {
        return false;
    }

    // 3. Composite Pipeline (renders directly into swapchain renderPass)
    VkPushConstantRange compRange{};
    compRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    compRange.offset = 0;
    compRange.size = sizeof(CompositePushConstants);

    VkPipelineLayoutCreateInfo compLayoutInfo{};
    compLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    compLayoutInfo.setLayoutCount = 1;
    compLayoutInfo.pSetLayouts = &m_twoTextureLayout;
    compLayoutInfo.pushConstantRangeCount = 1;
    compLayoutInfo.pPushConstantRanges = &compRange;

    if (vkCreatePipelineLayout(device, &compLayoutInfo, nullptr, &m_compositePipelineLayout) != VK_SUCCESS) {
        return false;
    }

    std::array<VkPipelineShaderStageCreateInfo, 2> compStages{};
    compStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    compStages[0].module = m_vertShader;
    compStages[0].pName = "main";
    compStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    compStages[1].module = m_compositeFragShader;
    compStages[1].pName = "main";

    VkGraphicsPipelineCreateInfo compPipeInfo = downPipeInfo;
    compPipeInfo.pStages = compStages.data();
    compPipeInfo.layout = m_compositePipelineLayout;
    compPipeInfo.renderPass = m_ctx->renderPass(); // Swapchain presentation render pass

    return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &compPipeInfo, nullptr, &m_compositePipeline) == VK_SUCCESS;
}

bool PostProcessPipeline::createBloomImages(uint32_t baseWidth, uint32_t baseHeight) {
    VkDevice device = m_ctx->device();
    const VmaAllocator allocator = static_cast<VmaAllocator>(m_ctx->vma().get());

    uint32_t curWidth = baseWidth;
    uint32_t curHeight = baseHeight;

    for (uint32_t i = 0; i < kBloomLevels; ++i) {
        curWidth = std::max(1u, curWidth / 2);
        curHeight = std::max(1u, curHeight / 2);

        auto& mip = m_bloomMips[i];
        mip.extent = {curWidth, curHeight};

        // Create Downsample Image
        VkImageCreateInfo imgInfo{};
        imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        imgInfo.extent = {curWidth, curHeight, 1};
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

        if (vmaCreateImage(allocator, &imgInfo, &allocInfo, &mip.downsampleImage,
                           reinterpret_cast<VmaAllocation*>(&mip.downsampleAlloc), nullptr) != VK_SUCCESS) {
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = mip.downsampleImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &mip.downsampleView) != VK_SUCCESS) {
            return false;
        }

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = m_bloomRenderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &mip.downsampleView;
        fbInfo.width = curWidth;
        fbInfo.height = curHeight;
        fbInfo.layers = 1;

        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &mip.downsampleFramebuffer) != VK_SUCCESS) {
            return false;
        }

        // Create Upsample Image
        if (vmaCreateImage(allocator, &imgInfo, &allocInfo, &mip.upsampleImage,
                           reinterpret_cast<VmaAllocation*>(&mip.upsampleAlloc), nullptr) != VK_SUCCESS) {
            return false;
        }

        viewInfo.image = mip.upsampleImage;
        if (vkCreateImageView(device, &viewInfo, nullptr, &mip.upsampleView) != VK_SUCCESS) {
            return false;
        }

        fbInfo.pAttachments = &mip.upsampleView;
        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &mip.upsampleFramebuffer) != VK_SUCCESS) {
            return false;
        }
    }

    return updateDescriptorSets();
}

void PostProcessPipeline::destroyBloomImages() {
    if (!m_ctx) return;
    VkDevice device = m_ctx->device();
    const VmaAllocator allocator = static_cast<VmaAllocator>(m_ctx->vma().get());

    for (auto& mip : m_bloomMips) {
        if (mip.downsampleFramebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, mip.downsampleFramebuffer, nullptr);
            mip.downsampleFramebuffer = VK_NULL_HANDLE;
        }
        if (mip.downsampleView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, mip.downsampleView, nullptr);
            mip.downsampleView = VK_NULL_HANDLE;
        }
        if (mip.downsampleImage != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, mip.downsampleImage, static_cast<VmaAllocation>(mip.downsampleAlloc));
            mip.downsampleImage = VK_NULL_HANDLE;
            mip.downsampleAlloc = nullptr;
        }

        if (mip.upsampleFramebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device, mip.upsampleFramebuffer, nullptr);
            mip.upsampleFramebuffer = VK_NULL_HANDLE;
        }
        if (mip.upsampleView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, mip.upsampleView, nullptr);
            mip.upsampleView = VK_NULL_HANDLE;
        }
        if (mip.upsampleImage != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, mip.upsampleImage, static_cast<VmaAllocation>(mip.upsampleAlloc));
            mip.upsampleImage = VK_NULL_HANDLE;
            mip.upsampleAlloc = nullptr;
        }
    }
}

bool PostProcessPipeline::updateDescriptorSets() {
    VkDevice device = m_ctx->device();
    vkResetDescriptorPool(device, m_descriptorPool, 0);

    // 1. Allocate Downsample sets (kBloomLevels)
    std::vector<VkDescriptorSetLayout> singleLayouts(kBloomLevels, m_singleTextureLayout);
    VkDescriptorSetAllocateInfo singleAlloc{};
    singleAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    singleAlloc.descriptorPool = m_descriptorPool;
    singleAlloc.descriptorSetCount = kBloomLevels;
    singleAlloc.pSetLayouts = singleLayouts.data();

    std::vector<VkDescriptorSet> downSets(kBloomLevels);
    if (vkAllocateDescriptorSets(device, &singleAlloc, downSets.data()) != VK_SUCCESS) {
        return false;
    }
    for (uint32_t i = 0; i < kBloomLevels; ++i) {
        m_bloomMips[i].downsampleDescriptorSet = downSets[i];
    }

    // 2. Allocate Upsample sets (kBloomLevels) + 1 for Composite
    std::vector<VkDescriptorSetLayout> twoLayouts(kBloomLevels + 1, m_twoTextureLayout);
    VkDescriptorSetAllocateInfo twoAlloc{};
    twoAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    twoAlloc.descriptorPool = m_descriptorPool;
    twoAlloc.descriptorSetCount = static_cast<uint32_t>(twoLayouts.size());
    twoAlloc.pSetLayouts = twoLayouts.data();

    std::vector<VkDescriptorSet> twoSets(kBloomLevels + 1);
    if (vkAllocateDescriptorSets(device, &twoAlloc, twoSets.data()) != VK_SUCCESS) {
        return false;
    }
    for (uint32_t i = 0; i < kBloomLevels; ++i) {
        m_bloomMips[i].upsampleDescriptorSet = twoSets[i];
    }
    m_compositeDescriptorSet = twoSets[kBloomLevels];

    // Write Downsample Descriptor Sets
    for (uint32_t i = 0; i < kBloomLevels; ++i) {
        VkDescriptorImageInfo imgInfo{};
        imgInfo.sampler = m_sampler;
        imgInfo.imageView = (i == 0) ? m_ctx->hdrImageView() : m_bloomMips[i - 1].downsampleView;
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = m_bloomMips[i].downsampleDescriptorSet;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imgInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    // Write Upsample Descriptor Sets
    for (int i = static_cast<int>(kBloomLevels) - 2; i >= 0; --i) {
        VkDescriptorImageInfo lowerInfo{};
        lowerInfo.sampler = m_sampler;
        lowerInfo.imageView = (i == static_cast<int>(kBloomLevels) - 2) ? m_bloomMips[i + 1].downsampleView
                                                                       : m_bloomMips[i + 1].upsampleView;
        lowerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo higherInfo{};
        higherInfo.sampler = m_sampler;
        higherInfo.imageView = m_bloomMips[i].downsampleView;
        higherInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        std::array<VkWriteDescriptorSet, 2> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = m_bloomMips[i].upsampleDescriptorSet;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &lowerInfo;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = m_bloomMips[i].upsampleDescriptorSet;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo = &higherInfo;

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    // Write Composite Descriptor Set (0: HDR Scene, 1: Bloom mip 0)
    VkDescriptorImageInfo hdrInfo{};
    hdrInfo.sampler = m_sampler;
    hdrInfo.imageView = m_ctx->hdrImageView();
    hdrInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkDescriptorImageInfo bloomInfo{};
    bloomInfo.sampler = m_sampler;
    bloomInfo.imageView = m_bloomMips[0].upsampleView;
    bloomInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 2> compWrites{};
    compWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    compWrites[0].dstSet = m_compositeDescriptorSet;
    compWrites[0].dstBinding = 0;
    compWrites[0].descriptorCount = 1;
    compWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    compWrites[0].pImageInfo = &hdrInfo;

    compWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    compWrites[1].dstSet = m_compositeDescriptorSet;
    compWrites[1].dstBinding = 1;
    compWrites[1].descriptorCount = 1;
    compWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    compWrites[1].pImageInfo = &bloomInfo;

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(compWrites.size()), compWrites.data(), 0, nullptr);

    return true;
}

void PostProcessPipeline::recordBloom(VkCommandBuffer cmd) {
    if (!m_ready || !m_settings.bloomEnabled) {
        return;
    }

    // 1. Downsample Pyramid
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_downsamplePipeline);

    const float knee = std::max(1e-4f, m_settings.bloomKnee);
    const float threshold = m_settings.bloomThreshold;

    for (uint32_t i = 0; i < kBloomLevels; ++i) {
        const auto& mip = m_bloomMips[i];

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = m_bloomRenderPass;
        rpInfo.framebuffer = mip.downsampleFramebuffer;
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = mip.extent;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{0.f, 0.f, static_cast<float>(mip.extent.width), static_cast<float>(mip.extent.height), 0.f, 1.f};
        VkRect2D scissor{{0, 0}, mip.extent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        DownsamplePushConstants push{};
        const float srcW = (i == 0) ? static_cast<float>(m_ctx->swapchainExtent().width) : static_cast<float>(m_bloomMips[i - 1].extent.width);
        const float srcH = (i == 0) ? static_cast<float>(m_ctx->swapchainExtent().height) : static_cast<float>(m_bloomMips[i - 1].extent.height);
        push.srcTexelSize[0] = 1.0f / srcW;
        push.srcTexelSize[1] = 1.0f / srcH;
        push.srcTexelSize[2] = 0.0f;
        push.srcTexelSize[3] = 0.0f;

        push.params[0] = threshold;
        push.params[1] = threshold - knee;
        push.params[2] = 2.0f * knee;
        push.params[3] = 0.25f / knee;

        push.isPrefilter = (i == 0) ? 1 : 0;

        vkCmdPushConstants(cmd, m_downsamplePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(DownsamplePushConstants), &push);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_downsamplePipelineLayout, 0, 1, &mip.downsampleDescriptorSet, 0, nullptr);

        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }

    // 2. Upsample Pyramid
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_upsamplePipeline);

    for (int i = static_cast<int>(kBloomLevels) - 2; i >= 0; --i) {
        const auto& mip = m_bloomMips[i];

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = m_bloomRenderPass;
        rpInfo.framebuffer = mip.upsampleFramebuffer;
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = mip.extent;

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{0.f, 0.f, static_cast<float>(mip.extent.width), static_cast<float>(mip.extent.height), 0.f, 1.f};
        VkRect2D scissor{{0, 0}, mip.extent};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        UpsamplePushConstants push{};
        const float lowerW = static_cast<float>(m_bloomMips[i + 1].extent.width);
        const float lowerH = static_cast<float>(m_bloomMips[i + 1].extent.height);
        push.texelSize[0] = 1.0f / lowerW;
        push.texelSize[1] = 1.0f / lowerH;
        push.texelSize[2] = m_settings.filterRadius;
        push.texelSize[3] = 0.0f;

        vkCmdPushConstants(cmd, m_upsamplePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UpsamplePushConstants), &push);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_upsamplePipelineLayout, 0, 1, &mip.upsampleDescriptorSet, 0, nullptr);

        vkCmdDraw(cmd, 3, 1, 0, 0);
        vkCmdEndRenderPass(cmd);
    }
}

void PostProcessPipeline::recordComposite(VkCommandBuffer cmd) {
    if (!m_ready) return;

    const VkExtent2D extent = m_ctx->swapchainExtent();
    VkViewport viewport{0.f, 0.f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.f, 1.f};
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipeline);

    CompositePushConstants push{};
    push.exposure = m_settings.exposure;
    push.bloomIntensity = m_settings.bloomIntensity;
    push.toneMapper = m_settings.toneMapper;
    push.bloomEnabled = m_settings.bloomEnabled ? 1 : 0;

    vkCmdPushConstants(cmd, m_compositePipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(CompositePushConstants), &push);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_compositePipelineLayout, 0, 1, &m_compositeDescriptorSet, 0, nullptr);

    vkCmdDraw(cmd, 3, 1, 0, 0);
}

} // namespace engine
