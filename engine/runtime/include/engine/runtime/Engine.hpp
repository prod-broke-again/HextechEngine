#pragma once

#include "engine/runtime/ModuleRegistry.hpp"
#include "engine/world/World.hpp"

#include <memory>
#include <concepts>
#include <type_traits>

namespace engine {

class Engine {
public:
    Engine();
    ~Engine();

    template<typename T, typename... Args>
    requires std::derived_from<T, IModule>
    void use(Args&&... args) {
        m_modules.addModule(std::make_unique<T>(std::forward<Args>(args)...));
    }

    bool init();
    void run();
    void shutdown();
    void requestClose();

    World& world() { return m_world; }
    const World& world() const { return m_world; }

    bool shouldClose() const { return m_shouldClose; }

private:
    void tick();
    void render(float alpha);

    World m_world;
    ModuleRegistry m_modules;
    bool m_shouldClose = false;
};

} // namespace engine

