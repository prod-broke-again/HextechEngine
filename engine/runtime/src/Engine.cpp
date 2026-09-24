#include "engine/runtime/Engine.hpp"
#include "engine/core/Time.hpp"
#include "engine/core/Events.hpp"

#include <iostream>

namespace engine {

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::init() {
    m_world.events().connect<WindowCloseEvent, &Engine::onWindowClose>(this);
    return m_modules.build(m_world);
}

void Engine::shutdown() {
    m_world.events().disconnect<WindowCloseEvent>(this);
    m_modules.shutdown(m_world);
}

void Engine::requestClose() {
    m_shouldClose = true;
}

void Engine::run() {
    Time time;
    time.reset();

    constexpr float kTickRate = 1.0f / 30.0f;
    constexpr int kMaxTicksPerFrame = 5;

    float accumulator = 0.0f;

    while (!m_shouldClose) {
        time.tick();
        float dt = time.delta();
        
        // Prevent spiral of death
        if (dt > 0.25f) {
            dt = 0.25f;
        }

        for (auto* m : m_modules.sortedModules()) {
            m->beginFrame(m_world);
        }

        accumulator += dt;
        int ticksExecuted = 0;

        while (accumulator >= kTickRate) {
            if (ticksExecuted < kMaxTicksPerFrame) {
                tick();
                ticksExecuted++;
            } else {
                // Too many ticks, drop the rest to prevent spiral of death
                std::cerr << "Warning: Dropping ticks to prevent spiral of death. Executed " 
                          << ticksExecuted << " ticks." << std::endl;
                accumulator = fmod(accumulator, kTickRate);
                break;
            }
            accumulator -= kTickRate;
        }

        float alpha = accumulator / kTickRate;
        render(alpha);

        for (auto* m : m_modules.sortedModules()) {
            m->endFrame(m_world);
        }
    }
}

void Engine::tick() {
    m_world.advanceTick();
    
    // CommandDispatch phase: validate and apply queued commands
    m_world.dispatchCommands();

    for (auto* m : m_modules.sortedModules()) {
        m->tick(m_world);
    }
    
    m_world.events().drain();
    m_world.deferred().apply();
}

void Engine::render(float alpha) {
    for (auto* m : m_modules.sortedModules()) {
        m->render(m_world, alpha);
    }
    m_world.events().drain<WindowCloseEvent>();
}

} // namespace engine

