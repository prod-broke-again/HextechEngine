#include "SandboxApp.hpp"

#include "engine/assets/GltfMeshLoader.hpp"
#include "engine/assets/GltfTextureLoader.hpp"
#include "engine/assets/MeshBuilder.hpp"
#include "engine/assets/ObjMeshLoader.hpp"
#include "engine/core/Events.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Path.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/ecs/Systems.hpp"
#include "engine/physics/PhysicsBridge.hpp"
#include "engine/renderer/vulkan/ShaderHotReload.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>

namespace engine {

namespace {

void onWindowResize(VulkanContext& vulkan, WindowResizeEvent& event) {
    vulkan.handleResize(event);
}

entt::entity spawnMeshEntity(entt::registry& registry, const MeshComponent& meshComponent,
                             const glm::vec3& position, const glm::vec3& scale) {
    const entt::entity entity = registry.create();
    registry.emplace<TransformLocal>(entity, TransformLocal{position, glm::quat{1.f, 0.f, 0.f, 0.f}, scale});
    registry.emplace<TransformWorld>(entity);
    registry.emplace<MeshComponent>(entity, meshComponent);
    registry.emplace<RenderableTag>(entity);
    return entity;
}

entt::entity spawnMeshEntity(entt::registry& registry, uint32_t meshId, const glm::vec3& position,
                             const glm::vec3& scale, const glm::vec3& tint) {
    MeshComponent meshComp{};
    meshComp.mesh = meshId;
    meshComp.tint = tint;
    return spawnMeshEntity(registry, meshComp, position, scale);
}

void spawnGltfModel(entt::registry& registry, AssetManager& assets, GpuMeshCache& meshes,
                    GpuTextureCache& textures, const std::filesystem::path& path,
                    const glm::vec3& position, float targetSize) {
    const AssetManager::GltfPtr gltf = assets.getOrLoadGltf(path);
    if (!gltf) {
        log(LogLevel::Warn, "spawnGltfModel: failed to load " + path.string());
        return;
    }

    std::vector<GltfMeshPart> parts = GltfMeshLoader::extractMeshParts(*gltf);
    if (parts.empty()) {
        log(LogLevel::Warn, "spawnGltfModel: no mesh parts in " + path.filename().string());
        return;
    }

    std::vector<GltfMeshPart> validParts;
    validParts.reserve(parts.size());
    for (GltfMeshPart& part : parts) {
        if (!part.mesh.empty()) {
            validParts.push_back(std::move(part));
        }
    }
    if (validParts.empty()) {
        return;
    }

    std::vector<MeshCpuData> meshData;
    meshData.reserve(validParts.size());
    for (GltfMeshPart& part : validParts) {
        meshData.push_back(std::move(part.mesh));
    }
    ObjMeshLoader::normalize(meshData, targetSize);

    for (size_t i = 0; i < validParts.size(); ++i) {
        const GltfMeshPart& part = validParts[i];

        MeshComponent meshComp{};
        meshComp.mesh = meshes.upload(meshData[i]);
        meshComp.baseColorFactor = part.baseColorFactor;
        meshComp.metallic = part.metallic;
        meshComp.roughness = part.roughness;

        if (part.baseColorTexture) {
            if (auto cpuTex = GltfTextureLoader::loadBaseColorTexture(assets, *gltf, part.baseColorTexture)) {
                meshComp.baseColorTexture = textures.upload(*cpuTex);
            }
        }

        spawnMeshEntity(registry, meshComp, position, {1.f, 1.f, 1.f});
    }

    log(LogLevel::Info, "spawnGltfModel: spawned " + path.filename().string() + " (" +
                           std::to_string(validParts.size()) + " parts)");
}

std::string shaderPath(const char* name) {
    return std::string(SHADER_DIR) + "/" + name;
}

} // namespace

void SandboxApp::loadConfig() {
    const std::filesystem::path cfgPath = resolvePath("engine_config.json");
    if (std::filesystem::exists(cfgPath)) {
        m_config.loadFromFile(cfgPath);
    }
    setMinLogLevel(static_cast<LogLevel>(m_config.getInt("log.level", static_cast<int>(LogLevel::Info))));
    m_showDebug = m_config.getBool("debug.draw", true);
}

void SandboxApp::setupInput() {
    m_inputMap.bind(Action::MoveForward, GLFW_KEY_W);
    m_inputMap.bind(Action::MoveBack, GLFW_KEY_S);
    m_inputMap.bind(Action::MoveLeft, GLFW_KEY_A);
    m_inputMap.bind(Action::MoveRight, GLFW_KEY_D);
    m_inputMap.bindMouse(Action::Look, GLFW_MOUSE_BUTTON_RIGHT);
}

bool SandboxApp::initEngine() {
    if (!m_platform.init(m_config.getInt("window.width", 1280), m_config.getInt("window.height", 720),
                         m_config.getString("window.title", "Hextech Engine Sandbox").c_str(),
                         &m_input, &m_dispatcher)) {
        return false;
    }

    if (!m_vulkan.init(m_platform.window(), m_config.getBool("vulkan.validation", true))) {
        return false;
    }

    m_dispatcher.sink<WindowResizeEvent>().connect<&onWindowResize>(m_vulkan);

    if (!m_imgui.init(m_vulkan, m_platform.window())) {
        return false;
    }

    m_meshes = std::make_unique<GpuMeshCache>(m_vulkan);
    m_textures = std::make_unique<GpuTextureCache>(m_vulkan);
    if (!m_renderer.init(m_vulkan)) {
        return false;
    }
    if (!m_renderer.initTextureCache(*m_textures)) {
        return false;
    }
    m_debugDraw.init(m_vulkan);

    shaderHotReloadWatchPath(shaderPath("pbr.vert").c_str());
    shaderHotReloadWatchPath(shaderPath("pbr.frag").c_str());

    m_time.reset();
    spawnScene();
    return true;
}

void SandboxApp::spawnScene() {
    const uint32_t floorMesh = m_meshes->upload(MeshBuilder::plane(20.f, {0.18f, 0.22f, 0.18f}));

    spawnMeshEntity(m_registry, floorMesh, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f});

