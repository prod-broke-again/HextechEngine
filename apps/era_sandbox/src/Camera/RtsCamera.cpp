#include "Camera/RtsCamera.hpp"

#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>

namespace engine::era {

RtsCamera::RtsCamera() = default;

glm::vec3 RtsCamera::eyePosition() const {
    const glm::vec3 offset{
        m_distance * std::cos(m_pitch) * std::sin(m_yaw),
        m_distance * std::sin(m_pitch),
        m_distance * std::cos(m_pitch) * std::cos(m_yaw)
    };
    return m_target + offset;
}

CameraState RtsCamera::getCameraState(float aspect) const {
    CameraState state{};
    state.aspect = aspect;
    state.position = eyePosition();
    state.view = glm::lookAt(state.position, m_target, glm::vec3(0.0f, 1.0f, 0.0f));
    state.proj = glm::perspective(glm::radians(m_fov), aspect, m_near, m_far);
    state.proj[1][1] *= -1.0f; // Vulkan Y flip
    state.viewProj = state.proj * state.view;
    return state;
}

void RtsCamera::update(const Input& input, float deltaTime, bool allowInput) {
    if (!allowInput) {
        return;
    }

    const float speedMultiplier = input.keyDown(GLFW_KEY_LEFT_SHIFT) ? 2.2f : 1.0f;

    const glm::vec3 eye = eyePosition();
    const glm::vec3 forward = glm::normalize(m_target - eye);
    const glm::vec3 flatForward = glm::normalize(glm::vec3(forward.x, 0.0f, forward.z));
    const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    // Keyboard Pan (WASD / Arrows)
    glm::vec3 moveDir{0.0f};
    if (input.keyDown(GLFW_KEY_W) || input.keyDown(GLFW_KEY_UP))    moveDir += flatForward;
    if (input.keyDown(GLFW_KEY_S) || input.keyDown(GLFW_KEY_DOWN))  moveDir -= flatForward;
    if (input.keyDown(GLFW_KEY_D) || input.keyDown(GLFW_KEY_RIGHT)) moveDir += right;
    if (input.keyDown(GLFW_KEY_A) || input.keyDown(GLFW_KEY_LEFT))  moveDir -= right;

    if (glm::length(moveDir) > 0.001f) {
        m_target += glm::normalize(moveDir) * (m_panSpeed * speedMultiplier * deltaTime);
    }

    // Keyboard Orbit (Q / E)
    if (input.keyDown(GLFW_KEY_Q)) m_yaw += 1.8f * deltaTime;
    if (input.keyDown(GLFW_KEY_E)) m_yaw -= 1.8f * deltaTime;

    // Mouse Wheel Zoom
    const float scroll = input.snapshot().scrollDelta;
    if (std::abs(scroll) > 0.01f) {
        m_distance -= scroll * m_zoomSpeed;
        m_distance = std::clamp(m_distance, m_minDistance, m_maxDistance);
    }

    const glm::vec2 delta = input.snapshot().mouseDelta;

    // Mouse RMB Orbit
    if (input.mouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT)) {
        m_yaw -= delta.x * m_orbitSensitivity;
        m_pitch += delta.y * m_orbitSensitivity;
        m_pitch = std::clamp(m_pitch, m_minPitch, m_maxPitch);
    }

    // Mouse MMB Drag Pan
    if (input.mouseButtonDown(GLFW_MOUSE_BUTTON_MIDDLE)) {
        const float panFactor = (m_distance / 25.0f) * 0.025f;
        m_target -= right * (delta.x * panFactor);
        m_target += flatForward * (delta.y * panFactor);
    }

    // Keep camera target within bounds of the 32x32 terrain
    m_target.x = std::clamp(m_target.x, -10.0f, 42.0f);
    m_target.z = std::clamp(m_target.z, -10.0f, 42.0f);
    m_target.y = 0.0f;
}

bool RtsCamera::unprojectCursorToPlane(const glm::vec2& mousePos, const glm::vec2& screenSize,
                                      float yPlane, glm::vec3& outIntersection,
                                      const CameraState& camera) const {
    if (screenSize.x <= 0.0f || screenSize.y <= 0.0f) {
        return false;
    }

    const float ndcX = (2.0f * mousePos.x / screenSize.x) - 1.0f;
    const float ndcY = (2.0f * mousePos.y / screenSize.y) - 1.0f;

    const glm::mat4 invVP = glm::inverse(camera.viewProj);

    glm::vec4 pFar = invVP * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    if (std::abs(pFar.w) < 1e-6f) {
        return false;
    }
    pFar /= pFar.w;

    const glm::vec3 rayOrigin = camera.position;
    const glm::vec3 rayDir = glm::normalize(glm::vec3(pFar) - rayOrigin);

    if (std::abs(rayDir.y) < 1e-6f) {
        return false;
    }

    const float t = (yPlane - rayOrigin.y) / rayDir.y;
    if (t < 0.0f) {
        return false;
    }

    outIntersection = rayOrigin + rayDir * t;
    return true;
}

bool RtsCamera::getGridCoords(const glm::vec3& worldPos, float tileSize, int gridSize,
                             int& outX, int& outZ) const {
    const int x = static_cast<int>(std::floor(worldPos.x / tileSize));
    const int z = static_cast<int>(std::floor(worldPos.z / tileSize));
    outX = x;
    outZ = z;
    return (x >= 0 && x < gridSize && z >= 0 && z < gridSize);
}

} // namespace engine::era
