#pragma once

#include "Simulation/EraData.hpp"
#include <cstdint>

namespace engine::era {

enum class BuildingAlertKind : uint8_t {
    None = 0,
    StorageFull,   // Internal storage buffer reached max capacity
    MissingInput,  // Production halted due to missing input ingredients
    NoWorker       // Reserved for future workforce/labor mechanic
};

struct BuildingStatusEvent {
    uint32_t buildingId = 0;
    BuildingType buildingType = BuildingType::None;
    BuildingAlertKind alert = BuildingAlertKind::None;
    bool active = false; // true = alert raised, false = alert resolved/cleared
    int gridX = 0;
    int gridZ = 0;
    ResourceType resource = ResourceType::Wood;
    float currentBuffer = 0.0f;
    float maxBuffer = 5.0f;
};

} // namespace engine::era
