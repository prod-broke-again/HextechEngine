#pragma once

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

} // namespace engine
