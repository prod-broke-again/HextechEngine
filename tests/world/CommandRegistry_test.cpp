#include <doctest/doctest.h>
#include "engine/world/World.hpp"
#include "engine/world/CommandRegistry.hpp"
#include "engine/world/CommandQueue.hpp"

namespace {

struct TestCounter {
    int value = 0;
};

struct IncrementCmd {
    int amount = 1;
};

struct FailCmd {
    bool shouldFail = true;
};

engine::CmdResult validate(const engine::World&, const IncrementCmd& cmd) {
    if (cmd.amount <= 0) {
        return engine::CmdResult::fail(engine::CmdStatus::RequirementsNotMet, "Amount must be positive");
    }
    return engine::CmdResult::success();
}

void apply(engine::World& world, const IncrementCmd& cmd) {
    world.resource<TestCounter>().value += cmd.amount;
}

engine::CmdResult validate(const engine::World&, const FailCmd& cmd) {
    if (cmd.shouldFail) {
        return engine::CmdResult::fail(engine::CmdStatus::Failure, "Explicit fail");
    }
    return engine::CmdResult::success();
}

void apply(engine::World&, const FailCmd&) {}

} // namespace

TEST_CASE("CommandRegistry and CommandQueue basic operations") {
    engine::World world;
    world.emplaceResource<TestCounter>();

    SUBCASE("Unregistered command fails validation") {
        IncrementCmd cmd{5};
        auto res = world.commands().validate(world, cmd);
        CHECK_FALSE(res.ok());
        CHECK(res.status == engine::CmdStatus::UnknownCommand);
    }

    SUBCASE("Register and validate command") {
        world.commands().registerCommand<IncrementCmd>(&validate, &apply);
        CHECK(world.commands().isRegistered<IncrementCmd>());

        IncrementCmd validCmd{10};
        auto resValid = world.commands().validate(world, validCmd);
        CHECK(resValid.ok());
        CHECK(resValid.status == engine::CmdStatus::Success);

        IncrementCmd invalidCmd{-5};
        auto resInvalid = world.commands().validate(world, invalidCmd);
        CHECK_FALSE(resInvalid.ok());
        CHECK(resInvalid.status == engine::CmdStatus::RequirementsNotMet);
    }

    SUBCASE("Dispatch through CommandQueue") {
        world.commands().registerCommand<IncrementCmd>(&validate, &apply);
        world.commands().registerCommand<FailCmd>(&validate, &apply);

        world.commandQueue().enqueue(IncrementCmd{5});
        world.commandQueue().enqueue(IncrementCmd{-2}); // Should fail validation and not apply
        world.commandQueue().enqueue(FailCmd{true});    // Should fail validation
        world.commandQueue().enqueue(IncrementCmd{3});  // Should succeed

        CHECK(world.commandQueue().size() == 4);
        CHECK_FALSE(world.commandQueue().empty());

        world.dispatchCommands();

        CHECK(world.commandQueue().empty());
        CHECK(world.resource<TestCounter>().value == 8);
    }
}
