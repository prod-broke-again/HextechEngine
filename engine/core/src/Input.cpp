#include "engine/core/Input.hpp"

#include <cstring>

namespace engine {

void Input::beginFrame() {
    m_previous = m_current;
}

void Input::endFrame() {
    m_current.mouseDelta = glm::vec2(0.f);
    m_current.scrollDelta = 0.f;
}

void Input::setKey(int key, bool pressed) {
    if (key >= 0 && key < 512) {
        m_current.keys[key] = pressed;
    }
}

void Input::setMouseButton(int button, bool pressed) {
    if (button >= 0 && button < 5) {
        m_current.mouseButtons[button] = pressed;
    }
}

void Input::setMousePosition(float x, float y) {
    const glm::vec2 next{x, y};
    m_current.mouseDelta = next - m_current.mousePosition;
    m_current.mousePosition = next;
}

void Input::addScroll(float delta) { m_current.scrollDelta += delta; }

bool Input::keyDown(int key) const {
    if (key < 0 || key >= 512) {
        return false;
    }
    return m_current.keys[key];
}

bool Input::keyPressed(int key) const {
    if (key < 0 || key >= 512) {
        return false;
    }
    return m_current.keys[key] && !m_previous.keys[key];
}

bool Input::mouseButtonDown(int button) const {
    if (button < 0 || button >= 5) {
        return false;
    }
    return m_current.mouseButtons[button];
}

} // namespace engine
