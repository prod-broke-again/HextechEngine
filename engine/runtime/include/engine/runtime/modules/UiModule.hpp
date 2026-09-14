#pragma once

#include "engine/runtime/IModule.hpp"
#include "engine/world/World.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"
#include "engine/integration/PlatformGLFW.hpp"
#include "engine/integration/ImGuiLayer.hpp"

namespace engine {

class UiModule : public IModule {
public:
    std::string_view name() const override { return "UiModule"; }
    std::span<const std::string_view> dependsOn() const override { 
        static constexpr std::string_view deps[] = { "RenderModule" };
        return deps;
    }

    void onAttach(World& world) override {
        auto* vulkan = &world.resource<VulkanContext>();
        auto* platform = &world.resource<PlatformGLFW>();
        
        auto* imgui = &world.emplaceResource<ImGuiLayer>();
        imgui->init(*vulkan, platform->window());
    }

    void render(World& world, float alpha) override {
        auto& imgui = world.resource<ImGuiLayer>();
        // We probably don't render UI here, EraApp does it? Or we call imgui.begin() / end()?
        // Let EraSandboxModule handle actual UI rendering. UiModule just provides ImGuiLayer.
    }
};

} // namespace engine

