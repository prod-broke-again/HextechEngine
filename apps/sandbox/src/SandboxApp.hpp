#pragma once

#include "engine/assets/AssetManager.hpp"
#include "engine/core/Config.hpp"
#include "engine/core/InputMap.hpp"
#include "engine/core/Time.hpp"
#include "engine/integration/ImGuiLayer.hpp"
#include "engine/integration/PlatformGLFW.hpp"
#include "engine/physics/JoltWorld.hpp"
#include "engine/renderer/vulkan/DebugDraw.hpp"
#include "engine/renderer/vulkan/GpuMeshCache.hpp"
#include "engine/renderer/vulkan/GpuTextureCache.hpp"
#include "engine/renderer/vulkan/PbrRenderer.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"

#include <entt/entt.hpp>
#include <memory>

namespace engine {

class SandboxApp {
public:
    int run();

private:
    void loadConfig();
    void setupInput();
    bool initEngine();
    void shutdownEngine();
    void spawnScene();
    void spawnDynamicObject(const MeshComponent& meshComp, const glm::vec3& halfExtents);
    void spawnDynamicConvexObject(const MeshComponent& meshComp, const std::vector<glm::vec3>& vertices);
    void updateFrame(float deltaTime);
    [[nodiscard]] bool renderFrame();

    Config m_config;
    Time m_time;
    InputMap m_inputMap;
    PlatformGLFW m_platform;
    Input m_input;
    entt::dispatcher m_dispatcher;
    entt::registry m_registry;
    VulkanContext m_vulkan;
    ImGuiLayer m_imgui;
    PbrRenderer m_renderer;
    DebugDraw m_debugDraw;
    std::unique_ptr<GpuMeshCache> m_meshes;
    std::unique_ptr<GpuTextureCache> m_textures;
    JoltWorld m_physics;
    AssetManager m_assets;
    bool m_showDebug = true;

    MeshComponent m_cubeComp{};
    MeshComponent m_teapotComp{};
    std::vector<glm::vec3> m_teapotVertices;
    bool m_hasTeapot = false;
};

} // namespace engine
