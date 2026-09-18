#include <doctest/doctest.h>
#include "Simulation/CityCommands.hpp"
#include "Simulation/CitySystems.hpp"
#include "Simulation/Components.hpp"
#include "engine/world/World.hpp"

using namespace engine;
using namespace engine::era;

TEST_CASE("CityCommands - PlaceBuildingCmd") {
    World world;
    CitySystems::initCity(world);

    SUBCASE("Success - valid placement with sufficient resources") {
        PlaceBuildingCmd cmd{10, 10, BuildingIds::Residence};
        auto res = validate(world, cmd);
        CHECK(res.ok());
        CHECK(res.status == CmdStatus::Success);

        apply(world, cmd);

        const auto& grid = world.resource<GridIndex>();
        CHECK(grid.cells[10][10].type == CellType::Building);
        CHECK(world.registry().valid(grid.cells[10][10].entity));
    }

    SUBCASE("Failure - out of bounds position") {
        PlaceBuildingCmd cmdLow{-1, 5, BuildingIds::Residence};
        CHECK_FALSE(validate(world, cmdLow).ok());
        CHECK(validate(world, cmdLow).status == CmdStatus::InvalidPosition);

        PlaceBuildingCmd cmdHigh{32, 5, BuildingIds::Residence};
        CHECK_FALSE(validate(world, cmdHigh).ok());
        CHECK(validate(world, cmdHigh).status == CmdStatus::InvalidPosition);
    }

    SUBCASE("Failure - invalid building type") {
        PlaceBuildingCmd cmd{5, 5, BuildingIds::None};
        auto res = validate(world, cmd);
        CHECK_FALSE(res.ok());
        CHECK(res.status == CmdStatus::RequirementsNotMet);
    }

    SUBCASE("Failure - era requirement not met") {
        // StoneQuarry requires BronzeAge, but initial city is StoneAge
        PlaceBuildingCmd cmd{5, 5, BuildingIds::StoneQuarry};
        auto res = validate(world, cmd);
        CHECK_FALSE(res.ok());
        CHECK(res.status == CmdStatus::RequirementsNotMet);
    }

    SUBCASE("Failure - tile already occupied") {
        // TownCenter is pre-placed at (15, 15)
        PlaceBuildingCmd cmd{15, 15, BuildingIds::Residence};
        auto res = validate(world, cmd);
        CHECK_FALSE(res.ok());
        CHECK(res.status == CmdStatus::TileOccupied);
    }

    SUBCASE("Failure - cannot afford building") {
        auto& state = world.resource<CityState>();
        state.storage.clear(); // Empty all resources

        PlaceBuildingCmd cmd{5, 5, BuildingIds::Residence};
        auto res = validate(world, cmd);
        CHECK_FALSE(res.ok());
        CHECK(res.status == CmdStatus::CannotAfford);
    }
}

TEST_CASE("CityCommands - DemolishBuildingCmd") {
    World world;
    CitySystems::initCity(world);

    SUBCASE("Success - demolish existing non-hub building") {
        PlaceBuildingCmd placeCmd{10, 10, BuildingIds::Residence};
        apply(world, placeCmd);

        const auto& grid = world.resource<GridIndex>();
        CHECK(grid.cells[10][10].type == CellType::Building);

        DemolishBuildingCmd demCmd{10, 10};
        auto res = validate(world, demCmd);
        CHECK(res.ok());
        CHECK(res.status == CmdStatus::Success);

        apply(world, demCmd);

        CHECK(grid.cells[10][10].type == CellType::Empty);
        CHECK(grid.cells[10][10].entity == entt::entity{entt::null});
    }

    SUBCASE("Failure - out of bounds") {
        DemolishBuildingCmd cmd{-1, 10};
        auto res = validate(world, cmd);
        CHECK_FALSE(res.ok());
        CHECK(res.status == CmdStatus::InvalidPosition);
    }

    SUBCASE("Failure - empty tile has no building") {
        DemolishBuildingCmd cmd{5, 5};
        auto res = validate(world, cmd);
        CHECK_FALSE(res.ok());
        CHECK(res.status == CmdStatus::TargetNotFound);
    }

    SUBCASE("Failure - cannot demolish TownCenter") {
        // TownCenter is at (15, 15)
        DemolishBuildingCmd cmd{15, 15};
        auto res = validate(world, cmd);
        CHECK_FALSE(res.ok());
        CHECK(res.status == CmdStatus::ProtectedEntity);
    }
}

TEST_CASE("CityCommands - EvolveEraCmd") {
    World world;
    CitySystems::initCity(world);

    SUBCASE("Failure - insufficient population") {
        auto& state = world.resource<CityState>();
        state.totalPopulation = 2; // BronzeAge needs 12
        state.storage.set(ResourceIds::Wood, 100.0f);
        state.storage.set(ResourceIds::Fish, 100.0f);

        EvolveEraCmd cmd{};
        auto res = validate(world, cmd);
        CHECK_FALSE(res.ok());
        CHECK(res.status == CmdStatus::RequirementsNotMet);
    }

    SUBCASE("Failure - insufficient resources") {
        auto& state = world.resource<CityState>();
        const auto& eraDef = getEraDefinition(state.currentEra);
        state.totalPopulation = eraDef.requiredPopulation + 5;
        state.storage.clear(); // No resources

        EvolveEraCmd cmd{};
        auto res = validate(world, cmd);
        CHECK_FALSE(res.ok());
        CHECK(res.status == CmdStatus::CannotAfford);
    }

    SUBCASE("Success - requirements met and evolution") {
        auto& state = world.resource<CityState>();
        const auto& eraDef = getEraDefinition(state.currentEra);
        state.totalPopulation = eraDef.requiredPopulation + 2;
        for (const auto& req : eraDef.evolutionRequirements) {
            state.storage.set(req.resource, req.requiredAmount + 20.0f);
        }

        EvolveEraCmd cmd{};
        auto res = validate(world, cmd);
        CHECK(res.ok());
        CHECK(res.status == CmdStatus::Success);

        apply(world, cmd);
        CHECK(state.currentEra == EraType::BronzeAge);

        // Subcase: Failure - already at max era
        auto secondRes = validate(world, cmd);
        CHECK_FALSE(secondRes.ok());
        CHECK(secondRes.status == CmdStatus::MaxEraReached);
    }
}

TEST_CASE("CityCommands - Queue and Dispatch Integration") {
    World world;
    CitySystems::initCity(world);
    registerCityCommands(world.commands());

    CHECK(world.commands().isRegistered<PlaceBuildingCmd>());
    CHECK(world.commands().isRegistered<DemolishBuildingCmd>());
    CHECK(world.commands().isRegistered<EvolveEraCmd>());

    // Enqueue 1 valid place, 1 invalid place, 1 invalid demolish
    world.commandQueue().enqueue(PlaceBuildingCmd{12, 12, BuildingIds::Residence});
    world.commandQueue().enqueue(PlaceBuildingCmd{-1, 0, BuildingIds::Residence});
    world.commandQueue().enqueue(DemolishBuildingCmd{5, 5}); // empty

    CHECK(world.commandQueue().size() == 3);

    world.dispatchCommands();

    CHECK(world.commandQueue().empty());

    const auto& grid = world.resource<GridIndex>();
    CHECK(grid.cells[12][12].type == CellType::Building);
}
