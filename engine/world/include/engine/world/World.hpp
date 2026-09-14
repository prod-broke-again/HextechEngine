#pragma once

#include "engine/foundation/EventBus.hpp"
#include "engine/foundation/Rng.hpp"
#include "engine/foundation/TypeRegistry.hpp"

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
    World();
    ~World();

    entt::registry& registry() { return m_registry; }
    const entt::registry& registry() const { return m_registry; }

    EventBus& events() { return m_events; }
    DeferredOps& deferred() { return *m_deferred; }
    Rng& rng(RngStream stream) { return m_rngs[static_cast<size_t>(stream)]; }
    
    Tick tick() const { return m_currentTick; }
    void advanceTick() { m_currentTick.index++; }

    TypeRegistry& types() { return m_types; }

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
    Tick m_currentTick;
};

} // namespace engine

