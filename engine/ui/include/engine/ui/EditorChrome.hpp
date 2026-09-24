#pragma once

#include "engine/ui/EditorHistory.hpp"
#include "engine/ui/TransformGizmo.hpp"

#include <string>

namespace engine::ui {

struct SceneSaveControls {
    std::string path = "saves/scene.json";
    std::string status;
    bool saveRequested = false;
    bool loadRequested = false;
};

void drawEditorToolbar(EditorHistory& history, entt::registry& registry, const TypeRegistry& types,
                       GizmoState& gizmo, SceneSaveControls& scene);

} // namespace engine::ui
