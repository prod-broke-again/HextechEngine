#include "ui/TopHudPanel.hpp"
#include "Simulation/Components.hpp"
#include "Simulation/EraData.hpp"
#include "engine/world/World.hpp"
#include "engine/world/CommandQueue.hpp"

#include <imgui.h>

namespace engine::era::ui {

void drawTopHud(const engine::World& world, engine::CommandQueue& /*commands*/) {
    const auto& state = world.resource<CityState>();

    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);
    if (ImGui::Begin("City Status HUD", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings)) {
        
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.2f, 1.0f), "[%s]", getEraName(state.currentEra).data());
        ImGui::SameLine();
        ImGui::Text("  |  ");
        ImGui::SameLine();
        ImGui::Text("Settlers: %d/%d", state.totalPopulation, state.maxPopulation);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "(%.0f%% happy)", state.averageSatisfaction * 100.0f);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float goldRate = state.taxIncomePerMinute;
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "Gold: %.0f  (%.1f / min)", 
                           state.storage.get(ResourceType::Gold), goldRate);

        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        
        const float woodRate = state.productionRatesPerMin[static_cast<size_t>(ResourceType::Wood)] - state.consumptionRatesPerMin[static_cast<size_t>(ResourceType::Wood)];
        ImGui::TextColored(woodRate >= 0 ? ImVec4(0.6f, 0.8f, 0.4f, 1.0f) : ImVec4(0.9f, 0.4f, 0.4f, 1.0f), 
                           "(%.1f / min)", woodRate);
        ImGui::SameLine();
        ImGui::Text("Wood: %.1f", state.storage.get(ResourceType::Wood));
        
        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        
        const float fishRate = state.productionRatesPerMin[static_cast<size_t>(ResourceType::Fish)] - state.consumptionRatesPerMin[static_cast<size_t>(ResourceType::Fish)];
        ImGui::TextColored(fishRate >= 0 ? ImVec4(0.6f, 0.8f, 0.4f, 1.0f) : ImVec4(0.9f, 0.4f, 0.4f, 1.0f), 
                           "(%.1f / min)", fishRate);
        ImGui::SameLine();
        ImGui::Text("Fish: %.1f", state.storage.get(ResourceType::Fish));

        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        
        const float stoneRate = state.productionRatesPerMin[static_cast<size_t>(ResourceType::Stone)] - state.consumptionRatesPerMin[static_cast<size_t>(ResourceType::Stone)];
        ImGui::TextColored(stoneRate >= 0 ? ImVec4(0.6f, 0.8f, 0.4f, 1.0f) : ImVec4(0.9f, 0.4f, 0.4f, 1.0f), 
                           "(%.1f / min)", stoneRate);
        ImGui::SameLine();
        ImGui::Text("Stone: %.1f", state.storage.get(ResourceType::Stone));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const auto& cPool = world.resource<CarrierPool>();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Global Warehouse Couriers: %d / %d active", 
                           cPool.activeCarrierCount, cPool.totalCarrierCount);
    }
    ImGui::End();
}

} // namespace engine::era::ui
