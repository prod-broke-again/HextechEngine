#pragma once

#include "engine/modules/economy/EconomyCommands.hpp"
#include "engine/modules/economy/Inventory.hpp"
#include "engine/runtime/IModule.hpp"

namespace engine::economy {

class EconomyModule : public IModule {
public:
    EconomyModule() = default;
    ~EconomyModule() override = default;

    std::string_view name() const override { return "EconomyModule"; }
    std::span<const std::string_view> dependsOn() const override { return {}; }

    void onAttach(World& world) override;
    void onDetach(World& world) override;
};

} // namespace engine::economy
