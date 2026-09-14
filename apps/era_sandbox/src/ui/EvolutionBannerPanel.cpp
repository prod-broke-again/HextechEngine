#include "ui/EvolutionBannerPanel.hpp"
#include "Simulation/CityCommands.hpp"
#include "engine/world/World.hpp"
#include "engine/world/CommandQueue.hpp"

#include <imgui.h>

namespace engine::era::ui {

void drawEvolutionBanner(const engine::World& world, engine::CommandQueue& commands, float screenWidth) {
    if (validate(world, EvolveEraCmd{}).ok()) {
        ImGui::SetNextWindowPos(ImVec2(screenWidth / 2.0f, 80.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.9f);
        if (ImGui::Begin("Era Evolution", nullptr, 
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
            
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "★★★ Your city is ready to evolve! ★★★");
            if (ImGui::Button("Evolve to Next Era!", ImVec2(280, 40))) {
                commands.enqueue(EvolveEraCmd{});
            }
        }
        ImGui::End();
    }
}

} // namespace engine::era::ui
