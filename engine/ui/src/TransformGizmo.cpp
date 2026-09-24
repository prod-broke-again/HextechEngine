#include "engine/ui/TransformGizmo.hpp"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace engine::ui {
namespace {

constexpr float kAxisLength = 1.5f;
constexpr float kHitPixels = 12.0f;

glm::vec3 axisVector(int axis) {
    switch (axis) {
    case 0: return {1.f, 0.f, 0.f};
    case 1: return {0.f, 1.f, 0.f};
    default: return {0.f, 0.f, 1.f};
    }
}

ImU32 axisColor(int axis, bool active) {
    const int boost = active ? 80 : 0;
    switch (axis) {
    case 0: return IM_COL32(220 + boost / 4, 60, 60, 255);
    case 1: return IM_COL32(60, 220 + boost / 4, 60, 255);
    default: return IM_COL32(60, 90, 230, 255);
    }
}

bool projectWorld(const glm::vec3& world, const GizmoCamera& camera, glm::vec2& out) {
    const glm::vec4 clip = camera.viewProj * glm::vec4(world, 1.f);
    if (std::abs(clip.w) < 1e-5f) {
        return false;
    }
    const glm::vec3 ndc = glm::vec3(clip) / clip.w;
    if (ndc.z < -1.f || ndc.z > 1.f) {
        return false;
    }
    out.x = (ndc.x * 0.5f + 0.5f) * camera.viewport.x;
    // viewProj already applies the Vulkan Y flip used by the renderer.
    out.y = (ndc.y * 0.5f + 0.5f) * camera.viewport.y;
    return true;
}

float distanceToSegment(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b) {
    const glm::vec2 ab = b - a;
    const float len2 = glm::dot(ab, ab);
    if (len2 < 1e-6f) {
        return glm::length(p - a);
    }
    const float t = std::clamp(glm::dot(p - a, ab) / len2, 0.f, 1.f);
    return glm::length(p - (a + ab * t));
}

glm::vec3 rayDirection(const GizmoCamera& camera, const glm::vec2& mouse) {
    const glm::vec2 ndc{
        (mouse.x / camera.viewport.x) * 2.f - 1.f,
        (mouse.y / camera.viewport.y) * 2.f - 1.f
    };
    const glm::mat4 inv = glm::inverse(camera.viewProj);
    glm::vec4 world = inv * glm::vec4(ndc, 1.f, 1.f);
    if (std::abs(world.w) < 1e-5f) {
        return {0.f, 0.f, -1.f};
    }
    world /= world.w;
    return glm::normalize(glm::vec3(world) - camera.position);
}

float closestAxisParam(const glm::vec3& origin, const glm::vec3& axis, const glm::vec3& rayOrigin,
                       const glm::vec3& rayDir) {
    const glm::vec3 w0 = origin - rayOrigin;
    const float a = glm::dot(axis, axis);
    const float b = glm::dot(axis, rayDir);
    const float c = glm::dot(rayDir, rayDir);
    const float d = glm::dot(axis, w0);
    const float e = glm::dot(rayDir, w0);
    const float denom = a * c - b * b;
    if (std::abs(denom) < 1e-6f) {
        return 0.f;
    }
    return (b * e - c * d) / denom;
}

int pickAxis(const glm::vec3& translation, const GizmoCamera& camera, const glm::vec2& mouse) {
    glm::vec2 originScreen{};
    if (!projectWorld(translation, camera, originScreen)) {
        return -1;
    }
    int best = -1;
    float bestDist = kHitPixels;
    for (int axis = 0; axis < 3; ++axis) {
        glm::vec2 tip{};
        if (!projectWorld(translation + axisVector(axis) * kAxisLength, camera, tip)) {
            continue;
        }
        const float dist = distanceToSegment(mouse, originScreen, tip);
        if (dist < bestDist) {
            bestDist = dist;
            best = axis;
        }
    }
    return best;
}

} // namespace

bool manipulateTransform(GizmoState& state, glm::vec3& translation, glm::quat& rotation,
                         const GizmoCamera& camera, const GizmoInput& input) {
    if (camera.viewport.x < 1.f || camera.viewport.y < 1.f) {
        return false;
    }

    if (!state.dragging) {
        state.hoveredAxis = pickAxis(translation, camera, input.mouse);
        if (input.leftPressed && state.hoveredAxis >= 0) {
            state.dragging = true;
            state.activeAxis = state.hoveredAxis;
            state.startTranslation = translation;
            state.startRotation = rotation;
        }
        return false;
    }

    if (input.leftReleased || !input.leftDown) {
        state.dragging = false;
        state.activeAxis = -1;
        return false;
    }

    const glm::vec3 axis = axisVector(state.activeAxis);
    const glm::vec3 rayDir = rayDirection(camera, input.mouse);
    bool changed = false;

    if (state.mode == GizmoMode::Translate) {
        const float t = closestAxisParam(state.startTranslation, axis, camera.position, rayDir);
        const glm::vec3 next = state.startTranslation + axis * t;
        if (next != translation) {
            translation = next;
            changed = true;
        }
    } else {
        glm::vec2 originScreen{};
        if (projectWorld(state.startTranslation, camera, originScreen)) {
            const glm::vec2 delta = input.mouse - originScreen;
            const float angle = delta.x * 0.01f;
            const glm::quat next = glm::angleAxis(angle, axis) * state.startRotation;
            if (next != rotation) {
                rotation = glm::normalize(next);
                changed = true;
            }
        }
    }
    return changed;
}

void drawGizmoOverlay(const GizmoState& state, const glm::vec3& translation, const GizmoCamera& camera) {
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    glm::vec2 origin{};
    if (!projectWorld(translation, camera, origin)) {
        return;
    }

    for (int axis = 0; axis < 3; ++axis) {
        glm::vec2 tip{};
        if (!projectWorld(translation + axisVector(axis) * kAxisLength, camera, tip)) {
            continue;
        }
        const bool hot = axis == state.activeAxis || axis == state.hoveredAxis;
        const float thickness = hot ? 4.0f : 2.5f;
        draw->AddLine(ImVec2(origin.x, origin.y), ImVec2(tip.x, tip.y), axisColor(axis, hot), thickness);
        draw->AddCircleFilled(ImVec2(tip.x, tip.y), hot ? 6.0f : 4.5f, axisColor(axis, hot));
    }
    draw->AddCircleFilled(ImVec2(origin.x, origin.y), 5.0f, IM_COL32(255, 255, 255, 220));
}

void drawGizmoModeButtons(GizmoState& state) {
    int mode = static_cast<int>(state.mode);
    ImGui::RadioButton("Translate", &mode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Rotate", &mode, 1);
    state.mode = static_cast<GizmoMode>(mode);
}

} // namespace engine::ui
