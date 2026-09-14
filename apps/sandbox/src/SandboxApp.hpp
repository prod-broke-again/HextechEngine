#pragma once

#include "engine/assets/AssetManager.hpp"
#include "engine/core/Config.hpp"
#include "engine/core/InputMap.hpp"
#include "engine/core/Time.hpp"
#include "engine/integration/ImGuiLayer.hpp"
#include "engine/integration/PlatformGLFW.hpp"
#include "engine/physics/CharacterController.hpp"
#include "engine/physics/JoltWorld.hpp"
#include "engine/renderer/vulkan/DebugDraw.hpp"
#include "engine/renderer/vulkan/GpuMeshCache.hpp"
#include "engine/renderer/vulkan/GpuTextureCache.hpp"
#include "engine/renderer/vulkan/PbrRenderer.hpp"
#include "engine/renderer/vulkan/PostProcessPipeline.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"
#include "engine/foundation/TypeRegistry.hpp"
#include "engine/vfx/ParticleSystem.hpp"

#include <entt/entt.hpp>
#include <memory>

namespace engine {

class SandboxApp {
public:
    int run(int argc = 0, char** argv = nullptr);

private:
    void loadConfig();
    void setupInput();
    bool initEngine();
    void shutdownEngine();
    void spawnScene();
    void spawnDynamicObject(const MeshComponent& meshComp, const glm::vec3& halfExtents);
    void spawnDynamicConvexObject(const MeshComponent& meshComp, const std::vector<glm::vec3>& vertices);
    void shootSphere();
    void kickObjectUnderCrosshair();
    void inspectObjectUnderCrosshair();
    void toggleCameraMode();
    void setCursorCapture(bool capture);
    void respawnPlayer();
    void clearSpawnedObjects();
    void updateFrame(float deltaTime);
    [[nodiscard]] bool renderFrame();

    Config m_config;
    Time m_time;
    InputMap m_inputMap;
    PlatformGLFW m_platform;
    Input m_input;
    entt::dispatcher m_dispatcher;
    entt::registry m_registry;
    TypeRegistry m_types;
    VulkanContext m_vulkan;
    ImGuiLayer m_imgui;
    PbrRenderer m_renderer;
    PostProcessPipeline m_postProcess;
    DebugDraw m_debugDraw;
    std::unique_ptr<GpuMeshCache> m_meshes;
    std::unique_ptr<GpuTextureCache> m_textures;
    JoltWorld m_physics;
    AssetManager m_assets;
    CharacterController m_character;
    ParticleSystem m_particles;
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

    std::vector<entt::entity> m_car1Entities;
    std::vector<entt::entity> m_car2Entities;
    float m_carRotationAngle = 0.0f;
    float m_turntableSpeed = 0.35f;
    bool m_rotateTurntables = true;

    bool m_animateLights = true;
    float m_lightAnimTime = 0.f;
    std::vector<entt::entity> m_demoPointLights;

    char m_sceneFilename[128] = "sandbox_scene.json";
    std::string m_sceneStatusMessage;
    float m_sceneStatusTimer = 0.f;
    float m_masterVolume = 1.0f;

    void saveScene(const std::string& filename);
    void loadScene(const std::string& filename);
    void resetScene();
    void handleResize(const WindowResizeEvent& e);
};

} // namespace engine
