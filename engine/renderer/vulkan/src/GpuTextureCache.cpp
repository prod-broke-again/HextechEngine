#include "engine/renderer/vulkan/GpuTextureCache.hpp"

#include "engine/core/Log.hpp"

#include <vk_mem_alloc.h>

#include <cstring>

namespace engine {

namespace {

void freeTextureResources(VulkanContext& ctx, GpuTexture& texture) {
    const VmaAllocator allocator = static_cast<VmaAllocator>(ctx.vma().get());
    if (texture.view != VK_NULL_HANDLE) {
        vkDestroyImageView(ctx.device(), texture.view, nullptr);
        texture.view = VK_NULL_HANDLE;
    }
    if (texture.image != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, texture.image, static_cast<VmaAllocation>(texture.allocation));
        texture.image = VK_NULL_HANDLE;
        texture.allocation = nullptr;
    }
}

} // namespace

GpuTextureCache::GpuTextureCache(VulkanContext& ctx) : m_ctx(&ctx) {}

bool GpuTextureCache::init(VkDescriptorSetLayout descriptorSetLayout, VkDescriptorPool descriptorPool,
                           VkSampler sampler) {
    m_descriptorSetLayout = descriptorSetLayout;
    m_descriptorPool = descriptorPool;
    m_sampler = sampler;
    return createDefaultTexture();
}

void GpuTextureCache::shutdown() {
    if (!m_ctx) {
        m_textures.clear();
        return;
    }

    for (GpuTexture& texture : m_textures) {
        freeTextureResources(*m_ctx, texture);
    }
    m_textures.clear();
    m_defaultDescriptorSet = VK_NULL_HANDLE;
    m_ctx = nullptr;
}

bool GpuTextureCache::createImage2D(const LoadedTextureCpu& texture, GpuTexture& out) {
    if (!m_ctx || texture.width <= 0 || texture.height <= 0 || texture.pixels.empty()) {
        return false;
    }

    const VmaAllocator allocator = static_cast<VmaAllocator>(m_ctx->vma().get());

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {static_cast<uint32_t>(texture.width), static_cast<uint32_t>(texture.height), 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (vmaCreateImage(allocator, &imageInfo, &allocInfo, &out.image,
                       reinterpret_cast<VmaAllocation*>(&out.allocation),
                       nullptr) != VK_SUCCESS) {
        return false;
    }

    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = texture.pixels.size();
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;
    VmaAllocationInfo stagingMapped{};
    if (vmaCreateBuffer(allocator, &stagingInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation,
                        &stagingMapped) != VK_SUCCESS) {
        vmaDestroyImage(allocator, out.image, static_cast<VmaAllocation>(out.allocation));
        out.image = VK_NULL_HANDLE;
        return false;
    }
    std::memcpy(stagingMapped.pMappedData, texture.pixels.data(), texture.pixels.size());

    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = m_ctx->commandPool();
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    vkAllocateCommandBuffers(m_ctx->device(), &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = out.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {static_cast<uint32_t>(texture.width), static_cast<uint32_t>(texture.height),
                          1};
    vkCmdCopyBufferToImage(cmd, stagingBuffer, out.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                           &region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_ctx->graphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_ctx->graphicsQueue());
    vkFreeCommandBuffers(m_ctx->device(), m_ctx->commandPool(), 1, &cmd);

    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = out.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(m_ctx->device(), &viewInfo, nullptr, &out.view) != VK_SUCCESS) {
        vmaDestroyImage(allocator, out.image, static_cast<VmaAllocation>(out.allocation));
        out.image = VK_NULL_HANDLE;
        return false;
    }

    out.width = static_cast<uint32_t>(texture.width);
    out.height = static_cast<uint32_t>(texture.height);
    return true;
}

bool GpuTextureCache::createDefaultTexture() {
    LoadedTextureCpu white{};
    white.width = 1;
    white.height = 1;
    white.channels = 4;
    white.pixels = {255, 255, 255, 255};

    GpuTexture texture{};
    if (!createImage2D(white, texture)) {
        log(LogLevel::Error, "GpuTextureCache: default texture failed");
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    if (vkAllocateDescriptorSets(m_ctx->device(), &allocInfo, &texture.descriptorSet) != VK_SUCCESS) {
        freeTextureResources(*m_ctx, texture);
        return false;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = texture.view;
    imageInfo.sampler = m_sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = texture.descriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(m_ctx->device(), 1, &write, 0, nullptr);

    m_defaultDescriptorSet = texture.descriptorSet;
    m_textures.push_back(texture);
    return true;
}

GpuTextureId GpuTextureCache::upload(const LoadedTextureCpu& texture) {
    if (!m_ctx) {
        return kInvalidGpuTexture;
    }

    GpuTexture gpu{};
    if (!createImage2D(texture, gpu)) {
        log(LogLevel::Error, "GpuTextureCache: upload failed");
        return kInvalidGpuTexture;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_descriptorSetLayout;
    if (vkAllocateDescriptorSets(m_ctx->device(), &allocInfo, &gpu.descriptorSet) != VK_SUCCESS) {
        log(LogLevel::Error, "GpuTextureCache: descriptor set allocation failed");
        freeTextureResources(*m_ctx, gpu);
        return kInvalidGpuTexture;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = gpu.view;
    imageInfo.sampler = m_sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = gpu.descriptorSet;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(m_ctx->device(), 1, &write, 0, nullptr);

    m_textures.push_back(gpu);
    return static_cast<GpuTextureId>(m_textures.size() - 1);
}

const GpuTexture* GpuTextureCache::get(GpuTextureId id) const {
    if (id >= m_textures.size()) {
        return nullptr;
    }
    return &m_textures[id];
}

} // namespace engine
