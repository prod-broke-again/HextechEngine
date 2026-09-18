#include <doctest/doctest.h>

#include "engine/modules/economy/EconomyCommands.hpp"
#include "engine/modules/economy/EconomyModule.hpp"
#include "engine/modules/economy/Inventory.hpp"
#include "engine/modules/spatial/SpatialGrid2D.hpp"
#include "engine/modules/spatial/SpatialModule.hpp"
#include "engine/modules/statemachine/StateMachine.hpp"
#include "engine/modules/statemachine/StateMachineModule.hpp"
#include "engine/modules/timer/TickTimer.hpp"
#include "engine/modules/timer/TimerModule.hpp"
#include "engine/world/World.hpp"

using namespace engine;
using namespace engine::economy;
using namespace engine::timer;
using namespace engine::spatial;
using namespace engine::statemachine;

// ----------------------------------------------------------------------------
// EconomyModule Tests
// ----------------------------------------------------------------------------

TEST_CASE("EconomyModule - Inventory basic operations and afford checks") {
    Inventory inv;
    const StringHash gold = "gold"_sh;
    const StringHash wood = "wood"_sh;

    CHECK(inv.get(gold) == 0.0f);
    inv.set(gold, 100.0f);
    CHECK(inv.get(gold) == 100.0f);

    inv.add(gold, -25.0f);
    CHECK(inv.get(gold) == 75.0f);

    CHECK(inv.canAfford(gold, 50.0f));
    CHECK_FALSE(inv.canAfford(gold, 100.0f));

    std::vector<ResourceQuantity> reqs = {
        {gold, 50.0f},
        {wood, 10.0f}
    };
    CHECK_FALSE(inv.canAfford(reqs));

    inv.set(wood, 15.0f);
    CHECK(inv.canAfford(reqs));

    CHECK(inv.tryConsume(reqs));
    CHECK(inv.get(gold) == 25.0f);
    CHECK(inv.get(wood) == 5.0f);

    CHECK_FALSE(inv.tryConsume(reqs));
}

TEST_CASE("EconomyModule - Deposit, Withdraw, Transfer Commands") {
    World world(42);
    EconomyModule ecoMod;
    ecoMod.onAttach(world);

    const StringHash ore = "ore"_sh;
    auto entityA = world.registry().create();
    world.registry().emplace<InventoryComponent>(entityA);

    auto entityB = world.registry().create();
    world.registry().emplace<InventoryComponent>(entityB);

    // Deposit to global store
    world.commandQueue().enqueue(DepositResourceCmd{entt::null, ore, 200.0f});
    world.dispatchCommands();
    CHECK(world.resource<EconomyStore>().globalStorage.get(ore) == 200.0f);

    // Withdraw from global store
    world.commandQueue().enqueue(WithdrawResourceCmd{entt::null, ore, 50.0f});
    world.dispatchCommands();
    CHECK(world.resource<EconomyStore>().globalStorage.get(ore) == 150.0f);

    // Transfer from global store to entity A
    world.commandQueue().enqueue(TransferResourceCmd{entt::null, entityA, ore, 100.0f});
    world.dispatchCommands();
    CHECK(world.resource<EconomyStore>().globalStorage.get(ore) == 50.0f);
    CHECK(world.registry().get<InventoryComponent>(entityA).inventory.get(ore) == 100.0f);

    // Transfer from entity A to entity B
    world.commandQueue().enqueue(TransferResourceCmd{entityA, entityB, ore, 40.0f});
    world.dispatchCommands();
    CHECK(world.registry().get<InventoryComponent>(entityA).inventory.get(ore) == 60.0f);
    CHECK(world.registry().get<InventoryComponent>(entityB).inventory.get(ore) == 40.0f);

    // Invalid transfer (insufficient funds)
    world.commandQueue().enqueue(TransferResourceCmd{entityB, entityA, ore, 999.0f});
    world.dispatchCommands();
    CHECK(world.registry().get<InventoryComponent>(entityB).inventory.get(ore) == 40.0f);
}

