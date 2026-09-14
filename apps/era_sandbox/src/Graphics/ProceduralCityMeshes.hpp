#pragma once

#include "Simulation/EraData.hpp"
#include "engine/assets/MeshData.hpp"
#include "engine/renderer/vulkan/GpuMeshCache.hpp"

#include <array>
#include <cstdint>

namespace engine::era {

class ProceduralCityMeshes {
public:
    void init(GpuMeshCache& cache);

    [[nodiscard]] uint32_t getBuildingMesh(BuildingType type, EraType era) const;
    [[nodiscard]] uint32_t getMeshId(BuildingType type) const { return getBuildingMesh(type, EraType::StoneAge); }
    [[nodiscard]] uint32_t getRoadMeshId() const { return getBuildingMesh(BuildingType::Road, EraType::StoneAge); }
    [[nodiscard]] uint32_t getCarrierMesh(EraType era) const;
    [[nodiscard]] uint32_t getAlertIconMesh() const { return m_alertIconMesh; }

private:
    std::array<std::array<uint32_t, kBuildingTypeCount>, kEraCount> m_buildingMeshes{};
    std::array<uint32_t, kEraCount> m_carrierMeshes{};
    uint32_t m_alertIconMesh = engine::kInvalidGpuMesh;
};

} // namespace engine::era
