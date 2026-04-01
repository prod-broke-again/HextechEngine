#pragma once

#include <entt/signal/dispatcher.hpp>

namespace engine {

struct WindowResizeEvent {
    int width{};
    int height{};
    WindowResizeEvent() = default;
    WindowResizeEvent(int w, int h) : width(w), height(h) {}
};

struct WindowCloseEvent {};

struct AssetLoadedEvent {
    const char* path{};
};

inline void registerCoreEvents(entt::dispatcher& dispatcher) {
    (void)dispatcher;
}

} // namespace engine
