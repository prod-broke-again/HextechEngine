#pragma once

#include "engine/foundation/TypeRegistry.hpp"
#include "engine/ui/EditorHistory.hpp"

#include <entt/entt.hpp>

namespace engine::ui {

void drawComponentInspector(entt::registry& registry, const TypeRegistry& types, entt::entity& selected,
                            EditorHistory* history = nullptr);

} // namespace engine::ui
