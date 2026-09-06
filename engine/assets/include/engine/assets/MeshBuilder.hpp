#pragma once

#include "engine/assets/MeshData.hpp"

namespace engine {

class MeshBuilder {
public:
    [[nodiscard]] static MeshCpuData plane(float halfExtent, const glm::vec3& color);
    [[nodiscard]] static MeshCpuData box(const glm::vec3& halfExtents, const glm::vec3& color);
    [[nodiscard]] static MeshCpuData sphere(float radius, uint32_t rings, uint32_t sectors,
                                            const glm::vec3& color);
};

} // namespace engine
