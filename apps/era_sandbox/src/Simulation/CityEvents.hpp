#pragma once

#include "Simulation/EraData.hpp"
#include <entt/entt.hpp>
#include <cstdint>

namespace engine::era {

enum class BuildingAlertKind : uint8_t {
    None = 0,
    StorageFull,   // Internal storage buffer reached max capacity
    MissingInput,  // Production halted due to missing input ingredients
    NoWorker       // Reserved for future workforce/labor mechanic
};

struct BuildingPlacedEvent {
    entt::entity entity = entt::null;
    int gridX = 0;
    int gridZ = 0;
    StringHash type = BuildingIds::None;
};

struct BuildingRemovedEvent {
    entt::entity entity = entt::null;
    int gridX = 0;
    int gridZ = 0;
};

struct CarrierSpawnedEvent {
    entt::entity entity = entt::null;
};

struct CarrierRemovedEvent {
    entt::entity entity = entt::null;
};

struct EraEvolvedEvent {
    EraType newEra = EraType::StoneAge;
};

struct BuildingStatusEvent {
    entt::entity entity = entt::null;
    StringHash buildingType = BuildingIds::None;
    BuildingAlertKind alert = BuildingAlertKind::None;
    bool active = false; // true = alert raised, false = alert resolved/cleared
    int gridX = 0;
    int gridZ = 0;
    ResourceType resource = ResourceType::Wood;
    float currentBuffer = 0.0f;
    float maxBuffer = 5.0f;
};

} // namespace engine::era
