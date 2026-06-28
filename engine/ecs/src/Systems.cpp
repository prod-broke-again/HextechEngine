#include "engine/ecs/Systems.hpp"

#include "engine/core/InputMap.hpp"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>

namespace engine {

void updateTransforms(entt::registry& registry) {
    auto view = registry.view<TransformLocal, TransformWorld>();
    for (const auto entity : view) {
        const auto& local = view.get<TransformLocal>(entity);
        auto& world = view.get<TransformWorld>(entity);
        world.matrix = glm::translate(glm::mat4(1.f), local.translation) *
                       glm::mat4_cast(local.rotation) * glm::scale(glm::mat4(1.f), local.scale);
    }
}

void updateFreeFlyCamera(entt::registry& registry, const Input& input, const InputMap& inputMap,
                         float deltaTime) {
    auto view = registry.view<TransformLocal, FreeFlyController, CameraComponent>();
    for (const auto entity : view) {
        auto& transform = view.get<TransformLocal>(entity);
        auto& controller = view.get<FreeFlyController>(entity);
        const auto& camera = view.get<CameraComponent>(entity);
        if (!camera.active) {
            continue;
        }

        if (inputMap.actionDown(input, Action::Look)) {
            const glm::vec2 delta = inputMap.lookDelta();
            controller.yaw += delta.x * controller.lookSensitivity;
            controller.pitch -= delta.y * controller.lookSensitivity;
            controller.pitch = std::clamp(controller.pitch, -1.4f, 1.4f);
        }

        const glm::vec3 forward{std::cos(controller.yaw) * std::cos(controller.pitch),
                                std::sin(controller.pitch),
                                std::sin(controller.yaw) * std::cos(controller.pitch)};
        const glm::vec3 right =
            glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));

        glm::vec3 velocity{0.f};
        if (inputMap.actionDown(input, Action::MoveForward)) {
            velocity += forward;
        }
        if (inputMap.actionDown(input, Action::MoveBack)) {
            velocity -= forward;
        }
        if (inputMap.actionDown(input, Action::MoveRight)) {
            velocity += right;
        }
        if (inputMap.actionDown(input, Action::MoveLeft)) {
            velocity -= right;
        }
        if (glm::length(velocity) > 0.f) {
            transform.translation +=
                glm::normalize(velocity) * controller.moveSpeed * deltaTime;
        }
    }
}

CameraState findActiveCamera(const entt::registry& registry, float aspect) {
    CameraState state{};
    state.aspect = aspect;

    auto view = registry.view<const TransformLocal, const CameraComponent>();
    for (const auto entity : view) {
        const auto& transform = view.get<const TransformLocal>(entity);
        const auto& camera = view.get<const CameraComponent>(entity);
        if (!camera.active) {
            continue;
        }

        const auto* controller = registry.try_get<const FreeFlyController>(entity);
        const float yaw = controller ? controller->yaw : 0.f;
        const float pitch = controller ? controller->pitch : 0.f;

        const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                                std::sin(yaw) * std::cos(pitch)};
        state.position = transform.translation;
        state.view = glm::lookAt(state.position, state.position + forward, glm::vec3(0.f, 1.f, 0.f));
        state.proj = glm::perspective(glm::radians(camera.fovDegrees), aspect, camera.nearPlane,
                                      camera.farPlane);
        state.proj[1][1] *= -1.f;
        state.viewProj = state.proj * state.view;
        return state;
    }

    state.view = glm::lookAt(glm::vec3(0.f, 2.f, 6.f), glm::vec3(0.f), glm::vec3(0.f, 1.f, 0.f));
    state.proj = glm::perspective(glm::radians(70.f), aspect, 0.1f, 500.f);
    state.proj[1][1] *= -1.f;
    state.viewProj = state.proj * state.view;
    state.position = glm::vec3(0.f, 2.f, 6.f);
    return state;
}

} // namespace engine