    const entt::entity floorCollider = m_registry.create();
    m_registry.emplace<TransformLocal>(floorCollider, TransformLocal{{0.f, -0.5f, 0.f}});
    m_registry.emplace<StaticColliderTag>(floorCollider);
    m_registry.emplace<RigidBodyComponent>(floorCollider);
    createStaticBox(m_physics, m_registry, floorCollider, {20.f, 0.5f, 20.f});

    spawnGltfModel(m_registry, m_assets, *m_meshes, *m_textures,
                   "assets/armored+female+character+3d+model (3).glb", {0.f, 0.f, 0.f}, 1.8f);

    MeshCpuData teapotData = ObjMeshLoader::loadFromFile("assets/teapot.obj");
    if (!teapotData.empty()) {
        ObjMeshLoader::normalize(teapotData, 1.0f);
        const uint32_t teapotMesh = m_meshes->upload(teapotData);

        MeshComponent teapotComp{};
        teapotComp.mesh = teapotMesh;
        teapotComp.tint = {0.95f, 0.60f, 0.20f};
        teapotComp.metallic = 0.85f;
        teapotComp.roughness = 0.2f;

        spawnMeshEntity(m_registry, teapotComp, {1.8f, 0.0f, 0.0f}, {1.f, 1.f, 1.f});
    }

    const entt::entity camera = m_registry.create();
    m_registry.emplace<TransformLocal>(camera, TransformLocal{{0.f, 2.f, 6.f}});
    m_registry.emplace<TransformWorld>(camera);
    m_registry.emplace<CameraComponent>(camera);
    m_registry.emplace<FreeFlyController>(camera);
}

