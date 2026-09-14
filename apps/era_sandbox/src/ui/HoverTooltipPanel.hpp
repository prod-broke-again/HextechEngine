#pragma once

#include "Simulation/EraData.hpp"
#include <entt/entt.hpp>

namespace engine {
class World;
}

namespace engine::era::ui {

void drawHoverTooltip(
    const engine::World& world,
    bool hasHoverTile,
    int hoverX,
    int hoverZ,
    BuildingType selectedBuildType,
    bool demolishMode,
    entt::entity inspectedBuilding
);

} // namespace engine::era::ui
