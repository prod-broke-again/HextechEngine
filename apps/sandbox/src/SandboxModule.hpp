#pragma once

#include "engine/runtime/IModule.hpp"
#include "engine/world/World.hpp"
#include "engine/core/InputMap.hpp"
#include "engine/core/Config.hpp"
#include "engine/core/Events.hpp"
#include "engine/renderer/vulkan/DebugDraw.hpp"
#include "engine/renderer/vulkan/PbrRenderer.hpp"
#include "engine/renderer/vulkan/PostProcessPipeline.hpp"
#include "engine/vfx/ParticleSystem.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/assets/AssetManager.hpp"
#include "engine/world/bridges/PhysicsBridge.hpp"

namespace engine::sandbox {

namespace SandboxActions {
    inline constexpr ActionId SpawnBox         = "spawn_box"_sh;
    inline constexpr ActionId SpawnMesh        = "spawn_mesh"_sh;
    inline constexpr ActionId ShootSphere      = "shoot_sphere"_sh;
    inline constexpr ActionId KickObject       = "kick_object"_sh;
    inline constexpr ActionId InspectObject    = "inspect_object"_sh;
    inline constexpr ActionId ToggleCameraMode = "toggle_camera_mode"_sh;
    inline constexpr ActionId ToggleCursor     = "toggle_cursor"_sh;
}

class SandboxModule : public IModule {
public:
    explicit SandboxModule(bool smokeTest = false);
    ~SandboxModule() override;

    std::string_view name() const override { return "SandboxModule"; }
    std::span<const std::string_view> dependsOn() const override {
        static constexpr std::string_view deps[] = {
            "UiModule", "VfxModule", "AudioModule", "PhysicsModule"
        };
        return deps;
    }

    void registerTypes(TypeRegistry& registry) override;
    void onAttach(World& world) override;
    void onDetach(World& world) override;
    void tick(World& world) override;
    void render(World& world, float alpha) override;

    void enableSmokeTest(bool enable = true) { m_smokeTest = enable; }

private:
    void loadConfig();
    void setupInput();
    void spawnScene(World& world);
    void respawnPlayer(World& world);
    void toggleCameraMode(World& world);
    void setCursorCapture(World& world, bool capture);
    void shootSphere(World& world);
    void kickObjectUnderCrosshair(World& world);
    void inspectObjectUnderCrosshair(World& world);
    void spawnDynamicObject(World& world, const MeshComponent& meshComp, const glm::vec3& halfExtents);
    void clearSpawnedObjects(World& world);
    void updateFrame(World& world, float deltaTime);
    bool renderFrame(World& world);
    void renderUi(World& world);
    void handleResize(const WindowResizeEvent& e, World& world);

    Config m_config;
    InputMap m_inputMap;
    DebugDraw m_debugDraw;
    AssetManager m_assets;

    CameraMode m_cameraMode = CameraMode::FirstPerson;
    bool m_cursorCaptured = true;
    float m_mouseSensitivity = 0.003f;
    glm::vec3 m_spawnPoint{0.0f, 1.2f, 6.0f};
    entt::entity m_selectedEntity = entt::null;
    bool m_showDebug = true;
    bool m_enableShadows = true;

    MeshComponent m_cubeComp{};
    MeshComponent m_sphereComp{};
    glm::vec3 m_sunDirection{-0.35f, -1.f, -0.25f};

    bool m_animateLights = true;
    float m_lightAnimTime = 0.f;
    std::vector<entt::entity> m_demoPointLights;

    bool m_smokeTest = false;
    int m_frameCount = 0;
};

} // namespace engine::sandbox