void SandboxApp::updateFrame(float deltaTime) {
    m_inputMap.beginFrame(m_input);
    updateFreeFlyCamera(m_registry, m_input, m_inputMap, deltaTime);

    for (int step = 0, fixedSteps = m_time.consumeFixedSteps(); step < fixedSteps; ++step) {
        m_physics.step(m_time.fixedDelta());
    }

    syncTransformsFromPhysics(m_registry, m_physics);
    updateTransforms(m_registry);
    m_renderer.tryReloadShaders();
}

bool SandboxApp::renderFrame() {
    const auto currentSize = m_platform.framebufferSize();
    if (currentSize.x == 0 || currentSize.y == 0) {
        return true;
    }

    uint32_t imageIndex = 0;
    const VkResult acquired = m_vulkan.acquireNextImage(&imageIndex);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        m_vulkan.handleResize(WindowResizeEvent{currentSize.x, currentSize.y});
        ImGui_ImplVulkan_SetMinImageCount(
            static_cast<uint32_t>(std::max(2, static_cast<int>(m_vulkan.framebuffers().size()))));
        return true;
    }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    VkCommandBuffer cmd = m_vulkan.commandBuffer(m_vulkan.currentFrame());
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.06f, 0.07f, 0.09f, 1.f}};
    clearValues[1].depthStencil = {1.f, 0};

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_vulkan.renderPass();
    renderPassInfo.framebuffer = m_vulkan.framebuffers()[imageIndex];
    renderPassInfo.renderArea.extent = m_vulkan.swapchainExtent();
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    const float aspect =
        static_cast<float>(m_vulkan.swapchainExtent().width) /
        static_cast<float>(std::max(1u, m_vulkan.swapchainExtent().height));
    const CameraState camera = findActiveCamera(m_registry, aspect);

    m_renderer.recordScene(cmd, m_registry, *m_meshes, *m_textures, camera);
    if (m_showDebug) {
        m_debugDraw.record(cmd, m_registry, camera);
    }

    m_imgui.beginFrame();
    ImGui::Begin("Sandbox");
    ImGui::Text("FPS: %.1f (avg %.1f)", m_time.fps(), m_time.smoothedFps());
    ImGui::Text("Frame: %llu  dt: %.2f ms", static_cast<unsigned long long>(m_time.frameIndex()),
                m_time.unscaledDelta() * 1000.f);
    ImGui::Text("Entities: %zu", m_registry.view<TransformLocal>().size());
    ImGui::Checkbox("Debug draw", &m_showDebug);
    bool cullBackfaces = (m_renderer.cullMode() != VK_CULL_MODE_NONE);
    if (ImGui::Checkbox("Cull backfaces", &cullBackfaces)) {
        m_renderer.setCullMode(cullBackfaces ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
    }
    ImGui::End();
    m_imgui.endFrame(cmd);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    const VkResult presentResult = m_vulkan.submitAndPresent(imageIndex);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        const auto size = m_platform.framebufferSize();
        if (size.x > 0 && size.y > 0) {
            m_vulkan.handleResize(WindowResizeEvent{size.x, size.y});
            ImGui_ImplVulkan_SetMinImageCount(
                static_cast<uint32_t>(std::max(2, static_cast<int>(m_vulkan.framebuffers().size()))));
        }
    }
    return true;
}

void SandboxApp::shutdownEngine() {
    vkDeviceWaitIdle(m_vulkan.device());
    destroyPhysicsBodies(m_registry, m_physics);
    m_debugDraw.shutdown();
    if (m_textures) {
        m_textures->shutdown();
    }
    m_renderer.shutdown();
    if (m_meshes) {
        m_meshes->clear();
    }
    m_imgui.shutdown();
    m_vulkan.shutdown();
    m_platform.shutdown();
}

int SandboxApp::run() {
    loadConfig();
    setupInput();
    if (!initEngine()) {
        return 1;
    }

    while (!m_platform.shouldClose()) {
        m_input.beginFrame();
        m_platform.pollEvents();
        m_time.tick();
        updateFrame(m_time.delta());
        if (!renderFrame()) {
            break;
        }
        m_input.endFrame();
    }

    shutdownEngine();
    return 0;
}

} // namespace engine
