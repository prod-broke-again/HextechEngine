#include "engine/modules/economy/EconomyCommands.hpp"
#include "engine/world/World.hpp"

namespace engine::economy {

// ----------------------------------------------------------------------------
// DepositResourceCmd
// ----------------------------------------------------------------------------

CmdResult validate(const World& world, const DepositResourceCmd& cmd) {
    if (cmd.amount <= 0.0f || cmd.resourceId == StringHash{}) {
        return CmdResult::fail(CmdStatus::Failure, "Invalid deposit parameters");
    }
    if (cmd.target != entt::null) {
        if (!world.registry().valid(cmd.target) || !world.registry().all_of<InventoryComponent>(cmd.target)) {
            return CmdResult::fail(CmdStatus::Failure, "Target lacks InventoryComponent");
        }
    } else {
        if (!world.hasResource<EconomyStore>()) {
            return CmdResult::fail(CmdStatus::Failure, "EconomyStore resource missing");
        }
    }
    return CmdResult::success();
}

void apply(World& world, const DepositResourceCmd& cmd) {
    if (cmd.target != entt::null) {
        world.registry().get<InventoryComponent>(cmd.target).inventory.add(cmd.resourceId, cmd.amount);
    } else {
        world.resource<EconomyStore>().globalStorage.add(cmd.resourceId, cmd.amount);
    }
}

// ----------------------------------------------------------------------------
// WithdrawResourceCmd
// ----------------------------------------------------------------------------

CmdResult validate(const World& world, const WithdrawResourceCmd& cmd) {
    if (cmd.amount <= 0.0f || cmd.resourceId == StringHash{}) {
        return CmdResult::fail(CmdStatus::Failure, "Invalid withdraw parameters");
    }
    if (cmd.target != entt::null) {
        if (!world.registry().valid(cmd.target) || !world.registry().all_of<InventoryComponent>(cmd.target)) {
            return CmdResult::fail(CmdStatus::Failure, "Target lacks InventoryComponent");
        }
        const auto& inv = world.registry().get<InventoryComponent>(cmd.target).inventory;
        if (!inv.canAfford(cmd.resourceId, cmd.amount)) {
            return CmdResult::fail(CmdStatus::CannotAfford, "Insufficient funds in target inventory");
        }
    } else {
        if (!world.hasResource<EconomyStore>()) {
            return CmdResult::fail(CmdStatus::Failure, "EconomyStore resource missing");
        }
        const auto& store = world.resource<EconomyStore>();
        if (!store.globalStorage.canAfford(cmd.resourceId, cmd.amount)) {
            return CmdResult::fail(CmdStatus::CannotAfford, "Insufficient funds in EconomyStore");
        }
    }
    return CmdResult::success();
}

void apply(World& world, const WithdrawResourceCmd& cmd) {
    if (cmd.target != entt::null) {
        world.registry().get<InventoryComponent>(cmd.target).inventory.add(cmd.resourceId, -cmd.amount);
    } else {
        world.resource<EconomyStore>().globalStorage.add(cmd.resourceId, -cmd.amount);
    }
}

// ----------------------------------------------------------------------------
// TransferResourceCmd
// ----------------------------------------------------------------------------

CmdResult validate(const World& world, const TransferResourceCmd& cmd) {
    if (cmd.amount <= 0.0f || cmd.resourceId == StringHash{} || cmd.from == cmd.to) {
        return CmdResult::fail(CmdStatus::Failure, "Invalid transfer parameters");
    }

    // Source validation
    if (cmd.from != entt::null) {
        if (!world.registry().valid(cmd.from) || !world.registry().all_of<InventoryComponent>(cmd.from)) {
            return CmdResult::fail(CmdStatus::Failure, "Source lacks InventoryComponent");
        }
        const auto& inv = world.registry().get<InventoryComponent>(cmd.from).inventory;
        if (!inv.canAfford(cmd.resourceId, cmd.amount)) {
            return CmdResult::fail(CmdStatus::CannotAfford, "Insufficient funds in source inventory");
        }
    } else {
        if (!world.hasResource<EconomyStore>()) {
            return CmdResult::fail(CmdStatus::Failure, "EconomyStore resource missing");
        }
        const auto& store = world.resource<EconomyStore>();
        if (!store.globalStorage.canAfford(cmd.resourceId, cmd.amount)) {
            return CmdResult::fail(CmdStatus::CannotAfford, "Insufficient funds in global storage");
        }
    }

    // Target validation
    if (cmd.to != entt::null) {
        if (!world.registry().valid(cmd.to) || !world.registry().all_of<InventoryComponent>(cmd.to)) {
            return CmdResult::fail(CmdStatus::Failure, "Destination lacks InventoryComponent");
        }
    } else {
        if (!world.hasResource<EconomyStore>()) {
            return CmdResult::fail(CmdStatus::Failure, "EconomyStore resource missing");
        }
    }

    return CmdResult::success();
}

void apply(World& world, const TransferResourceCmd& cmd) {
    if (cmd.from != entt::null) {
        world.registry().get<InventoryComponent>(cmd.from).inventory.add(cmd.resourceId, -cmd.amount);
    } else {
        world.resource<EconomyStore>().globalStorage.add(cmd.resourceId, -cmd.amount);
    }

    if (cmd.to != entt::null) {
        world.registry().get<InventoryComponent>(cmd.to).inventory.add(cmd.resourceId, cmd.amount);
    } else {
        world.resource<EconomyStore>().globalStorage.add(cmd.resourceId, cmd.amount);
    }
}

// ----------------------------------------------------------------------------
// Registration
// ----------------------------------------------------------------------------

void registerEconomyCommands(CommandRegistry& registry) {
    registry.registerCommand<DepositResourceCmd>(&validate, &apply);
    registry.registerCommand<WithdrawResourceCmd>(&validate, &apply);
    registry.registerCommand<TransferResourceCmd>(&validate, &apply);
}

} // namespace engine::economy
