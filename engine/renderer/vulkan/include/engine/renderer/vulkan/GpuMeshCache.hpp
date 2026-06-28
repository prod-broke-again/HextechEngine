#pragma once

#include "engine/assets/MeshData.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace engine {

struct GpuMesh {
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    void* vertexAllocation = nullptr;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    void* indexAllocation = nullptr;
    uint32_t indexCount = 0;
};

class GpuMeshCache {
public:
    explicit GpuMeshCache(VulkanContext& ctx);

    [[nodiscard]] GpuMeshId upload(const MeshCpuData& mesh);
    [[nodiscard]] const GpuMesh* get(GpuMeshId id) const;
    void clear();

private:
    VulkanContext* m_ctx = nullptr;
    std::vector<GpuMesh> m_meshes;
};

} // namespace engine
