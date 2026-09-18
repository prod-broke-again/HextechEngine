#pragma once

#include "engine/modules/timer/TickTimer.hpp"
#include "engine/runtime/IModule.hpp"

namespace engine::timer {

class TimerModule : public IModule {
public:
    TimerModule() = default;
    ~TimerModule() override = default;

    std::string_view name() const override { return "TimerModule"; }
    std::span<const std::string_view> dependsOn() const override { return {}; }

    void onAttach(World& world) override;
    void onDetach(World& world) override;
    void tick(World& world) override;
};

} // namespace engine::timer