// ----------------------------------------------------------------------------
// TimerModule Tests
// ----------------------------------------------------------------------------

TEST_CASE("TimerModule - TickTimer one-shot and repeat") {
    // One-shot
    TickTimer oneShot(3, false);
    CHECK_FALSE(oneShot.isExpired());
    CHECK(oneShot.progress() == 0.0f);

    CHECK_FALSE(oneShot.tick());
    CHECK(oneShot.elapsed() == 1);

    CHECK_FALSE(oneShot.tick());
    CHECK(oneShot.elapsed() == 2);

    CHECK(oneShot.tick());
    CHECK(oneShot.isExpired());
    CHECK_FALSE(oneShot.isActive());
    CHECK(oneShot.progress() == 1.0f);

    // Further ticks do not trigger when inactive
    CHECK_FALSE(oneShot.tick());

    // Repeat
    TickTimer repeater(2, true);
    CHECK_FALSE(repeater.tick());
    CHECK(repeater.tick()); // elapsed == 2 -> triggers, wraps to 0
    CHECK(repeater.isActive());
    CHECK(repeater.elapsed() == 0);
}

TEST_CASE("TimerModule - World tick execution on TimerComponent") {
    World world(42);
    TimerModule timerMod;
    timerMod.onAttach(world);

    auto e = world.registry().create();
    world.registry().emplace<TimerComponent>(e, TickTimer{5, false});

    for (int i = 0; i < 4; ++i) {
        timerMod.tick(world);
    }
    CHECK(world.registry().get<TimerComponent>(e).timer.elapsed() == 4);
    CHECK_FALSE(world.registry().get<TimerComponent>(e).timer.isExpired());

    timerMod.tick(world);
    CHECK(world.registry().get<TimerComponent>(e).timer.isExpired());
}

// ----------------------------------------------------------------------------
// SpatialModule Tests
// ----------------------------------------------------------------------------

TEST_CASE("SpatialModule - SpatialGrid2D bounds, radius, and nearest queries") {
    struct TestCell {
        int id = 0;
        bool occupied = false;
    };

    SpatialGrid2D<TestCell, 10, 10> grid;
    CHECK(grid.inBounds(0, 0));
    CHECK(grid.inBounds(9, 9));
    CHECK_FALSE(grid.inBounds(-1, 0));
    CHECK_FALSE(grid.inBounds(10, 5));

    grid.at(2, 2) = TestCell{101, true};
    grid.at(3, 2) = TestCell{102, true};
    grid.at(8, 8) = TestCell{103, true};

    // Radius query around (2, 2) with radius 1.5
    auto matches = grid.queryRadius(2, 2, 1.5f, [](const TestCell& c, int, int) {
        return c.occupied;
    });
    CHECK(matches.size() == 2);

    // Nearest query from (1, 1) looking for occupied cells
    auto nearest = grid.findNearest(1, 1, 5.0f, [](const TestCell& c, int, int) {
        return c.occupied;
    });
    REQUIRE(nearest.has_value());
    CHECK(nearest->first == 2);
    CHECK(nearest->second == 2);
}

// ----------------------------------------------------------------------------
// StateMachineModule Tests
// ----------------------------------------------------------------------------

TEST_CASE("StateMachineModule - Transitions and tick counting") {
    const StringHash idle = "idle"_sh;
    const StringHash moving = "moving"_sh;
    const StringHash stopped = "stopped"_sh;

    StateMachineComponent sm{idle, idle, 0};
    CHECK(sm.current == idle);

    sm.tick(5);
    CHECK(sm.ticksInState == 5);

    sm.transitionTo(moving);
    CHECK(sm.current == moving);
    CHECK(sm.previous == idle);
    CHECK(sm.ticksInState == 0);

    StateTransitionGraph<StringHash> graph;
    graph.allowTransition(idle, moving);
    graph.allowTransition(moving, stopped);

    CHECK(graph.canTransition(idle, moving));
    CHECK_FALSE(graph.canTransition(idle, stopped));
    CHECK(graph.canTransition(moving, stopped));
}
