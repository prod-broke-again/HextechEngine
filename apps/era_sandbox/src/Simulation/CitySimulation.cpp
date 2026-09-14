#include "Simulation/CitySimulation.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>

namespace engine::era {

CitySimulation::CitySimulation() {
    // Initial starting resources for Stone Age
    m_storage.set(ResourceType::Wood, 28.0f);
    m_storage.set(ResourceType::Fish, 20.0f);
    m_storage.set(ResourceType::Stone, 10.0f);
    m_storage.set(ResourceType::Grain, 0.0f);
    m_storage.set(ResourceType::Bread, 0.0f);
    m_storage.set(ResourceType::Gold, 25.0f);

    // Anchor Town Center at grid center (15, 15)
    uint32_t tcId = 0;
    placeBuilding(15, 15, BuildingType::TownCenter, tcId);
}

bool CitySimulation::isInBounds(int x, int z) const {
    return (x >= 0 && x < kGridSize && z >= 0 && z < kGridSize);
}

const CellData& CitySimulation::getCell(int x, int z) const {
    static const CellData kEmpty{};
    if (!isInBounds(x, z)) {
        return kEmpty;
    }
    return m_grid[z][x];
}

bool CitySimulation::canAfford(BuildingType type) const {
    const BuildingDef& def = getBuildingDef(type);
    return m_storage.canAfford(def.cost);
}

bool CitySimulation::canPlace(int x, int z, BuildingType type) const {
    if (!isInBounds(x, z) || type == BuildingType::None) {
        return false;
    }

    if (m_grid[z][x].type != CellType::Empty) {
        return false;
    }

    const BuildingDef& def = getBuildingDef(type);
    if (static_cast<uint8_t>(def.requiredEra) > static_cast<uint8_t>(m_currentEra)) {
        return false;
    }

    return canAfford(type);
}

glm::vec3 CitySimulation::getTownCenterPosition() const {
    for (const auto& b : m_buildings) {
        if (b.type == BuildingType::TownCenter) {
            return {static_cast<float>(b.gridX) + 0.5f, 0.0f, static_cast<float>(b.gridZ) + 0.5f};
        }
    }
    return {15.5f, 0.0f, 15.5f};
}

glm::vec3 CitySimulation::getIdleCarrierRestPos(size_t index, size_t total) const {
    const glm::vec3 tcPos = getTownCenterPosition();
    const float radius = 0.75f;
    const float angle = (static_cast<float>(index) / static_cast<float>(std::max<size_t>(1, total))) * 6.283185f;
    return {tcPos.x + std::cos(angle) * radius, 0.0f, tcPos.z + std::sin(angle) * radius};
}

float CitySimulation::computeTripDuration(const glm::vec3& from, const glm::vec3& to) const {
    const float dist = glm::distance(from, to);
    const CarrierDef cDef = getCarrierDefForEra(m_currentEra);
    float speed = cDef.speed;

    const int midX = static_cast<int>((from.x + to.x) * 0.5f);
    const int midZ = static_cast<int>((from.z + to.z) * 0.5f);
    if (isInBounds(midX, midZ) && m_grid[midZ][midX].type == CellType::Road) {
        speed *= cDef.roadSpeedMultiplier;
    }

    return std::max(1.2f, dist / std::max(0.5f, speed));
}

void CitySimulation::initCarrierPool() {
    syncCarrierPool();
}

void CitySimulation::syncCarrierPool() {
    const EraDefinition& eraDef = getEraDefinition(m_currentEra);
    const size_t targetCount = static_cast<size_t>(eraDef.carrierPoolSize);

    while (m_carriers.size() < targetCount) {
        CarrierAgent agent{};
        agent.id = m_nextCarrierId++;
        agent.state = CarrierState::IdleAtWarehouse;
        agent.currentPos = getIdleCarrierRestPos(m_carriers.size(), targetCount);
        agent.startPos = agent.currentPos;
        agent.targetPos = agent.currentPos;
        agent.progress = 0.0f;
        agent.hasCargo = false;
        agent.carriedAmount = 0.0f;

        m_carriers.push_back(agent);

        if (m_onCarrierSpawned) {
            m_onCarrierSpawned(agent);
        }
    }
}

int CitySimulation::getActiveCarrierCount() const {
    int active = 0;
    for (const auto& c : m_carriers) {
        if (c.state != CarrierState::IdleAtWarehouse) {
            active++;
        }
    }
    return active;
}

int CitySimulation::getTotalCarrierCount() const {
    return static_cast<int>(m_carriers.size());
}

bool CitySimulation::placeBuilding(int x, int z, BuildingType type, uint32_t& outBuildingId) {
    if (!isInBounds(x, z) || type == BuildingType::None) {
        return false;
    }
    if (m_grid[z][x].type != CellType::Empty) {
        return false;
    }

    const BuildingDef& def = getBuildingDef(type);
    if (!m_storage.tryConsume(def.cost)) {
        return false;
    }

    BuildingInstance inst{};
    inst.id = m_nextBuildingId++;
    inst.type = type;
    inst.gridX = x;
    inst.gridZ = z;

    if (type == BuildingType::Residence) {
        inst.residence.currentInhabitants = 2;
        inst.residence.maxInhabitants = (m_currentEra == EraType::BronzeAge) ? 8 : def.maxInhabitants;
        inst.residence.foodSatisfaction = 1.0f;
        inst.residence.warmthSatisfaction = 1.0f;
        inst.residence.overallSatisfaction = 1.0f;
        inst.residence.consumptionTimer = 0.0f;
    }

    if (def.production.outputPerMinute > 0.0f) {
        inst.production.progress = 0.0f;
        inst.production.cycleSeconds = def.production.cycleSeconds;
        inst.production.isWorking = true;
        inst.production.isBufferFull = false;
        inst.production.internalBuffer = 0.0f;
        inst.production.maxBuffer = 5.0f;
        inst.production.hasCourierAssigned = false;
    }

    CellData& cell = m_grid[z][x];
    cell.type = (type == BuildingType::Road) ? CellType::Road : CellType::Building;
    cell.buildingType = type;
    cell.buildingInstanceId = inst.id;

    m_buildings.push_back(inst);
    outBuildingId = inst.id;

    if (m_onPlaced) {
        m_onPlaced(inst);
    }

    updateAggregateStats();
    return true;
}

bool CitySimulation::demolishBuilding(int x, int z, uint32_t& outRemovedBuildingId) {
    if (!isInBounds(x, z)) {
        return false;
    }

    CellData& cell = m_grid[z][x];
    if (cell.type == CellType::Empty || cell.buildingType == BuildingType::TownCenter) {
        return false;
    }

    const uint32_t bId = cell.buildingInstanceId;
    outRemovedBuildingId = bId;

    // Refund 50% of cost
    const BuildingDef& def = getBuildingDef(cell.buildingType);
    for (size_t i = 0; i < kResourceCount; ++i) {
        m_storage.amounts[i] += def.cost.amounts[i] * 0.5f;
    }

    // Cancel any courier en route to pick up from this building
    const glm::vec3 tcPos = getTownCenterPosition();
    for (auto& c : m_carriers) {
        if (c.targetBuildingId == bId && c.state == CarrierState::EnRouteToPickup) {
            c.state = CarrierState::ReturningToWarehouse;
            c.startPos = c.currentPos;
            c.targetPos = tcPos;
            c.progress = 0.0f;
            c.tripDuration = computeTripDuration(c.startPos, c.targetPos);
            c.targetBuildingId = 0;
            c.hasCargo = false;
            c.carriedAmount = 0.0f;
        }
    }

    cell = CellData{};

    auto it = std::find_if(m_buildings.begin(), m_buildings.end(),
                           [bId](const BuildingInstance& b) { return b.id == bId; });
    if (it != m_buildings.end()) {
        if (it->production.currentAlert != BuildingAlertKind::None && m_onStatusChanged) {
            m_onStatusChanged(BuildingStatusEvent{
                .buildingId = bId,
                .buildingType = it->type,
                .alert = it->production.currentAlert,
                .active = false,
                .gridX = it->gridX,
                .gridZ = it->gridZ
            });
        }
        m_buildings.erase(it);
    }

    if (m_onRemoved) {
        m_onRemoved(bId, x, z);
    }

    updateAggregateStats();
    return true;
}

const BuildingInstance* CitySimulation::getBuildingById(uint32_t id) const {
    for (const auto& b : m_buildings) {
        if (b.id == id) {
            return &b;
        }
    }
    return nullptr;
}

const BuildingInstance* CitySimulation::getBuildingAt(int x, int z) const {
    if (!isInBounds(x, z)) {
        return nullptr;
    }
    const uint32_t id = m_grid[z][x].buildingInstanceId;
    return (id != 0) ? getBuildingById(id) : nullptr;
}

float CitySimulation::getNetRatePerMinute(ResourceType type) const {
    const size_t idx = static_cast<size_t>(type);
    return m_productionRatesPerMin[idx] - m_consumptionRatesPerMin[idx];
}

bool CitySimulation::canEvolve() const {
    if (m_currentEra != EraType::StoneAge) {
        return false;
    }

    const EraDefinition& eraDef = getEraDefinition(m_currentEra);
    if (m_totalPopulation < eraDef.requiredPopulation) {
        return false;
    }

    for (const auto& req : eraDef.evolutionRequirements) {
        if (m_storage.get(req.resource) < req.requiredAmount) {
            return false;
        }
    }

    return true;
}

bool CitySimulation::evolveToNextEra() {
    if (!canEvolve()) {
        return false;
    }

    const EraDefinition& eraDef = getEraDefinition(m_currentEra);
    for (const auto& req : eraDef.evolutionRequirements) {
        m_storage.add(req.resource, -req.requiredAmount);
    }

    m_currentEra = EraType::BronzeAge;

    // Upgrade all existing houses to tier 2: higher capacity (8) and bonus inhabitants
    for (auto& b : m_buildings) {
        if (b.type == BuildingType::Residence) {
            b.residence.maxInhabitants = 8;
            b.residence.currentInhabitants = std::min(8, b.residence.currentInhabitants + 2);
        }
    }

    // Expand warehouse courier fleet for Bronze Age
    syncCarrierPool();

    updateAggregateStats();

    if (m_onEvolved) {
        m_onEvolved(m_currentEra);
    }

    return true;
}

void CitySimulation::tickCarriers(float dt) {
    const CarrierDef cDef = getCarrierDefForEra(m_currentEra);
    const glm::vec3 tcPos = getTownCenterPosition();
    const size_t totalCarriers = m_carriers.size();

    for (size_t i = 0; i < m_carriers.size(); ++i) {
        auto& agent = m_carriers[i];

        if (agent.state == CarrierState::IdleAtWarehouse) {
            // Find most urgent production building that has goods waiting and no courier assigned
            BuildingInstance* bestCandidate = nullptr;
            float highestFillRatio = -1.0f;

            for (auto& b : m_buildings) {
                const BuildingDef& bDef = getBuildingDef(b.type);
                if (bDef.production.outputPerMinute <= 0.0f) {
                    continue;
                }
                if (b.production.internalBuffer < 1.0f) {
                    continue; // At least 1 unit to warrant dispatch
                }
                if (b.production.hasCourierAssigned) {
                    continue;
                }

                const float fillRatio = b.production.internalBuffer / std::max(0.1f, b.production.maxBuffer);
                if (fillRatio > highestFillRatio) {
                    highestFillRatio = fillRatio;
                    bestCandidate = &b;
                }
            }

            if (bestCandidate) {
                // Dispatch courier from Town Center to candidate
                bestCandidate->production.hasCourierAssigned = true;
                agent.state = CarrierState::EnRouteToPickup;
                agent.targetBuildingId = bestCandidate->id;
                agent.hasCargo = false;
                agent.carriedAmount = 0.0f;
                agent.startPos = agent.currentPos;
                agent.targetPos = {
                    static_cast<float>(bestCandidate->gridX) + 0.5f,
                    0.0f,
                    static_cast<float>(bestCandidate->gridZ) + 0.5f
                };
                agent.progress = 0.0f;
                agent.tripDuration = computeTripDuration(agent.startPos, agent.targetPos);
            } else {
                // Keep idle near Town Center hearth
                agent.currentPos = getIdleCarrierRestPos(i, totalCarriers);
            }
        } else {
            // Active journey (EnRouteToPickup or ReturningToWarehouse)
            agent.bobbingTimer += dt * 10.0f;
            agent.progress += dt / std::max(0.1f, agent.tripDuration);
            agent.currentPos = glm::mix(agent.startPos, agent.targetPos, std::min(1.0f, agent.progress));

            if (agent.state == CarrierState::EnRouteToPickup) {
                BuildingInstance* b = nullptr;
                for (auto& inst : m_buildings) {
                    if (inst.id == agent.targetBuildingId) {
                        b = &inst;
                        break;
                    }
                }

                if (!b) {
                    // Target building was demolished! Turn back to warehouse empty-handed
                    agent.state = CarrierState::ReturningToWarehouse;
                    agent.targetBuildingId = 0;
                    agent.startPos = agent.currentPos;
                    agent.targetPos = tcPos;
                    agent.progress = 0.0f;
                    agent.tripDuration = computeTripDuration(agent.startPos, agent.targetPos);
                } else if (agent.progress >= 1.0f) {
                    // Reached production building! Pick up goods!
                    const BuildingDef& bDef = getBuildingDef(b->type);
                    const float pickupAmount = std::min(static_cast<float>(cDef.capacity), b->production.internalBuffer);

                    b->production.internalBuffer = std::max(0.0f, b->production.internalBuffer - pickupAmount);
                    b->production.isBufferFull = (b->production.internalBuffer >= b->production.maxBuffer);
                    b->production.hasCourierAssigned = false;

                    // If building had an active StorageFull alert and is now freed, resolve it!
                    if (b->production.currentAlert == BuildingAlertKind::StorageFull && !b->production.isBufferFull) {
                        b->production.currentAlert = BuildingAlertKind::None;
                        if (m_onStatusChanged) {
                            m_onStatusChanged(BuildingStatusEvent{
                                .buildingId = b->id,
                                .buildingType = b->type,
                                .alert = BuildingAlertKind::StorageFull,
                                .active = false,
                                .gridX = b->gridX,
                                .gridZ = b->gridZ,
                                .resource = bDef.production.outputResource,
                                .currentBuffer = b->production.internalBuffer,
                                .maxBuffer = b->production.maxBuffer
                            });
                        }
                    }

                    agent.carriedResource = bDef.production.outputResource;
                    agent.carriedAmount = pickupAmount;
                    agent.hasCargo = (pickupAmount > 0.0f);

                    // Turn back to Town Center with cargo
                    agent.state = CarrierState::ReturningToWarehouse;
                    agent.startPos = agent.currentPos;
                    agent.targetPos = tcPos;
                    agent.progress = 0.0f;
                    agent.tripDuration = computeTripDuration(agent.startPos, agent.targetPos);
                }
            } else if (agent.state == CarrierState::ReturningToWarehouse) {
                if (agent.progress >= 1.0f) {
                    // Reached Town Center! Deliver goods into stockpile!
                    if (agent.hasCargo && agent.carriedAmount > 0.0f) {
                        m_storage.add(agent.carriedResource, agent.carriedAmount);
                    }

                    agent.state = CarrierState::IdleAtWarehouse;
                    agent.targetBuildingId = 0;
                    agent.hasCargo = false;
                    agent.carriedAmount = 0.0f;
                    agent.progress = 0.0f;
                    agent.currentPos = getIdleCarrierRestPos(i, totalCarriers);
                }
            }
        }
    }
}

void CitySimulation::tickProduction(float dt) {
    for (auto& b : m_buildings) {
        const BuildingDef& def = getBuildingDef(b.type);
        if (def.production.outputPerMinute <= 0.0f) {
            continue;
        }

        // Check if internal storage buffer is full
        if (b.production.internalBuffer >= b.production.maxBuffer) {
            b.production.isBufferFull = true;
            b.production.isWorking = false;

            if (b.production.currentAlert != BuildingAlertKind::StorageFull) {
                b.production.currentAlert = BuildingAlertKind::StorageFull;
                if (m_onStatusChanged) {
                    m_onStatusChanged(BuildingStatusEvent{
                        .buildingId = b.id,
                        .buildingType = b.type,
                        .alert = BuildingAlertKind::StorageFull,
                        .active = true,
                        .gridX = b.gridX,
                        .gridZ = b.gridZ,
                        .resource = def.production.outputResource,
                        .currentBuffer = b.production.internalBuffer,
                        .maxBuffer = b.production.maxBuffer
                    });
                }
            }
            continue;
        }

        const float cycle = std::max(0.5f, def.production.cycleSeconds);
        const float inputPerCycle = (def.production.inputPerMinute / 60.0f) * cycle;
        const float outputPerCycle = (def.production.outputPerMinute / 60.0f) * cycle;

        // Check if building needs input resource from Town Center stockpile
        if (def.production.inputPerMinute > 0.0f) {
            if (m_storage.get(def.production.inputResource) < inputPerCycle) {
                b.production.isWorking = false;
                if (b.production.currentAlert != BuildingAlertKind::MissingInput) {
                    b.production.currentAlert = BuildingAlertKind::MissingInput;
                    if (m_onStatusChanged) {
                        m_onStatusChanged(BuildingStatusEvent{
                            .buildingId = b.id,
                            .buildingType = b.type,
                            .alert = BuildingAlertKind::MissingInput,
                            .active = true,
                            .gridX = b.gridX,
                            .gridZ = b.gridZ,
                            .resource = def.production.inputResource,
                            .currentBuffer = b.production.internalBuffer,
                            .maxBuffer = b.production.maxBuffer
                        });
                    }
                }
                continue;
            } else if (b.production.currentAlert == BuildingAlertKind::MissingInput) {
                b.production.currentAlert = BuildingAlertKind::None;
                if (m_onStatusChanged) {
                    m_onStatusChanged(BuildingStatusEvent{
                        .buildingId = b.id,
                        .buildingType = b.type,
                        .alert = BuildingAlertKind::MissingInput,
                        .active = false,
                        .gridX = b.gridX,
                        .gridZ = b.gridZ,
                        .resource = def.production.inputResource,
                        .currentBuffer = b.production.internalBuffer,
                        .maxBuffer = b.production.maxBuffer
                    });
                }
            }
        }

        b.production.isWorking = true;
        b.production.isBufferFull = false;
        b.production.progress += dt;

        if (b.production.progress >= cycle) {
            b.production.progress -= cycle;

            if (def.production.inputPerMinute > 0.0f) {
                m_storage.add(def.production.inputResource, -inputPerCycle);
            }

            // Produced goods are placed into building's local buffer (Anno style)
            b.production.internalBuffer = std::min(b.production.maxBuffer, b.production.internalBuffer + outputPerCycle);
            if (b.production.internalBuffer >= b.production.maxBuffer) {
                b.production.isBufferFull = true;
                b.production.isWorking = false;

                if (b.production.currentAlert != BuildingAlertKind::StorageFull) {
                    b.production.currentAlert = BuildingAlertKind::StorageFull;
                    if (m_onStatusChanged) {
                        m_onStatusChanged(BuildingStatusEvent{
                            .buildingId = b.id,
                            .buildingType = b.type,
                            .alert = BuildingAlertKind::StorageFull,
                            .active = true,
                            .gridX = b.gridX,
                            .gridZ = b.gridZ,
                            .resource = def.production.outputResource,
                            .currentBuffer = b.production.internalBuffer,
                            .maxBuffer = b.production.maxBuffer
                        });
                    }
                }
            }
        }
    }
}

void CitySimulation::tickConsumption(float dt) {
    constexpr float kConsumptionInterval = 2.0f; // Check every 2s

    for (auto& b : m_buildings) {
        if (b.type != BuildingType::Residence) {
            continue;
        }

        b.residence.consumptionTimer += dt;
        if (b.residence.consumptionTimer >= kConsumptionInterval) {
            b.residence.consumptionTimer -= kConsumptionInterval;

            const float dtFactor = kConsumptionInterval / 60.0f;

            // 1. Food Need (Fish): 0.2 units/min per house
            const float fishNeeded = 0.20f * dtFactor;
            if (m_storage.get(ResourceType::Fish) >= fishNeeded) {
                m_storage.add(ResourceType::Fish, -fishNeeded);
                b.residence.foodSatisfaction = std::min(1.0f, b.residence.foodSatisfaction + 0.10f);
            } else {
                b.residence.foodSatisfaction = std::max(0.0f, b.residence.foodSatisfaction - 0.15f);
            }

            // 2. Warmth Need (Firewood / Wood): 0.15 units/min per house
            const float woodNeeded = 0.15f * dtFactor;
            if (m_storage.get(ResourceType::Wood) >= woodNeeded) {
                m_storage.add(ResourceType::Wood, -woodNeeded);
                b.residence.warmthSatisfaction = std::min(1.0f, b.residence.warmthSatisfaction + 0.10f);
            } else {
                b.residence.warmthSatisfaction = std::max(0.0f, b.residence.warmthSatisfaction - 0.15f);
            }

            b.residence.overallSatisfaction = (b.residence.foodSatisfaction + b.residence.warmthSatisfaction) * 0.5f;

            // Population growth / decline
            if (b.residence.overallSatisfaction >= 0.75f && b.residence.currentInhabitants < b.residence.maxInhabitants) {
                b.residence.currentInhabitants += 1;
            } else if (b.residence.overallSatisfaction < 0.25f && b.residence.currentInhabitants > 1) {
                b.residence.currentInhabitants -= 1;
            }

            // Taxes (Gold)
            const BuildingDef& def = getBuildingDef(b.type);
            const float tax = (def.baseTaxIncomePerMinute / 60.0f) * kConsumptionInterval *
                              b.residence.overallSatisfaction *
                              (static_cast<float>(b.residence.currentInhabitants) / static_cast<float>(b.residence.maxInhabitants));
            m_storage.add(ResourceType::Gold, tax);
        }
    }
}

void CitySimulation::updateAggregateStats() {
    int totalPop = 0;
    int maxPop = 0;
    float sumSat = 0.0f;
    int houseCount = 0;

    m_productionRatesPerMin.fill(0.0f);
    m_consumptionRatesPerMin.fill(0.0f);
    m_taxIncomePerMinute = 0.0f;

    for (const auto& b : m_buildings) {
        const BuildingDef& def = getBuildingDef(b.type);

        if (b.type == BuildingType::Residence) {
            totalPop += b.residence.currentInhabitants;
            maxPop += b.residence.maxInhabitants;
            sumSat += b.residence.overallSatisfaction;
            houseCount++;

            // Consumption per house
            m_consumptionRatesPerMin[static_cast<size_t>(ResourceType::Fish)] += 0.20f;
            m_consumptionRatesPerMin[static_cast<size_t>(ResourceType::Wood)] += 0.15f;

            m_taxIncomePerMinute += def.baseTaxIncomePerMinute * b.residence.overallSatisfaction *
                                    (static_cast<float>(b.residence.currentInhabitants) / static_cast<float>(def.maxInhabitants));
        }

        if (def.production.outputPerMinute > 0.0f) {
            if (b.production.isWorking) {
                m_productionRatesPerMin[static_cast<size_t>(def.production.outputResource)] += def.production.outputPerMinute;
                if (def.production.inputPerMinute > 0.0f) {
                    m_consumptionRatesPerMin[static_cast<size_t>(def.production.inputResource)] += def.production.inputPerMinute;
                }
            }
        }
    }

    m_totalPopulation = totalPop;
    m_maxPopulation = maxPop;
    m_averageSatisfaction = (houseCount > 0) ? (sumSat / static_cast<float>(houseCount)) : 1.0f;
}

void CitySimulation::update(float deltaTime) {
    tickProduction(deltaTime);
    tickConsumption(deltaTime);
    tickCarriers(deltaTime);
    updateAggregateStats();
}

} // namespace engine::era
