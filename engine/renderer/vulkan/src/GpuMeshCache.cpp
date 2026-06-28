#include "engine/renderer/vulkan/GpuMeshCache.hpp"

#include "engine/core/Log.hpp"

#include <vk_mem_alloc.h>

#include <cstring>

namespace engine {

namespace {

bool uploadBuffer(VulkanContext& ctx, VkBufferUsageFlags usage, const void* data, VkDeviceSize size,
                  VkBuffer* outBuffer, void** outAllocation) {
    const VmaAllocator allocator = static_cast<VmaAllocator>(ctx.vma().get());

    VmaAllocationCreateInfo stagingInfo{};
    stagingInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    stagingInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBufferCreateInfo stagingCreate{};
    stagingCreate.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingCreate.size = size;
    stagingCreate.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = VK_NULL_HANDLE;
    VmaAllocationInfo stagingAllocInfo{};
    if (vmaCreateBuffer(allocator, &stagingCreate, &stagingInfo, &stagingBuffer, &stagingAllocation,
                        &stagingAllocInfo) != VK_SUCCESS) {
        return false;
    }
    std::memcpy(stagingAllocInfo.pMappedData, data, static_cast<size_t>(size));

    VmaAllocationCreateInfo gpuInfo{};
    gpuInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkBufferCreateInfo gpuCreate{};
    gpuCreate.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    gpuCreate.size = size;
    gpuCreate.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (vmaCreateBuffer(allocator, &gpuCreate, &gpuInfo, outBuffer,
                        reinterpret_cast<VmaAllocation*>(outAllocation),
                        nullptr) != VK_SUCCESS) {
        vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
        return false;
    }

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = ctx.commandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(ctx.device(), &allocInfo, &cmd) != VK_SUCCESS) {
        vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkBufferCopy copy{};
    copy.size = size;
    vkCmdCopyBuffer(cmd, stagingBuffer, *outBuffer, 1, &copy);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(ctx.graphicsQueue(), 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(ctx.graphicsQueue());

    vkFreeCommandBuffers(ctx.device(), ctx.commandPool(), 1, &cmd);
    vmaDestroyBuffer(allocator, stagingBuffer, stagingAllocation);
    return true;
}

} // namespace

GpuMeshCache::GpuMeshCache(VulkanContext& ctx) : m_ctx(&ctx) {}

GpuMeshId GpuMeshCache::upload(const MeshCpuData& mesh) {
    if (!m_ctx || mesh.empty()) {
        return kInvalidGpuMesh;
    }

    GpuMesh gpu{};
    const VkDeviceSize vertexBytes =
        static_cast<VkDeviceSize>(mesh.vertices.size() * sizeof(MeshVertex));
    const VkDeviceSize indexBytes =
        static_cast<VkDeviceSize>(mesh.indices.size() * sizeof(uint32_t));

    if (!uploadBuffer(*m_ctx, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.vertices.data(), vertexBytes,
                      &gpu.vertexBuffer, &gpu.vertexAllocation)) {
        log(LogLevel::Error, "GpuMeshCache: vertex upload failed");
        return kInvalidGpuMesh;
    }
    if (!uploadBuffer(*m_ctx, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.indices.data(), indexBytes,
                      &gpu.indexBuffer, &gpu.indexAllocation)) {
        const VmaAllocator allocator = static_cast<VmaAllocator>(m_ctx->vma().get());
        vmaDestroyBuffer(allocator, gpu.vertexBuffer, static_cast<VmaAllocation>(gpu.vertexAllocation));
        log(LogLevel::Error, "GpuMeshCache: index upload failed");
        return kInvalidGpuMesh;
    }

    gpu.indexCount = static_cast<uint32_t>(mesh.indices.size());
    m_meshes.push_back(gpu);
    return static_cast<GpuMeshId>(m_meshes.size() - 1);
}

const GpuMesh* GpuMeshCache::get(GpuMeshId id) const {
    if (id >= m_meshes.size()) {
        return nullptr;
    }
    return &m_meshes[id];
}

void GpuMeshCache::clear() {
    if (!m_ctx) {
        m_meshes.clear();
        return;
    }
    const VmaAllocator allocator = static_cast<VmaAllocator>(m_ctx->vma().get());
    for (GpuMesh& mesh : m_meshes) {
        if (mesh.vertexBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, mesh.vertexBuffer,
                             static_cast<VmaAllocation>(mesh.vertexAllocation));
        }
        if (mesh.indexBuffer != VK_NULL_HANDLE) {
            vmaDestroyBuffer(allocator, mesh.indexBuffer,
                             static_cast<VmaAllocation>(mesh.indexAllocation));
        }
    }
    m_meshes.clear();
}

} // namespace engine
