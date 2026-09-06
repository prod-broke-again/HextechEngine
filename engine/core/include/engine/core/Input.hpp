#pragma once

#include <glm/vec2.hpp>

namespace engine {

struct InputSnapshot {
    glm::vec2 mousePosition{0.f};
    glm::vec2 mouseDelta{0.f};
    float scrollDelta{0.f};
    bool mouseButtons[5]{};
    bool keys[512]{};
};

class Input {
public:
    void beginFrame();
    void endFrame();

    void setKey(int key, bool pressed);
    void setMouseButton(int button, bool pressed);
    void setMousePosition(float x, float y);
    void addScroll(float delta);

    [[nodiscard]] const InputSnapshot& snapshot() const { return m_current; }
    [[nodiscard]] bool keyDown(int key) const;
    [[nodiscard]] bool keyPressed(int key) const;
    [[nodiscard]] bool mouseButtonDown(int button) const;

private:
    InputSnapshot m_current{};
    InputSnapshot m_previous{};
    bool m_firstMouseMove = true;
};

} // namespace engine
