#include "ui/BuildDockPanel.hpp"
#include "Simulation/CitySystems.hpp"
#include "Simulation/Components.hpp"
#include "engine/world/World.hpp"
#include "engine/world/CommandQueue.hpp"

#include <imgui.h>

namespace engine::era::ui {

void drawBuildDock(
    const engine::World& world,
    engine::CommandQueue& /*commands*/,
    float screenWidth,
    float screenHeight,
    StringHash& selectedBuildType,
    bool& demolishMode,
    entt::entity& inspectedBuilding
) {
    const auto& state = world.resource<CityState>();

    ImGui::SetNextWindowPos(ImVec2(screenWidth / 2.0f, screenHeight - 10.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.85f);
    if (ImGui::Begin("Build Dock", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CONSTRUCTION DOCK");
        ImGui::TextDisabled("Couriers stamp a dirt trail to gatherers as soon as they leave.");
        ImGui::Separator();
        ImGui::Spacing();

        const auto buildings = getAvailableBuildingsForEra(state.currentEra);
        
        for (size_t i = 0; i < buildings.size(); ++i) {
            const StringHash bType = buildings[i];
            const BuildingDef& def = getBuildingDef(bType);
            const bool canAfford = CitySystems::canAfford(world, bType);
            
            if (!canAfford) {
                ImGui::BeginDisabled();
            }
            
            if (selectedBuildType == bType) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));
            }

            if (ImGui::Button(def.name.c_str(), ImVec2(100, 40))) {
                selectedBuildType = bType;
                demolishMode = false;
                inspectedBuilding = entt::null;
            }
            
            ImGui::PopStyleColor(2);

            if (!canAfford) {
                ImGui::EndDisabled();
            }

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(def.name.c_str());
                ImGui::Separator();
                ImGui::Text("Cost:");
                if (def.cost.empty()) {
                    ImGui::Text(" - Free");
                } else {
                    for (const auto& req : def.cost) {
                        ImGui::Text(" - %.0f %s", req.amount, getResourceName(req.id).data());
                    }
                }
                ImGui::Spacing();
                ImGui::Text("%s", def.description.c_str());
                ImGui::EndTooltip();
            }

            if (i < buildings.size() - 1) {
                ImGui::SameLine();
            }
        }

        ImGui::SameLine(0, 30.0f); // Gap before demolish button

        if (demolishMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        }

        if (ImGui::Button("Demolish", ImVec2(80, 40))) {
            demolishMode = !demolishMode;
            if (demolishMode) {
                selectedBuildType = BuildingIds::None;
                inspectedBuilding = entt::null;
            }
        }
        ImGui::PopStyleColor(2);

        if (selectedBuildType != BuildingIds::None) {
            ImGui::Spacing();
            ImGui::TextDisabled("R — rotate (4 directions)");
        }

    }
    ImGui::End();
}

} // namespace engine::era::ui
