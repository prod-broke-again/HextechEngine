#pragma once

#include "engine/runtime/IModule.hpp"
#include "engine/world/World.hpp"
#include "Camera/RtsCamera.hpp"
#include "Graphics/ProceduralCityMeshes.hpp"
#include "Simulation/CitySystems.hpp"
#include "Simulation/CityCommands.hpp"
#include "Simulation/CityEvents.hpp"
#include "Simulation/Components.hpp"
#include "engine/core/Input.hpp"
#include "engine/integration/PlatformGLFW.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"
#include "engine/integration/ImGuiLayer.hpp"
#include "engine/renderer/vulkan/PbrRenderer.hpp"
#include "engine/renderer/vulkan/PostProcessPipeline.hpp"
#include "engine/vfx/ParticleSystem.hpp"
#include "engine/audio/AudioEngine.hpp"
#include "engine/ui/EditorHistory.hpp"
#include "engine/ui/TransformGizmo.hpp"
#include "engine/ui/EditorChrome.hpp"

#include <map>
#include <memory>
#include <iostream>

namespace engine::era {

class EraSandboxModule : public IModule {
public:
    explicit EraSandboxModule(bool smokeTest = false) : m_smokeTest(smokeTest) {}

    std::string_view name() const override { return "EraSandboxModule"; }
    std::span<const std::string_view> dependsOn() const override { 
        static constexpr std::string_view deps[] = { "UiModule", "VfxModule", "AudioModule" };
        return deps;
    }

    void registerTypes(TypeRegistry& registry) override;
    void registerCommands(CommandRegistry& registry) override;
    void onAttach(World& world) override;
    void onDetach(World& world) override;
    void tick(World& world) override;
    void render(World& world, float alpha) override;

private:
    void setupScene(World& world);
    void updateFrame(World& world, float deltaTime);
    bool renderFrame(World& world);
    void handleResize(const engine::WindowResizeEvent& ev);
    void renderUi(const World& world, CommandQueue& commands);

    void spawnVisualBuilding(const BuildingPlacedEvent& ev);
    void removeVisualBuilding(const BuildingRemovedEvent& ev);
    void spawnVisualCarrier(const CarrierSpawnedEvent& ev);
    void spawnAlertIndicator(const BuildingStatusEvent& ev);
    void removeAlertIndicator(entt::entity buildingId);
    void onEraEvolved(const EraEvolvedEvent& ev);

    RtsCamera m_camera;
    entt::entity m_cameraEntity = entt::null;
    entt::entity m_hoverTileEntity = entt::null;
    entt::entity m_ghostEntity = entt::null;

    ProceduralCityMeshes m_cityMeshes;

    // Notice: NO m_simulation, NO parallel maps!
    std::map<entt::entity, entt::entity> m_alertEntities; // map building entity -> alert indicator entity

    static constexpr int kGridSize = 32;
    static constexpr float kTileSize = 1.0f;

    bool m_hasHoverTile = false;
    int m_hoverX = -1;
    int m_hoverZ = -1;
    glm::vec3 m_groundHitPos{0.0f};

    StringHash m_selectedBuildType = BuildingIds::None;
    bool m_demolishMode = false;
    entt::entity m_inspectedBuildingId = entt::null; // using entity instead of uint32_t

    glm::vec3 m_sunDirection{-0.4f, -1.0f, -0.3f};
    bool m_enableShadows = true;
    float m_time = 0.0f;
    
    World* m_world = nullptr;

    bool m_smokeTest = false;
    int m_frameCount = 0;

    engine::ui::EditorHistory m_editorHistory;
    engine::ui::GizmoState m_gizmo;
    engine::ui::SceneSaveControls m_sceneControls;
    bool m_gizmoWasDragging = false;
    glm::vec3 m_gizmoStartTranslation{0.f};
    glm::quat m_gizmoStartRotation{1.f, 0.f, 0.f, 0.f};
};

} // namespace engine::era
