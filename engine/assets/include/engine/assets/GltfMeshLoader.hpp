#pragma once

#include "engine/assets/AssetManager.hpp"
#include "engine/assets/MeshData.hpp"

#include <cgltf.h>
#include <glm/vec4.hpp>

#include <vector>

namespace engine {

struct GltfMeshPart {
    MeshCpuData mesh;
    glm::vec4 baseColorFactor{1.f};
    float metallic{0.f};
    float roughness{0.5f};
    const cgltf_texture* baseColorTexture{nullptr};
};

class GltfMeshLoader {
public:
    [[nodiscard]] static std::vector<GltfMeshPart> extractMeshParts(const LoadedGltfCpu& gltf);
};

} // namespace engine
