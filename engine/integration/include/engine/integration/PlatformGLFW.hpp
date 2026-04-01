#pragma once

#include "engine/core/Events.hpp"
#include "engine/core/Input.hpp"

#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <functional>
#include <memory>

struct GLFWwindow;

namespace engine {

class PlatformGLFW {
public:
    using ResizeCallback = std::function<void(int, int)>;

    PlatformGLFW();
    ~PlatformGLFW();

    PlatformGLFW(const PlatformGLFW&) = delete;
    PlatformGLFW& operator=(const PlatformGLFW&) = delete;

    bool init(int width, int height, const char* title, Input* input, entt::dispatcher* dispatcher);
    void shutdown();

    void pollEvents() const;
    [[nodiscard]] bool shouldClose() const;
    [[nodiscard]] GLFWwindow* window() const { return m_window; }
    [[nodiscard]] glm::ivec2 framebufferSize() const;

    void setResizeCallback(ResizeCallback cb) { m_resizeCallback = std::move(cb); }

private:
    GLFWwindow* m_window = nullptr;
    Input* m_input = nullptr;
    entt::dispatcher* m_dispatcher = nullptr;
    ResizeCallback m_resizeCallback;
};

} // namespace engine
