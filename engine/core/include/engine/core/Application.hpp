#pragma once

#include "engine/core/Config.hpp"
#include "engine/core/Input.hpp"

#include <entt/entt.hpp>
#include <memory>

namespace engine {

class Application {
public:
    Application();
    virtual ~Application();

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    virtual void run() = 0;

    [[nodiscard]] Input& input() { return m_input; }
    [[nodiscard]] entt::dispatcher& events() { return m_dispatcher; }
    [[nodiscard]] Config& config() { return m_config; }

protected:
    Input m_input;
    entt::dispatcher m_dispatcher;
    Config m_config;
};

} // namespace engine
