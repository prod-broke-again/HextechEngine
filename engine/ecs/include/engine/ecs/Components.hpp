#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace engine {

struct TransformLocal {
    glm::vec3 translation{0.f};
    glm::quat rotation{1.f, 0.f, 0.f, 0.f};
    glm::vec3 scale{1.f};
};

struct TransformWorld {
    glm::mat4 matrix{1.f};
};

struct CameraComponent {
    float fovDegrees = 70.f;
    float nearPlane = 0.1f;
    float farPlane = 500.f;
    bool freeCam = true;
};

struct RigidBodyHandle {
    uint32_t bodyId = 0xffffffffu;
};

} // namespace engine
