#pragma once

#include <entt/entt.hpp>

namespace engine {
class World;
class CommandQueue;
}

namespace engine::era::ui {

void drawInspector(
    const engine::World& world,
    engine::CommandQueue& commands,
    float screenWidth,
    entt::entity& inspectedBuilding
);

} // namespace engine::era::ui
