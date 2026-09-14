#include "Simulation/CityCommands.hpp"
#include "Simulation/CitySystems.hpp"
#include "Simulation/Components.hpp"
#include "engine/world/World.hpp"

namespace engine::era {

// ----------------------------------------------------------------------------
// PlaceBuildingCmd
// ----------------------------------------------------------------------------

CmdResult validate(const World& world, const PlaceBuildingCmd& cmd) {
    if (cmd.type == BuildingType::None || static_cast<size_t>(cmd.type) >= kBuildingTypeCount) {
        return CmdResult::fail(CmdStatus::RequirementsNotMet, "Invalid building type");
    }

    if (!CitySystems::isInBounds(cmd.x, cmd.z)) {
        return CmdResult::fail(CmdStatus::InvalidPosition, "Position out of bounds");
    }

    const auto& grid = world.resource<GridIndex>();
    if (grid.cells[cmd.z][cmd.x].type != CellType::Empty) {
        return CmdResult::fail(CmdStatus::TileOccupied, "Tile is not empty");
    }

    const auto& state = world.resource<CityState>();
    const BuildingDef& def = getBuildingDef(cmd.type);
    if (static_cast<uint8_t>(def.requiredEra) > static_cast<uint8_t>(state.currentEra)) {
        return CmdResult::fail(CmdStatus::RequirementsNotMet, "Era requirement not met");
    }

    if (!state.storage.canAfford(def.cost)) {
        return CmdResult::fail(CmdStatus::CannotAfford, "Cannot afford building");
    }

    return CmdResult::success();
}

void apply(World& world, const PlaceBuildingCmd& cmd) {
    CitySystems::placeBuilding(world, cmd.x, cmd.z, cmd.type);
}

// ----------------------------------------------------------------------------
// DemolishBuildingCmd
// ----------------------------------------------------------------------------

CmdResult validate(const World& world, const DemolishBuildingCmd& cmd) {
    if (!CitySystems::isInBounds(cmd.x, cmd.z)) {
        return CmdResult::fail(CmdStatus::InvalidPosition, "Position out of bounds");
    }

    const auto& grid = world.resource<GridIndex>();
    const CellData& cell = grid.cells[cmd.z][cmd.x];
    if (cell.type == CellType::Empty) {
        return CmdResult::fail(CmdStatus::BuildingNotFound, "No building on tile");
    }

    const auto& registry = world.registry();
    entt::entity e = cell.entity;
    if (e == entt::null || !registry.valid(e)) {
        return CmdResult::fail(CmdStatus::BuildingNotFound, "Entity not found or invalid");
    }

    const auto& b = registry.get<BuildingComponent>(e);
    if (b.type == BuildingType::TownCenter) {
        return CmdResult::fail(CmdStatus::CannotDemolishTownCenter, "Cannot demolish Town Center");
    }

    return CmdResult::success();
}

void apply(World& world, const DemolishBuildingCmd& cmd) {
    CitySystems::demolishBuilding(world, cmd.x, cmd.z);
}

// ----------------------------------------------------------------------------
// EvolveEraCmd
// ----------------------------------------------------------------------------

CmdResult validate(const World& world, const EvolveEraCmd& /*cmd*/) {
    const auto& state = world.resource<CityState>();
    if (state.currentEra >= EraType::BronzeAge) {
        return CmdResult::fail(CmdStatus::MaxEraReached, "Already at maximum era");
    }

    const EraDefinition& eraDef = getEraDefinition(state.currentEra);
    if (state.totalPopulation < eraDef.requiredPopulation) {
        return CmdResult::fail(CmdStatus::RequirementsNotMet, "Insufficient population");
    }

    for (const auto& req : eraDef.evolutionRequirements) {
        if (state.storage.get(req.resource) < req.requiredAmount) {
            return CmdResult::fail(CmdStatus::CannotAfford, "Insufficient resources");
        }
    }

    return CmdResult::success();
}

void apply(World& world, const EvolveEraCmd& /*cmd*/) {
    CitySystems::evolveToNextEra(world);
}

// ----------------------------------------------------------------------------
// Registration
// ----------------------------------------------------------------------------

void registerCityCommands(CommandRegistry& registry) {
    registry.registerCommand<PlaceBuildingCmd>(&validate, &apply);
    registry.registerCommand<DemolishBuildingCmd>(&validate, &apply);
    registry.registerCommand<EvolveEraCmd>(&validate, &apply);
}

} // namespace engine::era
