#include "ui/HoverTooltipPanel.hpp"
#include "Simulation/Components.hpp"
#include "Simulation/EraData.hpp"
#include "engine/world/World.hpp"

#include <imgui.h>

namespace engine::era::ui {

void drawHoverTooltip(
    const engine::World& world,
    bool hasHoverTile,
    int hoverX,
    int hoverZ,
    StringHash selectedBuildType,
    bool demolishMode,
    entt::entity inspectedBuilding
) {
    if (ImGui::GetIO().WantCaptureMouse || !hasHoverTile || demolishMode || selectedBuildType != BuildingIds::None) {
        return;
    }

    if (world.registry().valid(inspectedBuilding)) {
        return;
    }

    const auto& grid = world.resource<GridIndex>();
    entt::entity hoveredId = grid.at(hoverX, hoverZ).entity;
    if (world.registry().valid(hoveredId)) {
        ImGui::BeginTooltip();
        const auto& hb = world.registry().get<BuildingComponent>(hoveredId);
        const BuildingDef& hDef = getBuildingDef(hb.type);
        ImGui::TextUnformatted(hDef.name.c_str());
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Click to inspect");
        ImGui::EndTooltip();
    }
}

} // namespace engine::era::ui
