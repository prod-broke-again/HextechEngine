#pragma once

#include "Simulation/CityEvents.hpp"
#include "Simulation/EraData.hpp"
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace engine::era {

enum class CellType : uint8_t {
    Empty = 0,
    Road,
    Building
};

struct CellData {
    CellType type = CellType::Empty;
    BuildingType buildingType = BuildingType::None;
    uint32_t buildingInstanceId = 0;
};

struct ResidenceState {
    int currentInhabitants = 2;
    int maxInhabitants = 5;
    float foodSatisfaction = 1.0f;   // 0.0 .. 1.0
    float warmthSatisfaction = 1.0f; // 0.0 .. 1.0
    float overallSatisfaction = 1.0f;
    float consumptionTimer = 0.0f;
};

struct ProductionState {
    float progress = 0.0f;
    float cycleSeconds = 5.0f;
    bool isWorking = true;
    bool isBufferFull = false;
    float internalBuffer = 0.0f;     // Local stockpile at building (0 .. maxBuffer)
    float maxBuffer = 5.0f;          // Max capacity before production halts
    bool hasCourierAssigned = false; // True if a warehouse courier is heading to pick up
    BuildingAlertKind currentAlert = BuildingAlertKind::None; // Active alert state
};

struct BuildingInstance {
    uint32_t id = 0;
    BuildingType type = BuildingType::None;
    int gridX = 0;
    int gridZ = 0;

    ResidenceState residence{};
    ProductionState production{};
};

enum class CarrierState : uint8_t {
    IdleAtWarehouse = 0,
    EnRouteToPickup,
    ReturningToWarehouse
};

struct CarrierAgent {
    uint32_t id = 0;
    CarrierState state = CarrierState::IdleAtWarehouse;
    uint32_t targetBuildingId = 0;
    ResourceType carriedResource = ResourceType::Wood;
    float carriedAmount = 0.0f;
    bool hasCargo = false;

    glm::vec3 currentPos{0.0f};
    glm::vec3 startPos{0.0f};
    glm::vec3 targetPos{0.0f};

    float progress = 0.0f; // 0.0 .. 1.0 along current journey
    float tripDuration = 2.5f;
    float bobbingTimer = 0.0f;
};

class CitySimulation {
public:
    static constexpr int kGridSize = 32;

    CitySimulation();

    void update(float deltaTime);

    // Grid queries
    [[nodiscard]] const CellData& getCell(int x, int z) const;
    [[nodiscard]] bool isInBounds(int x, int z) const;
    [[nodiscard]] bool canPlace(int x, int z, BuildingType type) const;
    [[nodiscard]] bool canAfford(BuildingType type) const;

    // Building Actions
    bool placeBuilding(int x, int z, BuildingType type, uint32_t& outBuildingId);
    bool demolishBuilding(int x, int z, uint32_t& outRemovedBuildingId);

    // Economy & Storage
    [[nodiscard]] const ResourceBundle& getStorage() const { return m_storage; }
    [[nodiscard]] float getResource(ResourceType type) const { return m_storage.get(type); }
    [[nodiscard]] float getNetRatePerMinute(ResourceType type) const;

    // Population & Taxes
    [[nodiscard]] int getTotalPopulation() const { return m_totalPopulation; }
    [[nodiscard]] int getMaxPopulation() const { return m_maxPopulation; }
    [[nodiscard]] float getAverageSatisfaction() const { return m_averageSatisfaction; }
    [[nodiscard]] float getTaxIncomePerMinute() const { return m_taxIncomePerMinute; }

    // Era & Evolution
    [[nodiscard]] EraType getCurrentEra() const { return m_currentEra; }
    [[nodiscard]] bool canEvolve() const;
    bool evolveToNextEra();
    [[nodiscard]] glm::vec3 getTownCenterPosition() const;

    // Building instances
    [[nodiscard]] const std::vector<BuildingInstance>& getBuildings() const { return m_buildings; }
    [[nodiscard]] const BuildingInstance* getBuildingById(uint32_t id) const;
    [[nodiscard]] const BuildingInstance* getBuildingAt(int x, int z) const;

    // Couriers
    [[nodiscard]] const std::vector<CarrierAgent>& getCarriers() const { return m_carriers; }
    [[nodiscard]] int getActiveCarrierCount() const;
    [[nodiscard]] int getTotalCarrierCount() const;
    void initCarrierPool();

    // Callbacks
    using OnBuildingPlacedCallback = std::function<void(const BuildingInstance&)>;
    using OnBuildingRemovedCallback = std::function<void(uint32_t id, int x, int z)>;
    using OnCarrierSpawnedCallback = std::function<void(const CarrierAgent&)>;
    using OnCarrierRemovedCallback = std::function<void(uint32_t id)>;
    using OnEvolvedCallback = std::function<void(EraType newEra)>;
    using OnBuildingStatusChangedCallback = std::function<void(const BuildingStatusEvent&)>;

    void setOnBuildingPlaced(OnBuildingPlacedCallback cb) { m_onPlaced = std::move(cb); }
    void setOnBuildingRemoved(OnBuildingRemovedCallback cb) { m_onRemoved = std::move(cb); }
    void setOnCarrierSpawned(OnCarrierSpawnedCallback cb) { m_onCarrierSpawned = std::move(cb); }
    void setOnCarrierRemoved(OnCarrierRemovedCallback cb) { m_onCarrierRemoved = std::move(cb); }
    void setOnEvolved(OnEvolvedCallback cb) { m_onEvolved = std::move(cb); }
    void setOnBuildingStatusChanged(OnBuildingStatusChangedCallback cb) { m_onStatusChanged = std::move(cb); }

private:
    void tickProduction(float dt);
    void tickConsumption(float dt);
    void tickCarriers(float dt);
    void updateAggregateStats();

    void syncCarrierPool();
    glm::vec3 getIdleCarrierRestPos(size_t index, size_t total) const;
    float computeTripDuration(const glm::vec3& from, const glm::vec3& to) const;

    std::array<std::array<CellData, kGridSize>, kGridSize> m_grid{};
    std::vector<BuildingInstance> m_buildings;
    std::vector<CarrierAgent> m_carriers;
    uint32_t m_nextBuildingId = 1;
    uint32_t m_nextCarrierId = 1;

    ResourceBundle m_storage{};
    EraType m_currentEra = EraType::StoneAge;

    int m_totalPopulation = 0;
    int m_maxPopulation = 0;
    float m_averageSatisfaction = 1.0f;
    float m_taxIncomePerMinute = 0.0f;

    std::array<float, kResourceCount> m_productionRatesPerMin{};
    std::array<float, kResourceCount> m_consumptionRatesPerMin{};

    OnBuildingPlacedCallback m_onPlaced;
    OnBuildingRemovedCallback m_onRemoved;
    OnCarrierSpawnedCallback m_onCarrierSpawned;
    OnCarrierRemovedCallback m_onCarrierRemoved;
    OnEvolvedCallback m_onEvolved;
    OnBuildingStatusChangedCallback m_onStatusChanged;
};

} // namespace engine::era
