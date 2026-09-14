#pragma once

#include "engine/world/World.hpp"
#include "Simulation/EraData.hpp"
#include <glm/vec3.hpp>
#include <cstdint>

namespace engine::era::CitySystems {

// Initialize the city state (e.g. at map creation or module attach)
void initCity(World& world);
void registerSimulationTypes(World& world);

// Core simulation loops (strictly taking Tick for determinism)
void tickProduction(World& world, Tick tick);
void tickConsumption(World& world, Tick tick);
void tickCarriers(World& world, Tick tick);
void updateAggregateStats(World& world);

// Master update function
void update(World& world, Tick tick);

// Commands
bool placeBuilding(World& world, int x, int z, BuildingType type);
bool demolishBuilding(World& world, int x, int z);
bool evolveToNextEra(World& world);

// Queries
bool isInBounds(int x, int z);
bool canAfford(const World& world, BuildingType type);
bool canEvolve(const World& world);

glm::vec3 getTownCenterPosition(const World& world);
float getNetRatePerMinute(const World& world, ResourceType type);

} // namespace engine::era::CitySystems
