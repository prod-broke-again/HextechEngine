#include "engine/core/InputMap.hpp"

namespace engine {

void InputMap::bind(Action action, int key) { m_keys[action] = key; }

void InputMap::bindMouse(Action action, int button) { m_buttons[action] = button; }

bool InputMap::actionDown(const Input& input, Action action) const {
    if (const auto keyIt = m_keys.find(action); keyIt != m_keys.end()) {
        return input.keyDown(keyIt->second);
    }
    if (const auto btnIt = m_buttons.find(action); btnIt != m_buttons.end()) {
        return input.mouseButtonDown(btnIt->second);
    }
    return false;
}

void InputMap::beginFrame(const Input& input) {
    m_lookDelta = glm::vec2(0.f);
    if (actionDown(input, Action::Look)) {
        m_lookDelta = input.snapshot().mouseDelta;
    }
}

} // namespace engine
