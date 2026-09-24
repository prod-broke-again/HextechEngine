#include <doctest/doctest.h>
#include <cstdlib>
#include "Simulation/Pathfinder.hpp"
#include "Simulation/CitySystems.hpp"
#include "Simulation/CityCommands.hpp"
#include "Simulation/Components.hpp"
#include "engine/world/World.hpp"

using namespace engine;
using namespace engine::era;

TEST_CASE("Pathfinder - orthogonal steps only") {
    GridIndex grid;
    CarrierPath path;
    REQUIRE(findCarrierPath(grid, {2, 2}, {5, 4}, path));
    REQUIRE(path.length >= 2);

    for (int i = 1; i < path.length; ++i) {
        const int dx = std::abs(path.cells[i].x - path.cells[i - 1].x);
        const int dz = std::abs(path.cells[i].z - path.cells[i - 1].z);
        CHECK(dx + dz == 1);
        CHECK(dx * dz == 0);
    }
}

TEST_CASE("Pathfinder - prefers a paved detour over dirt") {
    GridIndex grid;
    // Straight dirt: (0,0) -> (4,0) costs 3*4 = 12
    // Road along z=1 then back: longer but cheaper if most cells are road
    for (int x = 0; x <= 4; ++x) {
        grid.at(x, 1).type = CellType::Road;
    }
    grid.at(4, 0).type = CellType::Empty;

    CarrierPath path;
    REQUIRE(findCarrierPath(grid, {0, 0}, {4, 0}, path));

    bool usedRoad = false;
    for (int i = 0; i < path.length; ++i) {
        if (path.cells[i].z == 1) {
            usedRoad = true;
        }
    }
    CHECK(usedRoad);
}

TEST_CASE("Pathfinder - reuses a worn trail instead of a parallel row") {
    GridIndex grid;
    grid.at(1, 0).type = CellType::Trail;
    grid.at(2, 0).type = CellType::Trail;
    grid.at(3, 0).type = CellType::Trail;

    CarrierPath path;
    REQUIRE(findCarrierPath(grid, {4, 0}, {0, 0}, path));
    for (int i = 0; i < path.length; ++i) {
        CHECK(path.cells[i].z == 0);
    }
}

TEST_CASE("Pathfinder - walks around buildings") {
    GridIndex grid;
    grid.at(1, 0).type = CellType::Building;
    CarrierPath path;
    REQUIRE(findCarrierPath(grid, {0, 0}, {2, 0}, path));
    for (int i = 0; i < path.length; ++i) {
        CHECK_FALSE((path.cells[i].x == 1 && path.cells[i].z == 0));
    }
}

TEST_CASE("Couriers wear dirt trails on empty tiles") {
    World world;
    CitySystems::initCity(world);

    PlaceBuildingCmd cmd{18, 15, BuildingIds::Lumberjack};
    REQUIRE(validate(world, cmd).ok());
    apply(world, cmd);

    const auto& grid = world.resource<GridIndex>();
    entt::entity lumber = grid.cells[15][18].entity;
    REQUIRE(world.registry().valid(lumber));
    REQUIRE(world.registry().get<ProductionComponent>(lumber).internalBuffer >= 1.0f);

    CitySystems::tickCarriers(world, world.tick());

    CHECK(grid.cells[15][16].type == CellType::Trail);
    CHECK(grid.cells[15][17].type == CellType::Trail);

    for (int t = 0; t < 400; ++t) {
        CitySystems::tickCarriers(world, world.tick());
    }

    CHECK(grid.cells[15][16].type == CellType::Trail);
    CHECK(grid.cells[15][17].type == CellType::Trail);
    CHECK(grid.cells[14][16].type != CellType::Trail);
    CHECK(grid.cells[16][16].type != CellType::Trail);
    CHECK(grid.cells[14][17].type != CellType::Trail);
    CHECK(grid.cells[16][17].type != CellType::Trail);
}

TEST_CASE("Paved road can be built over a worn trail") {
    World world;
    CitySystems::initCity(world);
    world.resource<GridIndex>().at(10, 12).type = CellType::Trail;

    PlaceBuildingCmd cmd{10, 12, BuildingIds::Road};
    REQUIRE(validate(world, cmd).ok());
    apply(world, cmd);
    CHECK(world.resource<GridIndex>().at(10, 12).type == CellType::Road);
}
