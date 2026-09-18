#pragma once

#include "engine/ecs/Components.hpp"
#include "engine/physics/JoltWorld.hpp"

#include <entt/entt.hpp>
#include <glm/glm.hpp>

namespace engine {

struct RaycastHit {
    bool hasHit = false;
    glm::vec3 position{0.f};
    glm::vec3 normal{0.f};
    float distance = 0.f;
    uint32_t bodyIndex = UINT32_MAX;
    entt::entity entity = entt::null;
};

void createStaticBox(JoltWorld& world, entt::registry& registry, entt::entity entity,
                     const glm::vec3& halfExtents);
void createDynamicBox(JoltWorld& world, entt::registry& registry, entt::entity entity,
                      const glm::vec3& halfExtents, float mass = 1.f);
void createDynamicSphere(JoltWorld& world, entt::registry& registry, entt::entity entity,
                         float radius, float mass = 1.f, const glm::vec3& initialVelocity = {0.f, 0.f, 0.f});
void createDynamicConvexHull(JoltWorld& world, entt::registry& registry, entt::entity entity,
                             const std::vector<glm::vec3>& vertices, float mass = 1.f);
void syncTransformsFromPhysics(entt::registry& registry, JoltWorld& world);
void destroyPhysicsBodies(entt::registry& registry, JoltWorld& world);
void destroyPhysicsBody(JoltWorld& world, entt::registry& registry, entt::entity entity);
void clearDynamicBodies(entt::registry& registry, JoltWorld& world);

[[nodiscard]] bool raycast(JoltWorld& world, const entt::registry& registry,
                           const glm::vec3& origin, const glm::vec3& direction,
                           float maxDistance, RaycastHit& hit);

void applyImpulse(JoltWorld& world, entt::registry& registry, entt::entity entity,
                  const glm::vec3& impulse, const glm::vec3& point);

} // namespace engine
