#include "engine/world/WorldHasher.hpp"
#include "Simulation/CitySystems.hpp"
#include "Simulation/Components.hpp"
#include "Simulation/CityEvents.hpp"
#include "engine/foundation/EventBus.hpp"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <algorithm>
#include <cmath>

namespace engine::era::CitySystems {

static void syncCarrierPool(World& world) {
    auto& state = world.resource<CityState>();
    auto& pool = world.resource<CarrierPool>();
    auto& registry = world.registry();
    auto& events = world.events();

    const EraDefinition& eraDef = getEraDefinition(state.currentEra);
    const size_t targetCount = static_cast<size_t>(eraDef.carrierPoolSize);

    while (pool.totalCarrierCount < targetCount) {
        entt::entity e = registry.create();
        
        auto& c = registry.emplace<CarrierComponent>(e);
        c.state = CarrierState::IdleAtWarehouse;
        c.hasCargo = false;
        c.carriedAmount = 0.0f;
        c.carriedResource = ResourceType::Wood;
        c.targetBuilding = entt::null;
        
        const glm::vec3 tcPos = getTownCenterPosition(world);
        const float radius = 0.75f;
        const float angle = (static_cast<float>(pool.totalCarrierCount) / static_cast<float>(std::max<size_t>(1, targetCount))) * 6.283185f;
        glm::vec3 restPos = {tcPos.x + std::cos(angle) * radius, 0.0f, tcPos.z + std::sin(angle) * radius};

        auto& j = registry.emplace<CarrierJourneyComponent>(e);
        j.currentPos = restPos;
        j.startPos = restPos;
        j.targetPos = restPos;
        j.progress = 0.0f;

        pool.totalCarrierCount++;

        events.enqueue<CarrierSpawnedEvent>({e});
    }
}

glm::vec3 getTownCenterPosition(const World& world) {
    auto view = world.registry().view<const BuildingComponent, const GridPosition>();
    for (auto [e, b, pos] : view.each()) {
        if (b.type == BuildingType::TownCenter) {
            return {static_cast<float>(pos.x) + 0.5f, 0.0f, static_cast<float>(pos.z) + 0.5f};
        }
    }
    return {15.5f, 0.0f, 15.5f};
}

static glm::vec3 getIdleCarrierRestPos(const World& world, size_t index, size_t total) {
    const glm::vec3 tcPos = getTownCenterPosition(world);
    const float radius = 0.75f;
    const float angle = (static_cast<float>(index) / static_cast<float>(std::max<size_t>(1, total))) * 6.283185f;
    return {tcPos.x + std::cos(angle) * radius, 0.0f, tcPos.z + std::sin(angle) * radius};
}

static float computeTripDuration(const World& world, const glm::vec3& from, const glm::vec3& to) {
    const float dist = glm::distance(from, to);
    auto& state = world.resource<CityState>();
    const CarrierDef cDef = getCarrierDefForEra(state.currentEra);
    float speed = cDef.speed;

    const int midX = static_cast<int>((from.x + to.x) * 0.5f);
    const int midZ = static_cast<int>((from.z + to.z) * 0.5f);
    if (isInBounds(midX, midZ)) {
        auto& grid = world.resource<GridIndex>();
        if (grid.getCell(midX, midZ).type == CellType::Road) {
            speed *= cDef.roadSpeedMultiplier;
        }
    }

    return std::max(1.2f, dist / std::max(0.5f, speed));
}

void registerSimulationTypes(World& /*world*/) {
    WorldHasher::registerComponent<BuildingComponent>("BuildingComponent", [](const BuildingComponent& b, uint64_t& h) {
        WorldHasher::hashPod(h, b.type);
    });

    WorldHasher::registerComponent<GridPosition>("GridPosition", [](const GridPosition& p, uint64_t& h) {
        WorldHasher::hashPod(h, p.x);
        WorldHasher::hashPod(h, p.z);
    });

    WorldHasher::registerComponent<ResidenceComponent>("ResidenceComponent", [](const ResidenceComponent& r, uint64_t& h) {
        WorldHasher::hashPod(h, r.currentInhabitants);
        WorldHasher::hashPod(h, r.maxInhabitants);
        WorldHasher::hashPod(h, r.foodSatisfaction);
        WorldHasher::hashPod(h, r.warmthSatisfaction);
        WorldHasher::hashPod(h, r.overallSatisfaction);
        WorldHasher::hashPod(h, r.consumptionTimer);
    });

    WorldHasher::registerComponent<ProductionComponent>("ProductionComponent", [](const ProductionComponent& p, uint64_t& h) {
        WorldHasher::hashPod(h, p.progress);
        WorldHasher::hashPod(h, p.cycleSeconds);
        WorldHasher::hashPod(h, p.isWorking);
        WorldHasher::hashPod(h, p.isBufferFull);
        WorldHasher::hashPod(h, p.internalBuffer);
        WorldHasher::hashPod(h, p.maxBuffer);
        WorldHasher::hashPod(h, p.hasCourierAssigned);
        WorldHasher::hashPod(h, p.currentAlert);
    });

    WorldHasher::registerComponent<CarrierComponent>("CarrierComponent", [](const CarrierComponent& c, uint64_t& h) {
        WorldHasher::hashPod(h, c.state);
        WorldHasher::hashPod(h, static_cast<uint32_t>(entt::to_integral(c.targetBuilding)));
        WorldHasher::hashPod(h, c.carriedResource);
        WorldHasher::hashPod(h, c.carriedAmount);
        WorldHasher::hashPod(h, c.hasCargo);
    });

    WorldHasher::registerComponent<CarrierJourneyComponent>("CarrierJourneyComponent", [](const CarrierJourneyComponent& j, uint64_t& h) {
        WorldHasher::hashPod(h, j.currentPos);
        WorldHasher::hashPod(h, j.startPos);
        WorldHasher::hashPod(h, j.targetPos);
        WorldHasher::hashPod(h, j.progress);
        WorldHasher::hashPod(h, j.tripDuration);
        WorldHasher::hashPod(h, j.bobbingTimer);
    });

    WorldHasher::registerResource<CityState>("CityState", [](const CityState& s, uint64_t& h) {
        for (float a : s.storage.amounts) {
            WorldHasher::hashPod(h, a);
        }
        WorldHasher::hashPod(h, s.currentEra);
        WorldHasher::hashPod(h, s.totalPopulation);
        WorldHasher::hashPod(h, s.maxPopulation);
        WorldHasher::hashPod(h, s.averageSatisfaction);
        WorldHasher::hashPod(h, s.taxIncomePerMinute);
        for (float p : s.productionRatesPerMin) {
            WorldHasher::hashPod(h, p);
        }
        for (float c : s.consumptionRatesPerMin) {
            WorldHasher::hashPod(h, c);
        }
    });

    WorldHasher::registerResource<CarrierPool>("CarrierPool", [](const CarrierPool& p, uint64_t& h) {
        WorldHasher::hashPod(h, p.activeCarrierCount);
        WorldHasher::hashPod(h, p.totalCarrierCount);
    });

    WorldHasher::registerResource<GridIndex>("GridIndex", [](const GridIndex& g, uint64_t& h) {
        for (int z = 0; z < GridIndex::kGridSize; ++z) {
            for (int x = 0; x < GridIndex::kGridSize; ++x) {
                const auto& cell = g.cells[z][x];
                WorldHasher::hashPod(h, cell.type);
                WorldHasher::hashPod(h, static_cast<uint32_t>(entt::to_integral(cell.entity)));
            }
        }
    });
}

void registerCityTypes(TypeRegistry& types) {
    types.registerComponent<BuildingComponent>("BuildingComponent", 1)
        .field("type", &BuildingComponent::type);

    types.registerComponent<GridPosition>("GridPosition", 1)
        .field("x", &GridPosition::x)
        .field("z", &GridPosition::z);

    types.registerComponent<ResidenceComponent>("ResidenceComponent", 1)
        .field("currentInhabitants", &ResidenceComponent::currentInhabitants)
        .field("maxInhabitants", &ResidenceComponent::maxInhabitants)
        .field("foodSatisfaction", &ResidenceComponent::foodSatisfaction)
        .field("warmthSatisfaction", &ResidenceComponent::warmthSatisfaction)
        .field("overallSatisfaction", &ResidenceComponent::overallSatisfaction)
        .field("consumptionTimer", &ResidenceComponent::consumptionTimer);

    types.registerComponent<ProductionComponent>("ProductionComponent", 1)
        .field("progress", &ProductionComponent::progress)
        .field("cycleSeconds", &ProductionComponent::cycleSeconds)
        .field("isWorking", &ProductionComponent::isWorking)
        .field("isBufferFull", &ProductionComponent::isBufferFull)
        .field("internalBuffer", &ProductionComponent::internalBuffer)
        .field("maxBuffer", &ProductionComponent::maxBuffer)
        .field("hasCourierAssigned", &ProductionComponent::hasCourierAssigned)
        .field("currentAlert", &ProductionComponent::currentAlert);

    types.registerComponent<CarrierComponent>("CarrierComponent", 1)
        .field("state", &CarrierComponent::state)
        .field("targetBuilding", &CarrierComponent::targetBuilding)
        .field("carriedResource", &CarrierComponent::carriedResource)
        .field("carriedAmount", &CarrierComponent::carriedAmount)
        .field("hasCargo", &CarrierComponent::hasCargo);

    types.registerComponent<CarrierJourneyComponent>("CarrierJourneyComponent", 1)
        .field("currentPos", &CarrierJourneyComponent::currentPos)
        .field("startPos", &CarrierJourneyComponent::startPos)
        .field("targetPos", &CarrierJourneyComponent::targetPos)
        .field("progress", &CarrierJourneyComponent::progress)
        .field("tripDuration", &CarrierJourneyComponent::tripDuration)
        .field("bobbingTimer", &CarrierJourneyComponent::bobbingTimer);
}

void initCity(World& world) {
    registerSimulationTypes(world);
    registerCityTypes(world.types());
    if(!world.hasResource<CityState>()) {
        world.emplaceResource<CityState>();
    }
    if(!world.hasResource<GridIndex>()) {
        world.emplaceResource<GridIndex>();
    }
    if(!world.hasResource<CarrierPool>()) {
        world.emplaceResource<CarrierPool>();
    }

    auto& state = world.resource<CityState>();
    state.storage.set(ResourceType::Wood, 28.0f);
    state.storage.set(ResourceType::Fish, 20.0f);
    state.storage.set(ResourceType::Stone, 10.0f);
    state.storage.set(ResourceType::Grain, 0.0f);
    state.storage.set(ResourceType::Bread, 0.0f);
    state.storage.set(ResourceType::Gold, 25.0f);

    placeBuilding(world, 15, 15, BuildingType::TownCenter);
    syncCarrierPool(world);
}

bool isInBounds(int x, int z) {
    return (x >= 0 && x < GridIndex::kGridSize && z >= 0 && z < GridIndex::kGridSize);
}

bool canAfford(const World& world, BuildingType type) {
    const BuildingDef& def = getBuildingDef(type);
    auto& state = world.resource<CityState>();
    return state.storage.canAfford(def.cost);
}

float getNetRatePerMinute(const World& world, ResourceType type) {
    auto& state = world.resource<CityState>();
    const size_t idx = static_cast<size_t>(type);
    return state.productionRatesPerMin[idx] - state.consumptionRatesPerMin[idx];
}

bool canEvolve(const World& world) {
    auto& state = world.resource<CityState>();
    if (state.currentEra != EraType::StoneAge) {
        return false;
    }

    const EraDefinition& eraDef = getEraDefinition(state.currentEra);
    if (state.totalPopulation < eraDef.requiredPopulation) {
        return false;
    }

    for (const auto& req : eraDef.evolutionRequirements) {
        if (state.storage.get(req.resource) < req.requiredAmount) {
            return false;
        }
    }
    return true;
}

bool evolveToNextEra(World& world) {
    if (!canEvolve(world)) return false;

    auto& state = world.resource<CityState>();
    const EraDefinition& eraDef = getEraDefinition(state.currentEra);
    for (const auto& req : eraDef.evolutionRequirements) {
        state.storage.add(req.resource, -req.requiredAmount);
    }

    state.currentEra = EraType::BronzeAge;

    auto view = world.registry().view<BuildingComponent, ResidenceComponent>();
    for (auto [e, b, res] : view.each()) {
        if (b.type == BuildingType::Residence) {
            res.maxInhabitants = 8;
            res.currentInhabitants = std::min(8, res.currentInhabitants + 2);
        }
    }

    syncCarrierPool(world);
    updateAggregateStats(world);

    world.events().enqueue<EraEvolvedEvent>({state.currentEra});
    return true;
}

bool placeBuilding(World& world, int x, int z, BuildingType type) {
    if (!isInBounds(x, z) || type == BuildingType::None) return false;

    auto& grid = world.resource<GridIndex>();
    if (grid.cells[z][x].type != CellType::Empty) return false;

    auto& state = world.resource<CityState>();
    const BuildingDef& def = getBuildingDef(type);
    if (!state.storage.tryConsume(def.cost)) return false;

    auto& registry = world.registry();
    entt::entity e = registry.create();
    
    auto& b = registry.emplace<BuildingComponent>(e);
    b.type = type;
    
    auto& pos = registry.emplace<GridPosition>(e);
    pos.x = x;
    pos.z = z;

    if (type == BuildingType::Residence) {
        auto& res = registry.emplace<ResidenceComponent>(e);
        res.currentInhabitants = 2;
        res.maxInhabitants = (state.currentEra == EraType::BronzeAge) ? 8 : def.maxInhabitants;
    }

    if (def.production.outputPerMinute > 0.0f) {
        auto& prod = registry.emplace<ProductionComponent>(e);
        prod.cycleSeconds = def.production.cycleSeconds;
    }

    grid.cells[z][x].type = (type == BuildingType::Road) ? CellType::Road : CellType::Building;
    grid.cells[z][x].entity = e;

    world.events().enqueue<BuildingPlacedEvent>({e, x, z, type});
    
    updateAggregateStats(world);
    return true;
}

bool demolishBuilding(World& world, int x, int z) {
    if (!isInBounds(x, z)) return false;

    auto& grid = world.resource<GridIndex>();
    CellData& cell = grid.cells[z][x];
    if (cell.type == CellType::Empty) return false;

    auto& registry = world.registry();
    entt::entity e = cell.entity;
    if (e == entt::null || !registry.valid(e)) return false;

    const BuildingComponent& b = registry.get<BuildingComponent>(e);
    if (b.type == BuildingType::TownCenter) return false;

    // Refund 50%
    auto& state = world.resource<CityState>();
    const BuildingDef& def = getBuildingDef(b.type);
    for (size_t i = 0; i < kResourceCount; ++i) {
        state.storage.amounts[i] += def.cost.amounts[i] * 0.5f;
    }

    // Cancel carriers
    const glm::vec3 tcPos = getTownCenterPosition(world);
    auto carrierView = registry.view<CarrierComponent, CarrierJourneyComponent>();
    for (auto [ce, c, j] : carrierView.each()) {
        if (c.targetBuilding == e && c.state == CarrierState::EnRouteToPickup) {
            c.state = CarrierState::ReturningToWarehouse;
            j.startPos = j.currentPos;
            j.targetPos = tcPos;
            j.progress = 0.0f;
            j.tripDuration = computeTripDuration(world, j.startPos, j.targetPos);
            c.targetBuilding = entt::null;
            c.hasCargo = false;
            c.carriedAmount = 0.0f;
        }
    }

    if (auto* prod = registry.try_get<ProductionComponent>(e)) {
        if (prod->currentAlert != BuildingAlertKind::None) {
            world.events().enqueue<BuildingStatusEvent>({
                .entity = e,
                .buildingType = b.type,
                .alert = prod->currentAlert,
                .active = false,
                .gridX = x,
                .gridZ = z
            });
        }
    }

    world.events().enqueue<BuildingRemovedEvent>({e, x, z});
    registry.destroy(e);
    cell = CellData{};

    updateAggregateStats(world);
    return true;
}

void tickProduction(World& world, Tick /*tick*/) {
    const float dt = Tick::dt;
    world.registry().sort<BuildingComponent>([](entt::entity a, entt::entity b) {
        return entt::to_integral(a) < entt::to_integral(b);
    });
    auto& state = world.resource<CityState>();
    auto view = world.registry().view<BuildingComponent, GridPosition, ProductionComponent>();

    for (auto [e, b, pos, prod] : view.each()) {
        const BuildingDef& def = getBuildingDef(b.type);
        if (def.production.outputPerMinute <= 0.0f) continue;

        if (prod.internalBuffer >= prod.maxBuffer) {
            prod.isBufferFull = true;
            prod.isWorking = false;

            if (prod.currentAlert != BuildingAlertKind::StorageFull) {
                prod.currentAlert = BuildingAlertKind::StorageFull;
                world.events().enqueue<BuildingStatusEvent>({
                    .entity = e,
                    .buildingType = b.type,
                    .alert = BuildingAlertKind::StorageFull,
                    .active = true,
                    .gridX = pos.x,
                    .gridZ = pos.z,
                    .resource = def.production.outputResource,
                    .currentBuffer = prod.internalBuffer,
                    .maxBuffer = prod.maxBuffer
                });
            }
            continue;
        }

        const float cycle = std::max(0.5f, def.production.cycleSeconds);
        const float inputPerCycle = (def.production.inputPerMinute / 60.0f) * cycle;
        const float outputPerCycle = (def.production.outputPerMinute / 60.0f) * cycle;

        if (def.production.inputPerMinute > 0.0f) {
            if (state.storage.get(def.production.inputResource) < inputPerCycle) {
                prod.isWorking = false;
                if (prod.currentAlert != BuildingAlertKind::MissingInput) {
                    prod.currentAlert = BuildingAlertKind::MissingInput;
                    world.events().enqueue<BuildingStatusEvent>({
                        .entity = e,
                        .buildingType = b.type,
                        .alert = BuildingAlertKind::MissingInput,
                        .active = true,
                        .gridX = pos.x,
                        .gridZ = pos.z,
                        .resource = def.production.inputResource,
                        .currentBuffer = prod.internalBuffer,
                        .maxBuffer = prod.maxBuffer
                    });
                }
                continue;
            } else if (prod.currentAlert == BuildingAlertKind::MissingInput) {
                prod.currentAlert = BuildingAlertKind::None;
                world.events().enqueue<BuildingStatusEvent>({
                    .entity = e,
                    .buildingType = b.type,
                    .alert = BuildingAlertKind::MissingInput,
                    .active = false,
                    .gridX = pos.x,
                    .gridZ = pos.z,
                    .resource = def.production.inputResource,
                    .currentBuffer = prod.internalBuffer,
                    .maxBuffer = prod.maxBuffer
                });
            }
        }

        prod.isWorking = true;
        prod.isBufferFull = false;
        prod.progress += dt;

        if (prod.progress >= cycle) {
            prod.progress -= cycle;

            if (def.production.inputPerMinute > 0.0f) {
                state.storage.add(def.production.inputResource, -inputPerCycle);
            }

            prod.internalBuffer = std::min(prod.maxBuffer, prod.internalBuffer + outputPerCycle);
            if (prod.internalBuffer >= prod.maxBuffer) {
                prod.isBufferFull = true;
                prod.isWorking = false;

                if (prod.currentAlert != BuildingAlertKind::StorageFull) {
                    prod.currentAlert = BuildingAlertKind::StorageFull;
                    world.events().enqueue<BuildingStatusEvent>({
                        .entity = e,
                        .buildingType = b.type,
                        .alert = BuildingAlertKind::StorageFull,
                        .active = true,
                        .gridX = pos.x,
                        .gridZ = pos.z,
                        .resource = def.production.outputResource,
                        .currentBuffer = prod.internalBuffer,
                        .maxBuffer = prod.maxBuffer
                    });
                }
            }
        }
    }
}

void tickConsumption(World& world, Tick /*tick*/) {
    const float dt = Tick::dt;
    world.registry().sort<ResidenceComponent>([](entt::entity a, entt::entity b) {
        return entt::to_integral(a) < entt::to_integral(b);
    });
    constexpr float kConsumptionInterval = 2.0f;
    auto& state = world.resource<CityState>();
    auto view = world.registry().view<BuildingComponent, ResidenceComponent>();

    for (auto [e, b, res] : view.each()) {
        if (b.type != BuildingType::Residence) continue;

        res.consumptionTimer += dt;
        if (res.consumptionTimer >= kConsumptionInterval) {
            res.consumptionTimer -= kConsumptionInterval;
            const float dtFactor = kConsumptionInterval / 60.0f;

            const float fishNeeded = 0.20f * dtFactor;
            if (state.storage.get(ResourceType::Fish) >= fishNeeded) {
                state.storage.add(ResourceType::Fish, -fishNeeded);
                res.foodSatisfaction = std::min(1.0f, res.foodSatisfaction + 0.10f);
            } else {
                res.foodSatisfaction = std::max(0.0f, res.foodSatisfaction - 0.15f);
            }

            const float woodNeeded = 0.15f * dtFactor;
            if (state.storage.get(ResourceType::Wood) >= woodNeeded) {
                state.storage.add(ResourceType::Wood, -woodNeeded);
                res.warmthSatisfaction = std::min(1.0f, res.warmthSatisfaction + 0.10f);
            } else {
                res.warmthSatisfaction = std::max(0.0f, res.warmthSatisfaction - 0.15f);
            }

            res.overallSatisfaction = (res.foodSatisfaction + res.warmthSatisfaction) * 0.5f;

            if (res.overallSatisfaction >= 0.75f && res.currentInhabitants < res.maxInhabitants) {
                res.currentInhabitants += 1;
            } else if (res.overallSatisfaction < 0.25f && res.currentInhabitants > 1) {
                res.currentInhabitants -= 1;
            }

            const BuildingDef& def = getBuildingDef(b.type);
            const float tax = (def.baseTaxIncomePerMinute / 60.0f) * kConsumptionInterval *
                              res.overallSatisfaction *
                              (static_cast<float>(res.currentInhabitants) / static_cast<float>(res.maxInhabitants));
            state.storage.add(ResourceType::Gold, tax);
        }
    }
}

void tickCarriers(World& world, Tick /*tick*/) {
    const float dt = Tick::dt;
    world.registry().sort<CarrierComponent>([](entt::entity a, entt::entity b) {
        return entt::to_integral(a) < entt::to_integral(b);
    });
    auto& state = world.resource<CityState>();
    auto& pool = world.resource<CarrierPool>();
    const CarrierDef cDef = getCarrierDefForEra(state.currentEra);
    const glm::vec3 tcPos = getTownCenterPosition(world);
    auto& registry = world.registry();
    
    auto bView = registry.view<BuildingComponent, GridPosition, ProductionComponent>();
    auto cView = registry.view<CarrierComponent, CarrierJourneyComponent>();

    size_t i = 0;
    for (auto [ce, c, j] : cView.each()) {
        if (c.state == CarrierState::IdleAtWarehouse) {
            entt::entity bestCandidate = entt::null;
            float highestFillRatio = -1.0f;
            glm::vec3 bestPos{0.0f};

            for (auto [be, b, pos, prod] : bView.each()) {
                const BuildingDef& bDef = getBuildingDef(b.type);
                if (bDef.production.outputPerMinute <= 0.0f) continue;
                if (prod.internalBuffer < 1.0f) continue;
                if (prod.hasCourierAssigned) continue;

                const float fillRatio = prod.internalBuffer / std::max(0.1f, prod.maxBuffer);
                if (fillRatio > highestFillRatio) {
                    highestFillRatio = fillRatio;
                    bestCandidate = be;
                    bestPos = {static_cast<float>(pos.x) + 0.5f, 0.0f, static_cast<float>(pos.z) + 0.5f};
                }
            }

            if (bestCandidate != entt::null) {
                auto& prod = registry.get<ProductionComponent>(bestCandidate);
                prod.hasCourierAssigned = true;
                
                c.state = CarrierState::EnRouteToPickup;
                c.targetBuilding = bestCandidate;
                c.hasCargo = false;
                c.carriedAmount = 0.0f;
                
                j.startPos = j.currentPos;
                j.targetPos = bestPos;
                j.progress = 0.0f;
                j.tripDuration = computeTripDuration(world, j.startPos, j.targetPos);
            } else {
                j.currentPos = getIdleCarrierRestPos(world, i, pool.totalCarrierCount);
            }
        } else {
            j.bobbingTimer += dt * 10.0f;
            j.progress += dt / std::max(0.1f, j.tripDuration);
            j.currentPos = glm::mix(j.startPos, j.targetPos, std::min(1.0f, j.progress));

            if (c.state == CarrierState::EnRouteToPickup) {
                if (!registry.valid(c.targetBuilding)) {
                    c.state = CarrierState::ReturningToWarehouse;
                    c.targetBuilding = entt::null;
                    j.startPos = j.currentPos;
                    j.targetPos = tcPos;
                    j.progress = 0.0f;
                    j.tripDuration = computeTripDuration(world, j.startPos, j.targetPos);
                } else if (j.progress >= 1.0f) {
                    auto& b = registry.get<BuildingComponent>(c.targetBuilding);
                    auto& pos = registry.get<GridPosition>(c.targetBuilding);
                    auto& prod = registry.get<ProductionComponent>(c.targetBuilding);
                    const BuildingDef& bDef = getBuildingDef(b.type);
                    const float pickupAmount = std::min(static_cast<float>(cDef.capacity), prod.internalBuffer);

                    prod.internalBuffer = std::max(0.0f, prod.internalBuffer - pickupAmount);
                    prod.isBufferFull = (prod.internalBuffer >= prod.maxBuffer);
                    prod.hasCourierAssigned = false;

                    if (prod.currentAlert == BuildingAlertKind::StorageFull && !prod.isBufferFull) {
                        prod.currentAlert = BuildingAlertKind::None;
                        world.events().enqueue<BuildingStatusEvent>({
                            .entity = c.targetBuilding,
                            .buildingType = b.type,
                            .alert = BuildingAlertKind::StorageFull,
                            .active = false,
                            .gridX = pos.x,
                            .gridZ = pos.z,
                            .resource = bDef.production.outputResource,
                            .currentBuffer = prod.internalBuffer,
                            .maxBuffer = prod.maxBuffer
                        });
                    }

                    c.carriedResource = bDef.production.outputResource;
                    c.carriedAmount = pickupAmount;
                    c.hasCargo = (pickupAmount > 0.0f);

                    c.state = CarrierState::ReturningToWarehouse;
                    j.startPos = j.currentPos;
                    j.targetPos = tcPos;
                    j.progress = 0.0f;
                    j.tripDuration = computeTripDuration(world, j.startPos, j.targetPos);
                }
            } else if (c.state == CarrierState::ReturningToWarehouse) {
                if (j.progress >= 1.0f) {
                    if (c.hasCargo && c.carriedAmount > 0.0f) {
                        state.storage.add(c.carriedResource, c.carriedAmount);
                    }

                    c.state = CarrierState::IdleAtWarehouse;
                    c.targetBuilding = entt::null;
                    c.hasCargo = false;
                    c.carriedAmount = 0.0f;
                    j.progress = 0.0f;
                    j.currentPos = getIdleCarrierRestPos(world, i, pool.totalCarrierCount);
                }
            }
        }
        i++;
    }
}

void updateAggregateStats(World& world) {
    auto& state = world.resource<CityState>();
    int totalPop = 0;
    int maxPop = 0;
    float sumSat = 0.0f;
    int houseCount = 0;

    state.productionRatesPerMin.fill(0.0f);
    state.consumptionRatesPerMin.fill(0.0f);
    state.taxIncomePerMinute = 0.0f;

    auto bView = world.registry().view<BuildingComponent>();
    for (auto e : bView) {
        const auto& b = bView.get<BuildingComponent>(e);
        const BuildingDef& def = getBuildingDef(b.type);

        if (b.type == BuildingType::Residence) {
            if(auto* res = world.registry().try_get<ResidenceComponent>(e)) {
                totalPop += res->currentInhabitants;
                maxPop += res->maxInhabitants;
                sumSat += res->overallSatisfaction;
                houseCount++;

                state.consumptionRatesPerMin[static_cast<size_t>(ResourceType::Fish)] += 0.20f;
                state.consumptionRatesPerMin[static_cast<size_t>(ResourceType::Wood)] += 0.15f;
                state.taxIncomePerMinute += def.baseTaxIncomePerMinute * res->overallSatisfaction *
                                            (static_cast<float>(res->currentInhabitants) / static_cast<float>(def.maxInhabitants));
            }
        }

        if (def.production.outputPerMinute > 0.0f) {
            if(auto* prod = world.registry().try_get<ProductionComponent>(e)) {
                if (prod->isWorking) {
                    state.productionRatesPerMin[static_cast<size_t>(def.production.outputResource)] += def.production.outputPerMinute;
                    if (def.production.inputPerMinute > 0.0f) {
                        state.consumptionRatesPerMin[static_cast<size_t>(def.production.inputResource)] += def.production.inputPerMinute;
                    }
                }
            }
        }
    }

    state.totalPopulation = totalPop;
    state.maxPopulation = maxPop;
    state.averageSatisfaction = (houseCount > 0) ? (sumSat / static_cast<float>(houseCount)) : 1.0f;
}

void update(World& world, Tick tick) {
    tickProduction(world, tick);
    tickConsumption(world, tick);
    tickCarriers(world, tick);
    updateAggregateStats(world);
}

} // namespace engine::era::CitySystems
