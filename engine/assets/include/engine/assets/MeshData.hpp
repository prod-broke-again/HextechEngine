#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <vector>

namespace engine {

using GpuMeshId = uint32_t;
using GpuTextureId = uint32_t;
inline constexpr GpuMeshId kInvalidGpuMesh = UINT32_MAX;
inline constexpr GpuTextureId kInvalidGpuTexture = UINT32_MAX;

struct MeshVertex {
    glm::vec3 position{0.f};
    glm::vec3 normal{0.f};
    glm::vec2 uv{0.f};
    glm::vec3 color{1.f};
};

struct MeshCpuData {
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    [[nodiscard]] bool empty() const { return vertices.empty() || indices.empty(); }
};

} // namespace engine
