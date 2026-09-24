#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>

namespace engine::ui {

enum class GizmoMode : uint8_t {
    Translate = 0,
    Rotate = 1
};

struct GizmoState {
    GizmoMode mode = GizmoMode::Translate;
    int hoveredAxis = -1;
    int activeAxis = -1;
    bool dragging = false;
    glm::vec3 startTranslation{0.f};
    glm::quat startRotation{1.f, 0.f, 0.f, 0.f};
};

struct GizmoCamera {
    glm::mat4 view{1.f};
    glm::mat4 proj{1.f};
    glm::mat4 viewProj{1.f};
    glm::vec3 position{0.f};
    glm::vec2 viewport{1.f, 1.f};
};

struct GizmoInput {
    glm::vec2 mouse{0.f};
    bool leftDown = false;
    bool leftPressed = false;
    bool leftReleased = false;
};

bool manipulateTransform(GizmoState& state, glm::vec3& translation, glm::quat& rotation,
                         const GizmoCamera& camera, const GizmoInput& input);

void drawGizmoOverlay(const GizmoState& state, const glm::vec3& translation,
                      const GizmoCamera& camera);

void drawGizmoModeButtons(GizmoState& state);

} // namespace engine::ui
