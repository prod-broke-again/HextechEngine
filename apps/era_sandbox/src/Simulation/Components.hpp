#pragma once

#include "Simulation/EraData.hpp"
#include "Simulation/CityEvents.hpp"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <entt/entt.hpp>
#include <array>

namespace engine::era {

// ----------------------------------------------------------------------------
// Components
// ----------------------------------------------------------------------------

struct GridPosition {
    int x = 0;
    int z = 0;
};

struct BuildingComponent {
    BuildingType type = BuildingType::None;
};

struct ResidenceComponent {
    int currentInhabitants = 2;
    int maxInhabitants = 5;
    float foodSatisfaction = 1.0f;   // 0.0 .. 1.0
    float warmthSatisfaction = 1.0f; // 0.0 .. 1.0
    float overallSatisfaction = 1.0f;
    float consumptionTimer = 0.0f;
};

struct ProductionComponent {
    float progress = 0.0f;
    float cycleSeconds = 5.0f;
    bool isWorking = true;
    bool isBufferFull = false;
    float internalBuffer = 0.0f;     // Local stockpile at building (0 .. maxBuffer)
    float maxBuffer = 5.0f;          // Max capacity before production halts
    bool hasCourierAssigned = false; // True if a warehouse courier is heading to pick up
    BuildingAlertKind currentAlert = BuildingAlertKind::None; // Active alert state
};

enum class CarrierState : uint8_t {
    IdleAtWarehouse = 0,
    EnRouteToPickup,
    ReturningToWarehouse
};

struct CarrierComponent {
    CarrierState state = CarrierState::IdleAtWarehouse;
    entt::entity targetBuilding = entt::null;
    ResourceType carriedResource = ResourceType::Wood;
    float carriedAmount = 0.0f;
    bool hasCargo = false;
};

struct CarrierJourneyComponent {
    glm::vec3 currentPos{0.0f};
    glm::vec3 startPos{0.0f};
    glm::vec3 targetPos{0.0f};

    float progress = 0.0f; // 0.0 .. 1.0 along current journey
    float tripDuration = 2.5f;
    float bobbingTimer = 0.0f;
};

// ----------------------------------------------------------------------------
// Resources (Singletons)
// ----------------------------------------------------------------------------

struct CityState {
    ResourceBundle storage{};
    EraType currentEra = EraType::StoneAge;

    int totalPopulation = 0;
    int maxPopulation = 0;
    float averageSatisfaction = 1.0f;
    float taxIncomePerMinute = 0.0f;

    std::array<float, kResourceCount> productionRatesPerMin{};
    std::array<float, kResourceCount> consumptionRatesPerMin{};
};

enum class CellType : uint8_t {
    Empty = 0,
    Road,
    Building
};

struct CellData {
    CellType type = CellType::Empty;
    entt::entity entity = entt::null;
};

struct GridIndex {
    static constexpr int kGridSize = 32;
    std::array<std::array<CellData, kGridSize>, kGridSize> cells{};

    bool isInBounds(int x, int z) const {
        return (x >= 0 && x < kGridSize && z >= 0 && z < kGridSize);
    }
    
    const CellData& getCell(int x, int z) const {
        static const CellData kEmpty{};
        if (!isInBounds(x, z)) return kEmpty;
        return cells[z][x];
    }
};

struct CarrierPool {
    int activeCarrierCount = 0;
    int totalCarrierCount = 0;
};

} // namespace engine::era
