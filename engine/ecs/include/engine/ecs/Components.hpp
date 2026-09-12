#pragma once

#include "engine/assets/MeshData.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>

namespace engine {

struct TagComponent {
    std::string tag = "Entity";
};

enum class MeshGeometryType : uint8_t {
    Custom = 0,
    Box,
    Sphere,
    Plane,
    Teapot,
    Model
};

struct MeshGeometryComponent {
    MeshGeometryType type = MeshGeometryType::Custom;
    std::string assetPath;
    glm::vec3 params{1.f};
};

enum class ColliderShapeType : uint8_t {
    None = 0,
    Box,
    Sphere,
    ConvexHull
};

struct ColliderComponent {
    ColliderShapeType shape = ColliderShapeType::None;
    glm::vec3 halfExtents{0.5f};
    float radius = 0.5f;
    bool isStatic = false;
    float mass = 1.f;
};

struct TransformLocal {
    glm::vec3 translation{0.f};
    glm::quat rotation{1.f, 0.f, 0.f, 0.f};
    glm::vec3 scale{1.f};
};

struct TransformWorld {
    glm::mat4 matrix{1.f};
};

struct MeshComponent {
    uint32_t mesh = UINT32_MAX;
    uint32_t baseColorTexture = kInvalidGpuTexture;
    glm::vec3 tint{1.f};
    glm::vec4 baseColorFactor{1.f};
    float metallic{0.f};
    float roughness{0.5f};
};

struct RenderableTag {};

struct CameraComponent {
    float fovDegrees = 70.f;
    float nearPlane = 0.1f;
    float farPlane = 500.f;
    bool active = true;
};

enum class CameraMode {
    FreeFly,
    FirstPerson
};

struct FreeFlyController {
    float yaw = -1.5707963f;
    float pitch = -0.2f;
    float moveSpeed = 8.f;
    float lookSensitivity = 0.005f;
};

struct RigidBodyComponent {
    uint32_t bodyIndex = UINT32_MAX;
    bool dynamic = true;
};

struct StaticColliderTag {};

struct PointLightComponent {
    glm::vec3 color{1.0f, 0.9f, 0.7f};
    float intensity = 5.0f;
    float radius = 10.0f;
};

struct CameraState {
    glm::mat4 view{1.f};
    glm::mat4 proj{1.f};
    glm::mat4 viewProj{1.f};
    glm::vec3 position{0.f};
    float aspect = 1.f;
};

} // namespace engine
