#include "ui/InspectorPanel.hpp"
#include "Simulation/CityCommands.hpp"
#include "Simulation/Components.hpp"
#include "Simulation/EraData.hpp"
#include "engine/world/World.hpp"
#include "engine/world/CommandQueue.hpp"

#include <imgui.h>

namespace engine::era::ui {

void drawInspector(
    const engine::World& world,
    engine::CommandQueue& commands,
    float screenWidth,
    entt::entity& inspectedBuilding
) {
    if (!world.registry().valid(inspectedBuilding)) {
        inspectedBuilding = entt::null;
        return;
    }

    const auto& b = world.registry().get<BuildingComponent>(inspectedBuilding);
    const auto& pos = world.registry().get<GridPosition>(inspectedBuilding);
    const auto& state = world.resource<CityState>();
    const BuildingDef& def = getBuildingDef(b.type);

    ImGui::SetNextWindowPos(ImVec2(screenWidth - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.9f);

    if (ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", def.name.c_str());
        ImGui::Text("Position: [%d, %d]", pos.x, pos.z);
        ImGui::Separator();
        ImGui::Spacing();

        if (b.type == BuildingIds::TownCenter) {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "TOWN CENTER HUB");
            ImGui::Text("Current Era: %s", getEraName(state.currentEra).data());
            
            if (state.currentEra == EraType::StoneAge) {
                const EraDefinition& eraDef = getEraDefinition(EraType::BronzeAge);
                ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Evolution Requirements:");
                
                const bool popOk = (state.totalPopulation >= eraDef.requiredPopulation);
                ImGui::TextColored(popOk ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 
                                   "[%s] Pop: %d / %d", popOk ? "x" : " ", state.totalPopulation, eraDef.requiredPopulation);
                
                for (const auto& req : eraDef.evolutionRequirements) {
                    const float cur = state.storage.get(req.resource);
                    const bool resOk = cur >= static_cast<float>(req.requiredAmount);
                    ImGui::TextColored(resOk ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 
                                       "[%s] %s: %.0f / %.0f", resOk ? "x" : " ", getResourceName(req.resource).data(), cur, req.requiredAmount);
                }
                
                const bool ready = validate(world, EvolveEraCmd{}).ok();
                if (!ready) ImGui::BeginDisabled();
                if (ImGui::Button("Evolve to Bronze Age!", ImVec2(-1.0f, 35.0f))) {
                    commands.enqueue(EvolveEraCmd{});
                }
                if (!ready) ImGui::EndDisabled();
                ImGui::Separator();
            }

            const auto& cPool = world.resource<CarrierPool>();
            ImGui::Text("Warehouse Fleet: %d / %d busy", cPool.activeCarrierCount, cPool.totalCarrierCount);
            if (ImGui::TreeNodeEx("Active Couriers", ImGuiTreeNodeFlags_DefaultOpen)) {
                for (auto [e, c, sm] : world.registry().view<CarrierComponent, engine::statemachine::StateMachineComponent>().each()) {
                    if (sm.current != CarrierStates::IdleAtWarehouse) {
                        ImGui::BulletText("Courier fetching %s", getResourceName(c.carriedResource).data());
                    }
                }
                ImGui::TreePop();
            }
        } else if (b.type == BuildingIds::Residence || def.maxInhabitants > 0) {
            if (const auto* res = world.registry().try_get<ResidenceComponent>(inspectedBuilding)) {
                ImGui::Text("Inhabitants: %d / %d", res->currentInhabitants, res->maxInhabitants);
                ImGui::Text("Food (Fish): %.0f%%", res->foodSatisfaction * 100.0f);
                ImGui::ProgressBar(res->foodSatisfaction, ImVec2(-1.0f, 10.0f), "");
                ImGui::Text("Warmth (Firewood): %.0f%%", res->warmthSatisfaction * 100.0f);
                ImGui::ProgressBar(res->warmthSatisfaction, ImVec2(-1.0f, 10.0f), "");
                const float tax = def.baseTaxIncomePerMinute * res->overallSatisfaction * 
                                  (static_cast<float>(res->currentInhabitants) / static_cast<float>(res->maxInhabitants));
                ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "Tax: %.1f gold/min", tax);
            }
        } else if (def.production.outputPerMinute > 0.0f) {
            if (const auto* prod = world.registry().try_get<ProductionComponent>(inspectedBuilding)) {
                ImGui::Text("Production: %s", getResourceName(def.production.outputResource).data());
                ImGui::Text("Buffer: %.1f / %.1f", prod->internalBuffer, prod->maxBuffer);
                ImGui::ProgressBar(prod->internalBuffer / prod->maxBuffer, ImVec2(-1.0f, 10.0f), "");
                if (prod->isWorking) {
                    ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[WORKING]");
                    ImGui::ProgressBar(prod->cycleTimer.progress(), ImVec2(-1.0f, 8.0f), "Cycle");
                } else if (prod->isBufferFull) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.1f, 1.0f), "[HALTED - STORAGE FULL]");
                } else {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[HALTED - MISSING INPUTS]");
                }
            }
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (b.type != BuildingIds::TownCenter) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
            if (ImGui::Button("Demolish Building", ImVec2(-1.0f, 30.0f))) {
                commands.enqueue(DemolishBuildingCmd{pos.x, pos.z});
                inspectedBuilding = entt::null;
            }
            ImGui::PopStyleColor();
        }
    }
    ImGui::End();
}

} // namespace engine::era::ui
