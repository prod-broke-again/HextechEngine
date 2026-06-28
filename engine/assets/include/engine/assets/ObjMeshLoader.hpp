#pragma once

#include "engine/assets/MeshData.hpp"

#include <filesystem>
#include <vector>

namespace engine {

class ObjMeshLoader {
public:
    [[nodiscard]] static MeshCpuData loadFromFile(const std::filesystem::path& path);

    /// Uniformly scales mesh to targetMaxExtent and translates so min Y sits at 0.
    static void normalize(MeshCpuData& mesh, float targetMaxExtent = 1.5f);
    static void normalize(std::vector<MeshCpuData>& meshes, float targetMaxExtent = 1.5f);
};

} // namespace engine
