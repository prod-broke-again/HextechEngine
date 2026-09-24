#include <doctest/doctest.h>
#include "engine/world/World.hpp"
#include "engine/world/WorldHasher.hpp"
#include "engine/world/CommandLog.hpp"
#include "Simulation/CityCommands.hpp"
#include "Simulation/CitySystems.hpp"
#include "Simulation/Components.hpp"
#include "Simulation/EraData.hpp"

using namespace engine;
using namespace engine::era;

static uint64_t runSimulation(uint64_t seed, const CommandLog& log, uint64_t maxTicks = 10000) {
    initEraData();
    World world(seed);
    registerCityCommands(world.commands());
    CitySystems::initCity(world);

    CommandPlayback playback(log);

    for (uint64_t t = 0; t < maxTicks; ++t) {
        world.advanceTick();
        playback.update(world);
        world.dispatchCommands();
        CitySystems::update(world, world.tick());
        world.events().drain();
        world.deferred().apply();
    }

    return WorldHasher::computeHash(world);
}

static CommandLog buildTestCommandLog() {
    CommandLog log;

    // A rich, deterministic sequence of commands across 10,000 ticks
    log.record(10, PlaceBuildingCmd{14, 15, BuildingIds::Residence});
    log.record(20, PlaceBuildingCmd{16, 15, BuildingIds::Lumberjack});
    log.record(30, PlaceBuildingCmd{15, 14, BuildingIds::Fishery});
    log.record(40, PlaceBuildingCmd{15, 16, BuildingIds::Residence});
    log.record(50, PlaceBuildingCmd{14, 14, BuildingIds::Road});
    log.record(100, PlaceBuildingCmd{16, 16, BuildingIds::Residence});
    log.record(200, DemolishBuildingCmd{16, 16});
    log.record(300, PlaceBuildingCmd{13, 15, BuildingIds::Residence});
    log.record(400, PlaceBuildingCmd{17, 15, BuildingIds::Lumberjack});
    log.record(500, PlaceBuildingCmd{13, 14, BuildingIds::Road});
    log.record(600, PlaceBuildingCmd{17, 14, BuildingIds::Road});
    log.record(1000, PlaceBuildingCmd{12, 15, BuildingIds::Residence});
    log.record(1500, PlaceBuildingCmd{18, 15, BuildingIds::Residence});
    log.record(2000, EvolveEraCmd{});
    log.record(2500, DemolishBuildingCmd{14, 14});
    log.record(3000, PlaceBuildingCmd{14, 14, BuildingIds::Road});
    log.record(4000, EvolveEraCmd{});
    log.record(5000, PlaceBuildingCmd{16, 16, BuildingIds::Residence});
    log.record(6000, EvolveEraCmd{});
    log.record(7000, DemolishBuildingCmd{13, 15});
    log.record(8000, PlaceBuildingCmd{13, 15, BuildingIds::Residence});
    log.record(9000, EvolveEraCmd{});

    return log;
}

TEST_CASE("Determinism - 10,000 Ticks Simulation Replay") {
    CommandLog log = buildTestCommandLog();

    // Pass 1
    uint64_t hash1 = runSimulation(42, log, 10000);

    // Pass 2 in the same process
    uint64_t hash2 = runSimulation(42, log, 10000);

    // Identical setup and replay must yield exact same hash
    CHECK(hash1 == hash2);

    // Different seed must yield different hash
    uint64_t hashDifferentSeed = runSimulation(1337, log, 10000);
    CHECK(hash1 != hashDifferentSeed);

    // Altered command log must yield different hash
    CommandLog emptyLog;
    uint64_t hashNoCommands = runSimulation(42, emptyLog, 10000);
    CHECK(hash1 != hashNoCommands);

    // Compare against baseline reference hash
    constexpr uint64_t kReferenceHash10k = 9829627130785056071ULL;
    CHECK(hash1 == kReferenceHash10k);
}
