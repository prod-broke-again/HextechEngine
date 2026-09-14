#pragma once

#include "engine/core/Input.hpp"
#include "engine/ecs/Components.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace engine::era {

class RtsCamera {
public:
    RtsCamera();

    void update(const Input& input, float deltaTime, bool allowInput);

    [[nodiscard]] CameraState getCameraState(float aspect) const;

    [[nodiscard]] glm::vec3 eyePosition() const;
    [[nodiscard]] glm::vec3 target() const { return m_target; }
    [[nodiscard]] float distance() const { return m_distance; }
    [[nodiscard]] float yaw() const { return m_yaw; }
    [[nodiscard]] float pitch() const { return m_pitch; }

    void setTarget(const glm::vec3& target) { m_target = target; }
    void setDistance(float distance) { m_distance = distance; }

    bool unprojectCursorToPlane(const glm::vec2& mousePos, const glm::vec2& screenSize,
                                float yPlane, glm::vec3& outIntersection,
                                const CameraState& camera) const;

    bool getGridCoords(const glm::vec3& worldPos, float tileSize, int gridSize,
                       int& outX, int& outZ) const;

private:
    glm::vec3 m_target{16.0f, 0.0f, 16.0f}; // Center of 32x32 grid
    float m_distance = 28.0f;
    float m_pitch = 0.95f;                 // ~54 degrees
    float m_yaw = 0.785398f;               // 45 degrees isometric angle
    float m_fov = 50.0f;
    float m_near = 0.1f;
    float m_far = 300.0f;

    float m_minDistance = 6.0f;
    float m_maxDistance = 70.0f;
    float m_minPitch = 0.25f;              // ~15 degrees
    float m_maxPitch = 1.45f;              // ~83 degrees

    float m_panSpeed = 18.0f;
    float m_zoomSpeed = 3.5f;
    float m_orbitSensitivity = 0.006f;

    bool m_isOrbiting = false;
    bool m_isPanning = false;
};

} // namespace engine::era
