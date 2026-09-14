#pragma once

#include "engine/runtime/IModule.hpp"
#include "engine/world/World.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"
#include "engine/vfx/ParticleSystem.hpp"

namespace engine {

class VfxModule : public IModule {
public:
    std::string_view name() const override { return "VfxModule"; }
    std::span<const std::string_view> dependsOn() const override { 
        static constexpr std::string_view deps[] = { "RenderModule" };
        return deps;
    }

    void onAttach(World& world) override {
        auto* vulkan = &world.resource<VulkanContext>();
        auto* particles = &world.emplaceResource<ParticleSystem>();
        particles->init(*vulkan);
    }
};

} // namespace engine

