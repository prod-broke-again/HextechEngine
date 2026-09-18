#pragma once

#include "engine/modules/statemachine/StateMachine.hpp"
#include "engine/runtime/IModule.hpp"

namespace engine::statemachine {

class StateMachineModule : public IModule {
public:
    StateMachineModule() = default;
    ~StateMachineModule() override = default;

    std::string_view name() const override { return "StateMachineModule"; }
    std::span<const std::string_view> dependsOn() const override { return {}; }

    void onAttach(World& world) override;
    void onDetach(World& world) override;
    void tick(World& world) override;
};

} // namespace engine::statemachine
