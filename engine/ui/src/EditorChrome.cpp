#include "engine/ui/EditorChrome.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstring>

namespace engine::ui {

void drawEditorToolbar(EditorHistory& history, entt::registry& registry, const TypeRegistry& types,
                       GizmoState& gizmo, SceneSaveControls& scene) {
    scene.saveRequested = false;
    scene.loadRequested = false;

    if (!ImGui::Begin("Editor")) {
        ImGui::End();
        return;
    }

    drawGizmoModeButtons(gizmo);
    ImGui::Separator();

    const bool canUndo = history.canUndo();
    const bool canRedo = history.canRedo();
    if (!canUndo) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Undo")) {
        history.undo(registry, types);
    }
    if (!canUndo) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    if (!canRedo) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Redo")) {
        history.redo(registry, types);
    }
    if (!canRedo) {
        ImGui::EndDisabled();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Ctrl+Z / Ctrl+Y");

    ImGui::Separator();
    ImGui::TextUnformatted("Scene JSON");
    char pathBuf[260]{};
    const size_t copyLen = (std::min)(scene.path.size(), sizeof(pathBuf) - 1);
    if (copyLen > 0) {
        std::memcpy(pathBuf, scene.path.data(), copyLen);
    }
    if (ImGui::InputText("Path", pathBuf, sizeof(pathBuf))) {
        scene.path = pathBuf;
    }
    scene.saveRequested = ImGui::Button("Save Scene");
    ImGui::SameLine();
    scene.loadRequested = ImGui::Button("Load Scene");
    if (!scene.status.empty()) {
        ImGui::TextWrapped("%s", scene.status.c_str());
    }

    ImGui::End();
}

} // namespace engine::ui
