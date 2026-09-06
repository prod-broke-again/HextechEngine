#pragma once

#include "engine/core/Input.hpp"

#include <glm/vec2.hpp>

#include <unordered_map>

namespace engine {

enum class Action {
    MoveForward,
    MoveBack,
    MoveLeft,
    MoveRight,
    Look,
    SpawnBox,
    SpawnTeapot,
    ShootSphere,
    KickObject,
    Jump,
    ToggleCameraMode,
    InspectObject,
};

class InputMap {
public:
    void bind(Action action, int key);
    void bindMouse(Action action, int button);

    [[nodiscard]] bool actionDown(const Input& input, Action action) const;
    [[nodiscard]] bool actionPressed(const Input& input, Action action) const;
    [[nodiscard]] glm::vec2 lookDelta() const { return m_lookDelta; }

    void beginFrame(const Input& input);

private:
    std::unordered_map<Action, int> m_keys;
    std::unordered_map<Action, int> m_buttons;
    glm::vec2 m_lookDelta{0.f};
};

} // namespace engine
