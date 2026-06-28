#pragma once

#include "engine/core/Input.hpp"
#include "engine/core/InputMap.hpp"
#include "engine/ecs/Components.hpp"

#include <entt/entt.hpp>

namespace engine {

class InputMap;

void updateTransforms(entt::registry& registry);
void updateFreeFlyCamera(entt::registry& registry, const Input& input, const InputMap& inputMap,
                         float deltaTime);
[[nodiscard]] CameraState findActiveCamera(const entt::registry& registry, float aspect);

} // namespace engine
