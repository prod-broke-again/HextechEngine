#pragma once

#include "engine/ecs/Components.hpp"
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

namespace engine {

class VulkanContext;

struct Particle {
    glm::vec3 position{0.f};
    glm::vec3 velocity{0.f};
    glm::vec4 startColor{1.f};
    glm::vec4 endColor{1.f, 1.f, 1.f, 0.f};
    float startSize = 0.15f;
    float endSize = 0.0f;
    float rotation = 0.f;
    float rotationSpeed = 0.f;
    float life = 0.f;
    float maxLife = 1.f;
    float gravity = -9.81f;
    float drag = 0.98f;
    bool active = false;
};

class ParticleSystem {
public:
    ParticleSystem();
    ~ParticleSystem();

    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;
    ParticleSystem(ParticleSystem&&) = delete;
    ParticleSystem& operator=(ParticleSystem&&) = delete;

    bool init(VulkanContext& ctx);
    void shutdown();
    [[nodiscard]] bool isInitialized() const { return m_initialized; }

    void update(float deltaTime);
    void record(VkCommandBuffer cmd, const CameraState& camera);

    void spawn(const glm::vec3& position, const glm::vec3& velocity,
               const glm::vec4& startColor, const glm::vec4& endColor,
               float startSize, float endSize, float lifetime,
               float gravity = -9.81f, float drag = 0.98f);

    void spawnMuzzleFlash(const glm::vec3& position, const glm::vec3& forward);
    void spawnImpactSparks(const glm::vec3& position, const glm::vec3& normal, float intensity = 1.0f);
    void spawnBurst(const glm::vec3& position, int count, const glm::vec4& color, float speed = 5.0f);

    [[nodiscard]] size_t activeParticleCount() const;
    void clear();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    bool m_initialized = false;
};

} // namespace engine
