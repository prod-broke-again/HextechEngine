#pragma once

#include "Simulation/EraData.hpp"
#include <entt/entt.hpp>

namespace engine {
class World;
class CommandQueue;
}

namespace engine::era::ui {

void drawBuildDock(
    const engine::World& world,
    engine::CommandQueue& commands,
    float screenWidth,
    float screenHeight,
    StringHash& selectedBuildType,
    bool& demolishMode,
    entt::entity& inspectedBuilding
);

} // namespace engine::era::ui
