#pragma once

#include "engine/modules/economy/Inventory.hpp"
#include "engine/world/CmdResult.hpp"
#include "engine/world/CommandRegistry.hpp"
#include <entt/entt.hpp>

namespace engine {
class World;
}

namespace engine::economy {

struct EconomyStore {
    Inventory globalStorage;
};

struct DepositResourceCmd {
    entt::entity target = entt::null;
    StringHash resourceId{};
    float amount = 0.0f;
};

CmdResult validate(const World& world, const DepositResourceCmd& cmd);
void apply(World& world, const DepositResourceCmd& cmd);

struct WithdrawResourceCmd {
    entt::entity target = entt::null;
    StringHash resourceId{};
    float amount = 0.0f;
};

CmdResult validate(const World& world, const WithdrawResourceCmd& cmd);
void apply(World& world, const WithdrawResourceCmd& cmd);

struct TransferResourceCmd {
    entt::entity from = entt::null;
    entt::entity to = entt::null;
    StringHash resourceId{};
    float amount = 0.0f;
};

CmdResult validate(const World& world, const TransferResourceCmd& cmd);
void apply(World& world, const TransferResourceCmd& cmd);

void registerEconomyCommands(CommandRegistry& registry);

} // namespace engine::economy
