#pragma once

#include "engine/foundation/EventBus.hpp"
#include "engine/foundation/Rng.hpp"
#include "engine/foundation/TypeRegistry.hpp"
#include "engine/world/CmdResult.hpp"
#include "engine/world/CommandRegistry.hpp"
#include "engine/world/CommandQueue.hpp"

#include <entt/entt.hpp>
#include <memory>
#include <vector>

namespace engine {

struct Tick {
    uint64_t index = 0;
    static constexpr float dt = 1.0f / 30.0f;
};

class DeferredOps {
public:
    explicit DeferredOps(entt::registry& reg) : m_registry(reg) {}

    entt::entity create() {
        return m_registry.create();
    }

    void destroy(entt::entity e) {
        m_toDestroy.push_back(e);
    }

    void apply() {
        for (auto e : m_toDestroy) {
            if (m_registry.valid(e)) {
                m_registry.destroy(e);
            }
        }
        m_toDestroy.clear();
    }

private:
    entt::registry& m_registry;
    std::vector<entt::entity> m_toDestroy;
};

class World {
public:
    explicit World(uint64_t seed = 0);
    ~World();

    void setSeed(uint64_t seed);

    entt::registry& registry() { return m_registry; }
    const entt::registry& registry() const { return m_registry; }

    EventBus& events() { return m_events; }
    DeferredOps& deferred() { return *m_deferred; }
    Rng& rng(RngStream stream) { return m_rngs[static_cast<size_t>(stream)]; }
    const Rng& rng(RngStream stream) const { return m_rngs[static_cast<size_t>(stream)]; }
    
    Tick tick() const { return m_currentTick; }
    void advanceTick() { m_currentTick.index++; }

    TypeRegistry& types() { return m_types; }
    const TypeRegistry& types() const { return m_types; }

    CommandRegistry& commands() { return m_commands; }
    const CommandRegistry& commands() const { return m_commands; }

    CommandQueue& commandQueue() { return m_commandQueue; }
    const CommandQueue& commandQueue() const { return m_commandQueue; }

    void dispatchCommands();

    template<class T> bool hasResource() const {
        return m_registry.ctx().contains<T>();
    }

    template<class T> T& resource() {
        return m_registry.ctx().get<T>();
    }

    template<class T> const T& resource() const {
        return m_registry.ctx().get<T>();
    }
    
    template<class T, typename... Args>
    T& emplaceResource(Args&&... args) {
        return m_registry.ctx().emplace<T>(std::forward<Args>(args)...);
    }

private:
    entt::registry m_registry;
    EventBus m_events;
    std::unique_ptr<DeferredOps> m_deferred;
    std::vector<Rng> m_rngs;
    TypeRegistry m_types;
    CommandRegistry m_commands;
    CommandQueue m_commandQueue;
    Tick m_currentTick;
};

} // namespace engine

