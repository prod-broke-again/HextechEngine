#pragma once

#include "Simulation/EraData.hpp"
#include "Simulation/CityEvents.hpp"
#include "engine/modules/economy/Inventory.hpp"
#include "engine/modules/spatial/SpatialGrid2D.hpp"
#include "engine/modules/statemachine/StateMachine.hpp"
#include "engine/modules/timer/TickTimer.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <entt/entt.hpp>
#include <array>
#include <unordered_map>

namespace engine::era {

// ----------------------------------------------------------------------------
// Components
// ----------------------------------------------------------------------------

struct GridPosition {
    int x = 0;
    int z = 0;
};

struct BuildingComponent {
    StringHash type = BuildingIds::None;
    uint8_t facing = 0; // 0..3, 90-degree steps clockwise
};

struct ResidenceComponent {
    int currentInhabitants = 2;
    int maxInhabitants = 5;
    float foodSatisfaction = 1.0f;   // 0.0 .. 1.0
    float warmthSatisfaction = 1.0f; // 0.0 .. 1.0
    float overallSatisfaction = 1.0f;
    engine::timer::TickTimer consumptionTimer{20, true}; // 1 second at 20 TPS
};

struct ProductionComponent {
    engine::timer::TickTimer cycleTimer{80, true}; // Initialized based on cycleSeconds * 20
    float cycleSeconds = 5.0f;
    bool isWorking = true;
    bool isBufferFull = false;
    float internalBuffer = 0.0f;     // Local stockpile at building (0 .. maxBuffer)
    float maxBuffer = 5.0f;          // Max capacity before production halts
    bool hasCourierAssigned = false; // True if a warehouse courier is heading to pick up
    BuildingAlertKind currentAlert = BuildingAlertKind::None; // Active alert state
};

namespace CarrierStates {
    inline constexpr StringHash IdleAtWarehouse = "idle_warehouse"_sh;
    inline constexpr StringHash EnRouteToPickup = "en_route_pickup"_sh;
    inline constexpr StringHash ReturningToWarehouse = "returning_warehouse"_sh;
}

struct CarrierComponent {
    entt::entity targetBuilding = entt::null;
    StringHash carriedResource = ResourceIds::None;
    float carriedAmount = 0.0f;
    bool hasCargo = false;
};

struct CarrierJourneyComponent {
    static constexpr int kMaxPath = 96;

    glm::vec3 currentPos{0.0f};
    glm::vec3 startPos{0.0f};
    glm::vec3 targetPos{0.0f};

    float progress = 0.0f; // 0.0 .. 1.0 along current segment
    float tripDuration = 2.5f;
    float bobbingTimer = 0.0f;

    int destX = 0;
    int destZ = 0;
    std::array<uint8_t, kMaxPath> pathX{};
    std::array<uint8_t, kMaxPath> pathZ{};
    uint8_t pathLength = 0;
    uint8_t pathIndex = 0;
};

// ----------------------------------------------------------------------------
// Resources (Singletons)
// ----------------------------------------------------------------------------

struct CityState {
    engine::economy::Inventory storage;
    EraType currentEra = EraType::StoneAge;

    int totalPopulation = 0;
    int maxPopulation = 0;
    float averageSatisfaction = 1.0f;
    float taxIncomePerMinute = 0.0f;

    std::unordered_map<StringHash, float> productionRatesPerMin;
    std::unordered_map<StringHash, float> consumptionRatesPerMin;

    [[nodiscard]] float getProductionRate(StringHash res) const {
        auto it = productionRatesPerMin.find(res);
        return (it != productionRatesPerMin.end()) ? it->second : 0.0f;
    }

    [[nodiscard]] float getConsumptionRate(StringHash res) const {
        auto it = consumptionRatesPerMin.find(res);
        return (it != consumptionRatesPerMin.end()) ? it->second : 0.0f;
    }

    [[nodiscard]] float getNetRate(StringHash res) const {
        return getProductionRate(res) - getConsumptionRate(res);
    }
};

enum class CellType : uint8_t {
    Empty = 0,
    Road,
    Building,
    Trail    // Worn dirt path left by couriers
};

[[nodiscard]] inline bool isBuildableCell(CellType type) {
    return type == CellType::Empty || type == CellType::Trail;
}

struct CellData {
    CellType type = CellType::Empty;
    entt::entity entity = entt::null;
};

struct GridIndex : public engine::spatial::SpatialGrid2D<CellData, 32, 32> {
    static constexpr int kGridSize = 32;

    [[nodiscard]] bool isInBounds(int x, int z) const noexcept {
        return inBounds(x, z);
    }

    [[nodiscard]] const CellData& getCell(int x, int z) const noexcept {
        static const CellData kEmpty{};
        if (!isInBounds(x, z)) return kEmpty;
        return at(x, z);
    }
};

struct CarrierPool {
    int activeCarrierCount = 0;
    int totalCarrierCount = 0;
};

} // namespace engine::era
