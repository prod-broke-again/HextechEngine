#pragma once

#include "Simulation/EraData.hpp"
#include "engine/assets/MeshData.hpp"
#include "engine/renderer/vulkan/GpuMeshCache.hpp"

#include <array>
#include <cstdint>
#include <unordered_map>

namespace engine::era {

class ProceduralCityMeshes {
public:
    void init(GpuMeshCache& cache);

    [[nodiscard]] uint32_t getBuildingMesh(StringHash type, EraType era) const;
    [[nodiscard]] uint32_t getMeshId(StringHash type) const { return getBuildingMesh(type, EraType::StoneAge); }
    [[nodiscard]] uint32_t getRoadMeshId() const { return getBuildingMesh(BuildingIds::Road, EraType::StoneAge); }
    [[nodiscard]] uint32_t getCarrierMesh(EraType era) const;
    [[nodiscard]] uint32_t getAlertIconMesh() const { return m_alertIconMesh; }

private:
    std::unordered_map<StringHash, std::array<uint32_t, kEraCount>> m_buildingMeshes;
    std::array<uint32_t, kEraCount> m_carrierMeshes{};
    uint32_t m_alertIconMesh = engine::kInvalidGpuMesh;
};

} // namespace engine::era
