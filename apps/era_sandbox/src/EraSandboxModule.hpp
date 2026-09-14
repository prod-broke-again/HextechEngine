#pragma once

#include "engine/runtime/IModule.hpp"
#include "engine/world/World.hpp"
#include "Camera/RtsCamera.hpp"
#include "Graphics/ProceduralCityMeshes.hpp"
#include "Simulation/CitySimulation.hpp"
#include "Simulation/EraData.hpp"
#include "engine/core/Input.hpp"
#include "engine/integration/PlatformGLFW.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"
#include "engine/integration/ImGuiLayer.hpp"
#include "engine/renderer/vulkan/PbrRenderer.hpp"
#include "engine/renderer/vulkan/PostProcessPipeline.hpp"
#include "engine/vfx/ParticleSystem.hpp"
#include "engine/audio/AudioEngine.hpp"

#include <unordered_map>
#include <memory>
#include <iostream>

namespace engine::era {

class EraSandboxModule : public IModule {
public:
    std::string_view name() const override { return "EraSandboxModule"; }
    std::span<const std::string_view> dependsOn() const override { 
        static constexpr std::string_view deps[] = { "UiModule", "VfxModule", "AudioModule" };
        return deps;
    }

    void onAttach(World& world) override;
    void onDetach(World& world) override;
    void tick(World& world) override;
    void render(World& world, float alpha) override;

private:
    void setupScene(World& world);
    void updateFrame(World& world, float deltaTime);
    bool renderFrame(World& world);
    void handleResize(const engine::WindowResizeEvent& ev);
    void renderUi(World& world);

    void spawnVisualBuilding(const BuildingInstance& b, World& world);
    void removeVisualBuilding(uint32_t buildingId, World& world);
    void spawnVisualCarrier(const CarrierAgent& agent, World& world);
    void removeVisualCarrier(uint32_t carrierId, World& world);
    void spawnAlertIndicator(const BuildingStatusEvent& ev, World& world);
    void removeAlertIndicator(uint32_t buildingId, World& world);
    void onEraEvolved(EraType newEra, World& world);

    RtsCamera m_camera;
    entt::entity m_cameraEntity = entt::null;
    entt::entity m_hoverTileEntity = entt::null;
    entt::entity m_ghostEntity = entt::null;

    ProceduralCityMeshes m_cityMeshes;
    CitySimulation m_simulation;
    std::unordered_map<uint32_t, entt::entity> m_buildingEntities;
    std::unordered_map<uint32_t, entt::entity> m_carrierEntities;
    std::unordered_map<uint32_t, entt::entity> m_alertEntities;

    static constexpr int kGridSize = 32;
    static constexpr float kTileSize = 1.0f;

    bool m_hasHoverTile = false;
    int m_hoverX = -1;
    int m_hoverZ = -1;
    glm::vec3 m_groundHitPos{0.0f};

    BuildingType m_selectedBuildType = BuildingType::None;
    bool m_demolishMode = false;
    uint32_t m_inspectedBuildingId = 0;

    glm::vec3 m_sunDirection{-0.4f, -1.0f, -0.3f};
    bool m_enableShadows = true;
    float m_time = 0.0f;
    
    World* m_world = nullptr;
};

} // namespace engine::era

