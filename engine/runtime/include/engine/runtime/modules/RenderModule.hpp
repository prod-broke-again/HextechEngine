#pragma once

#include "engine/runtime/IModule.hpp"
#include "engine/world/World.hpp"
#include "engine/integration/PlatformGLFW.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"
#include "engine/renderer/vulkan/PbrRenderer.hpp"
#include "engine/renderer/vulkan/PostProcessPipeline.hpp"
#include "engine/renderer/vulkan/GpuMeshCache.hpp"
#include "engine/renderer/vulkan/GpuTextureCache.hpp"
#include "engine/core/Events.hpp"

#include <iostream>

namespace engine {

class RenderModule : public IModule {
public:
    std::string_view name() const override { return "RenderModule"; }
    std::span<const std::string_view> dependsOn() const override { 
        static constexpr std::string_view deps[] = { "PlatformModule" };
        return deps;
    }

    void onAttach(World& world) override {
        auto* platform = &world.resource<PlatformGLFW>();
        
        auto* vulkan = &world.emplaceResource<VulkanContext>();
        if (!vulkan->init(platform->window(), true)) {
            std::cerr << "Failed to init VulkanContext\n";
        }

        // We will need a way to handle resize. 
        // We can subscribe here, but we need to hold the sink or it's connected directly?
        // Let's just connect a lambda or connect to the resource itself?
        // Wait, connect requires a pointer to instance if it's a member function. 
        // We can just connect a free function that fetches world.resource and calls handleResize.
        m_world = &world;
        world.events().connect<WindowResizeEvent, &RenderModule::onResize>(this);

        auto* meshes = &world.emplaceResource<GpuMeshCache>(*vulkan);
        auto* textures = &world.emplaceResource<GpuTextureCache>(*vulkan);

        auto* renderer = &world.emplaceResource<PbrRenderer>();
        if (!renderer->init(*vulkan) || !renderer->initTextureCache(*textures)) {
            std::cerr << "Failed to init PbrRenderer\n";
        }

        auto* postProcess = &world.emplaceResource<PostProcessPipeline>();
        if (!postProcess->init(*vulkan)) {
            std::cerr << "Failed to init PostProcessPipeline\n";
        }
    }

    void onDetach(World& world) override {
        world.events().disconnect<WindowResizeEvent>(this);
        m_world = nullptr;
    }
    
    void onResize(const WindowResizeEvent& e) {
        if (m_world) {
            m_world->resource<VulkanContext>().handleResize(e);
            m_world->resource<PostProcessPipeline>().handleResize(e.width, e.height);
        }
    }
    
    World* m_world = nullptr;
};

} // namespace engine

