#pragma once

#include <entt/entt.hpp>
#include <utility>

namespace engine {

// Deterministic EventBus strictly using queues.
// `trigger()` is intentionally omitted to prevent immediate callback chains.
class EventBus {
public:
    // Enqueue an event to be processed later during drain()
    template <typename T>
    void enqueue(T&& event) {
        m_dispatcher.enqueue<T>(std::forward<T>(event));
    }

    // Connect a member function or free function to an event
    template <typename T, auto Candidate, typename Type>
    void connect(Type* instance) {
        m_dispatcher.sink<T>().template connect<Candidate>(instance);
    }

    template <typename T, auto Candidate>
    void connect() {
        m_dispatcher.sink<T>().template connect<Candidate>();
    }

    template <typename T>
    void disconnect(const void* instance) {
        m_dispatcher.sink<T>().disconnect(instance);
    }

    // Process all queued events
    void drain() {
        m_dispatcher.update();
    }

    template <typename T>
    void drain() {
        m_dispatcher.update<T>();
    }

    entt::dispatcher& dispatcher() { return m_dispatcher; }

private:
    entt::dispatcher m_dispatcher;
};

} // namespace engine
