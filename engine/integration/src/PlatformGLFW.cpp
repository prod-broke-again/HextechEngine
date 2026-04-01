#include "engine/integration/PlatformGLFW.hpp"

#include "engine/core/Events.hpp"
#include "engine/core/Log.hpp"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

namespace engine {

PlatformGLFW::PlatformGLFW() = default;

PlatformGLFW::~PlatformGLFW() { shutdown(); }

bool PlatformGLFW::init(int width, int height, const char* title, Input* input,
                        entt::dispatcher* dispatcher) {
    m_input = input;
    m_dispatcher = dispatcher;

    if (!glfwInit()) {
        log(LogLevel::Error, "GLFW: init failed");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_window) {
        log(LogLevel::Error, "GLFW: window creation failed");
        glfwTerminate();
        return false;
    }

    glfwSetWindowUserPointer(m_window, this);

    glfwSetKeyCallback(m_window, [](GLFWwindow* w, int key, int, int action, int) {
        auto* self = static_cast<PlatformGLFW*>(glfwGetWindowUserPointer(w));
        if (!self || !self->m_input) {
            return;
        }
        if (action == GLFW_PRESS || action == GLFW_REPEAT) {
            self->m_input->setKey(key, true);
        } else if (action == GLFW_RELEASE) {
            self->m_input->setKey(key, false);
        }
    });

    glfwSetMouseButtonCallback(m_window, [](GLFWwindow* w, int button, int action, int) {
        auto* self = static_cast<PlatformGLFW*>(glfwGetWindowUserPointer(w));
        if (!self || !self->m_input) {
            return;
        }
        self->m_input->setMouseButton(button, action == GLFW_PRESS);
    });

    glfwSetCursorPosCallback(m_window, [](GLFWwindow* w, double x, double y) {
        auto* self = static_cast<PlatformGLFW*>(glfwGetWindowUserPointer(w));
        if (!self || !self->m_input) {
            return;
        }
        self->m_input->setMousePosition(static_cast<float>(x), static_cast<float>(y));
    });

    glfwSetScrollCallback(m_window, [](GLFWwindow* w, double /*x*/, double y) {
        auto* self = static_cast<PlatformGLFW*>(glfwGetWindowUserPointer(w));
        if (!self || !self->m_input) {
            return;
        }
        self->m_input->addScroll(static_cast<float>(y));
    });

    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow* w, int width, int height) {
        auto* self = static_cast<PlatformGLFW*>(glfwGetWindowUserPointer(w));
        if (!self) {
            return;
        }
        if (self->m_dispatcher) {
            self->m_dispatcher->trigger(WindowResizeEvent{width, height});
        }
        if (self->m_resizeCallback) {
            self->m_resizeCallback(width, height);
        }
    });

    return true;
}

void PlatformGLFW::shutdown() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

void PlatformGLFW::pollEvents() const {
    if (m_window) {
        glfwPollEvents();
    }
}

bool PlatformGLFW::shouldClose() const { return m_window && glfwWindowShouldClose(m_window); }

glm::ivec2 PlatformGLFW::framebufferSize() const {
    int w = 0;
    int h = 0;
    if (m_window) {
        glfwGetFramebufferSize(m_window, &w, &h);
    }
    return {w, h};
}

} // namespace engine
