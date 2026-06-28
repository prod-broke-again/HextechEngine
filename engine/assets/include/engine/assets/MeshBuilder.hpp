#pragma once

#include "engine/assets/MeshData.hpp"

namespace engine {

class MeshBuilder {
public:
    [[nodiscard]] static MeshCpuData plane(float halfExtent, const glm::vec3& color);
    [[nodiscard]] static MeshCpuData box(const glm::vec3& halfExtents, const glm::vec3& color);
};

} // namespace engine
