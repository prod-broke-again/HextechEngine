#pragma once

#include "engine/ecs/Components.hpp"
#include "engine/physics/JoltWorld.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace engine {

void createStaticBox(JoltWorld& world, entt::registry& registry, entt::entity entity,
                     const glm::vec3& halfExtents);
void createDynamicBox(JoltWorld& world, entt::registry& registry, entt::entity entity,
                      const glm::vec3& halfExtents, float mass = 1.f);
void syncTransformsFromPhysics(entt::registry& registry, JoltWorld& world);
void destroyPhysicsBodies(entt::registry& registry, JoltWorld& world);

} // namespace engine
