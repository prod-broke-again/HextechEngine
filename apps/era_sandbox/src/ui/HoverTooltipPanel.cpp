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
    const CellData& cell = grid.at(hoverX, hoverZ);
    if (cell.type == CellType::Trail) {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted("Worn trail");
        ImGui::TextColored(ImVec4(0.72f, 0.58f, 0.38f, 1.0f), "Slow dirt path left by couriers");
        ImGui::EndTooltip();
        return;
    }

    entt::entity hoveredId = cell.entity;
    if (world.registry().valid(hoveredId)) {
        ImGui::BeginTooltip();
        const auto& hb = world.registry().get<BuildingComponent>(hoveredId);
        const BuildingDef& hDef = getBuildingDef(hb.type);
        ImGui::TextUnformatted(hDef.name.c_str());
        if (hb.type == BuildingIds::Road) {
            ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.55f, 1.0f), "Couriers walk faster here");
        }
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Click to inspect");
        ImGui::EndTooltip();
    }
}

} // namespace engine::era::ui
