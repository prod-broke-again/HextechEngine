#pragma once

#include "engine/foundation/TypeRegistry.hpp"

#include <entt/entt.hpp>

namespace engine::ui {

void drawComponentInspector(entt::registry& registry, const TypeRegistry& types, entt::entity& selected);

} // namespace engine::ui
