#pragma once

#include "engine/runtime/IModule.hpp"
#include "engine/world/World.hpp"
#include "engine/integration/PlatformGLFW.hpp"
#include "engine/core/Input.hpp"

#include <iostream>

namespace engine {

class PlatformModule : public IModule {
public:
    std::string_view name() const override { return "PlatformModule"; }

    void onAttach(World& world) override {
        auto* input = &world.emplaceResource<Input>();
        auto* platform = &world.emplaceResource<PlatformGLFW>();
        
        // Use hardcoded values for now, config should be applied later if available
        if (!platform->init(1280, 720, "Hextech Engine", input, &world.events().dispatcher())) {
            std::cerr << "Failed to init PlatformGLFW\n";
        }
    }

    void onDetach(World& world) override {
        world.resource<PlatformGLFW>().shutdown();
    }

    void tick(World& world) override {
    }

    void render(World& world, float) override {
        world.resource<Input>().endFrame(); // End previous frame state
        world.resource<Input>().beginFrame(); // Begin new frame
        auto& platform = world.resource<PlatformGLFW>();
        platform.pollEvents();
        if (platform.shouldClose()) {
            world.events().enqueue(WindowCloseEvent{});
        }
    }
};

} // namespace engine

