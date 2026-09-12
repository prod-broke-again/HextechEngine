#pragma once

#include "engine/assets/MeshData.hpp"
#include "engine/ecs/Components.hpp"

#include <entt/entt.hpp>
#include <filesystem>
#include <functional>
#include <string>

namespace engine {

struct SceneResourceContext {
    std::function<uint32_t(const MeshCpuData&)> uploadMesh;
    std::function<void(const std::filesystem::path& path, const glm::vec3& pos, float targetSize)> spawnModel;
    std::function<void(entt::entity entity, const glm::vec3& halfExtents, bool isStatic, float mass)> createBoxCollider;
    std::function<void(entt::entity entity, float radius, bool isStatic, float mass)> createSphereCollider;
    std::function<void(entt::entity entity, float mass)> createConvexHullCollider;
    std::function<void(entt::entity entity)> destroyPhysicsBody;
    std::function<void()> clearPhysicsBodies;
    const MeshCpuData* teapotMeshData = nullptr;
};

class SceneSerializer {
public:
    static bool serialize(const std::filesystem::path& filepath, const entt::registry& registry,
                          const glm::vec3& sunDir);

    static bool deserialize(const std::filesystem::path& filepath, entt::registry& registry,
                            glm::vec3& outSunDir, const SceneResourceContext& resCtx);
};

} // namespace engine
