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
    m_inputMap.bind(Action::SpawnBox, GLFW_KEY_B);
    m_inputMap.bind(Action::Jump, GLFW_KEY_SPACE);
    m_inputMap.bind(Action::ToggleCameraMode, GLFW_KEY_F1);
    m_inputMap.bind(Action::InspectObject, GLFW_KEY_I);
    m_inputMap.bind(Action::SpawnTeapot, GLFW_KEY_T);
    m_inputMap.bind(Action::ShootSphere, GLFW_KEY_F);
    m_inputMap.bind(Action::KickObject, GLFW_KEY_E);
    m_inputMap.bindMouse(Action::Look, GLFW_MOUSE_BUTTON_RIGHT);
    m_inputMap.bindMouse(Action::ShootSphere, GLFW_MOUSE_BUTTON_LEFT);
    m_inputMap.bindMouse(Action::KickObject, GLFW_MOUSE_BUTTON_MIDDLE);
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

    m_cubeComp.mesh = m_meshes->upload(MeshBuilder::box({0.5f, 0.5f, 0.5f}, {0.8f, 0.2f, 0.2f}));
    m_cubeComp.metallic = 0.1f;
    m_cubeComp.roughness = 0.8f;

    m_sphereComp.mesh = m_meshes->upload(MeshBuilder::sphere(0.4f, 24, 24, {0.2f, 0.65f, 0.95f}));
    m_sphereComp.tint = {0.25f, 0.75f, 1.0f};
    m_sphereComp.metallic = 0.9f;
    m_sphereComp.roughness = 0.15f;

    m_renderer.setLightDir(m_sunDirection);

    MeshCpuData teapotData = ObjMeshLoader::loadFromFile("assets/teapot.obj");
    if (!teapotData.empty()) {
        ObjMeshLoader::normalize(teapotData, 1.0f);
        m_teapotComp.mesh = m_meshes->upload(teapotData);
        m_teapotComp.tint = {0.95f, 0.60f, 0.20f};
        m_teapotComp.metallic = 0.85f;
        m_teapotComp.roughness = 0.2f;
        
        m_teapotVertices.reserve(teapotData.vertices.size());
        for (const auto& v : teapotData.vertices) {
            m_teapotVertices.push_back(v.position);
        }
        
        m_hasTeapot = true;

        spawnMeshEntity(m_registry, m_teapotComp, {1.8f, 0.0f, 0.0f}, {1.f, 1.f, 1.f});
    }

    // Demo Point Lights
    const uint32_t lightMarkerMesh = m_meshes->upload(MeshBuilder::sphere(0.12f, 16, 16, {1.f, 1.f, 1.f}));

    // 1. Warm gold light near teapots
    {
        const entt::entity light1 = m_registry.create();
        m_registry.emplace<TransformLocal>(light1, TransformLocal{{2.2f, 1.6f, 0.6f}});
        m_registry.emplace<TransformWorld>(light1);
        m_registry.emplace<PointLightComponent>(light1, PointLightComponent{
            .color = {1.0f, 0.65f, 0.2f},
            .intensity = 15.0f,
            .radius = 8.0f
        });
        MeshComponent markerComp{};
        markerComp.mesh = lightMarkerMesh;
        markerComp.tint = {1.0f, 0.65f, 0.2f};
        markerComp.metallic = 0.1f;
        markerComp.roughness = 0.2f;
        m_registry.emplace<MeshComponent>(light1, markerComp);
        m_registry.emplace<RenderableTag>(light1);
        m_demoPointLights.push_back(light1);
    }

    // 2. Cyan light near character / center
    {
        const entt::entity light2 = m_registry.create();
        m_registry.emplace<TransformLocal>(light2, TransformLocal{{-2.0f, 2.0f, -0.5f}});
        m_registry.emplace<TransformWorld>(light2);
        m_registry.emplace<PointLightComponent>(light2, PointLightComponent{
            .color = {0.2f, 0.75f, 1.0f},
            .intensity = 18.0f,
            .radius = 10.0f
        });
        MeshComponent markerComp{};
        markerComp.mesh = lightMarkerMesh;
        markerComp.tint = {0.2f, 0.75f, 1.0f};
        markerComp.metallic = 0.1f;
        markerComp.roughness = 0.2f;
        m_registry.emplace<MeshComponent>(light2, markerComp);
        m_registry.emplace<RenderableTag>(light2);
        m_demoPointLights.push_back(light2);
    }

    const entt::entity camera = m_registry.create();
    m_registry.emplace<TransformLocal>(camera, TransformLocal{{0.f, 2.f, 6.f}});
    m_registry.emplace<TransformWorld>(camera);
    m_registry.emplace<CameraComponent>(camera);
    m_registry.emplace<FreeFlyController>(camera);

    m_character.init(m_physics, {0.0f, 0.5f, 6.0f});
}

void SandboxApp::toggleCameraMode() {
    auto view = m_registry.view<TransformLocal, CameraComponent, FreeFlyController>();
    if (view.begin() == view.end()) return;
    auto camEntity = *view.begin();
    auto& camTransform = view.get<TransformLocal>(camEntity);

    if (m_cameraMode == CameraMode::FreeFly) {
        m_cameraMode = CameraMode::FirstPerson;
        glm::vec3 charPos = camTransform.translation - glm::vec3(0.0f, m_character.eyeHeight, 0.0f);
        m_character.setPosition(charPos);
        log(LogLevel::Info, "Camera mode: FirstPerson (FPS Controller)");
    } else {
        m_cameraMode = CameraMode::FreeFly;
        log(LogLevel::Info, "Camera mode: FreeFly");
    }
}

void SandboxApp::inspectObjectUnderCrosshair() {
    auto view = m_registry.view<TransformLocal, CameraComponent>();
    if (view.begin() == view.end()) return;
    auto camEntity = *view.begin();
    const auto& camTransform = view.get<TransformLocal>(camEntity);

    const auto* controller = m_registry.try_get<FreeFlyController>(camEntity);
    const float yaw = controller ? controller->yaw : 0.f;
    const float pitch = controller ? controller->pitch : 0.f;

    const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                            std::sin(yaw) * std::cos(pitch)};

    RaycastHit hit;
    if (raycast(m_physics, m_registry, camTransform.translation, forward, 100.f, hit)) {
        if (hit.entity != entt::null) {
            m_selectedEntity = hit.entity;
            log(LogLevel::Info, "Inspected entity: " + std::to_string(static_cast<uint32_t>(hit.entity)));
        }
    }
}

void SandboxApp::spawnDynamicObject(const MeshComponent& meshComp, const glm::vec3& halfExtents) {
    auto view = m_registry.view<TransformLocal, CameraComponent>();
    if (view.begin() == view.end()) return;
    auto camEntity = *view.begin();
    const auto& camTransform = view.get<TransformLocal>(camEntity);

    const auto* controller = m_registry.try_get<FreeFlyController>(camEntity);
    const float yaw = controller ? controller->yaw : 0.f;
    const float pitch = controller ? controller->pitch : 0.f;

    const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                            std::sin(yaw) * std::cos(pitch)};

    glm::vec3 spawnPos = camTransform.translation + (forward * 2.0f);

    const entt::entity entity = m_registry.create();
    m_registry.emplace<TransformLocal>(entity, TransformLocal{spawnPos});
    m_registry.emplace<TransformWorld>(entity);
    m_registry.emplace<MeshComponent>(entity, meshComp);
    m_registry.emplace<RenderableTag>(entity);
    m_registry.emplace<RigidBodyComponent>(entity);
    createDynamicBox(m_physics, m_registry, entity, halfExtents, 1.0f);
}

void SandboxApp::spawnDynamicConvexObject(const MeshComponent& meshComp, const std::vector<glm::vec3>& vertices) {
    auto view = m_registry.view<TransformLocal, CameraComponent>();
    if (view.begin() == view.end()) return;
    auto camEntity = *view.begin();
    const auto& camTransform = view.get<TransformLocal>(camEntity);

    const auto* controller = m_registry.try_get<FreeFlyController>(camEntity);
    const float yaw = controller ? controller->yaw : 0.f;
    const float pitch = controller ? controller->pitch : 0.f;

    const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                            std::sin(yaw) * std::cos(pitch)};

    glm::vec3 spawnPos = camTransform.translation + (forward * 2.0f);

    const entt::entity entity = m_registry.create();
    m_registry.emplace<TransformLocal>(entity, TransformLocal{spawnPos});
    m_registry.emplace<TransformWorld>(entity);
    m_registry.emplace<MeshComponent>(entity, meshComp);
    m_registry.emplace<RenderableTag>(entity);
    m_registry.emplace<RigidBodyComponent>(entity);
    createDynamicConvexHull(m_physics, m_registry, entity, vertices, 1.0f);
}

void SandboxApp::shootSphere() {
    auto view = m_registry.view<TransformLocal, CameraComponent>();
    if (view.begin() == view.end()) return;
    auto camEntity = *view.begin();
    const auto& camTransform = view.get<TransformLocal>(camEntity);

    const auto* controller = m_registry.try_get<FreeFlyController>(camEntity);
    const float yaw = controller ? controller->yaw : 0.f;
    const float pitch = controller ? controller->pitch : 0.f;

    const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                            std::sin(yaw) * std::cos(pitch)};

    const glm::vec3 spawnPos = camTransform.translation + (forward * 1.5f);
    const glm::vec3 velocity = forward * 30.f;

    const entt::entity entity = m_registry.create();
    m_registry.emplace<TransformLocal>(entity, TransformLocal{spawnPos});
    m_registry.emplace<TransformWorld>(entity);
    m_registry.emplace<MeshComponent>(entity, m_sphereComp);
    m_registry.emplace<RenderableTag>(entity);
    m_registry.emplace<RigidBodyComponent>(entity);
    createDynamicSphere(m_physics, m_registry, entity, 0.4f, 4.0f, velocity);
}

void SandboxApp::kickObjectUnderCrosshair() {
    auto view = m_registry.view<TransformLocal, CameraComponent>();
    if (view.begin() == view.end()) return;
    auto camEntity = *view.begin();
    const auto& camTransform = view.get<TransformLocal>(camEntity);

    const auto* controller = m_registry.try_get<FreeFlyController>(camEntity);
    const float yaw = controller ? controller->yaw : 0.f;
    const float pitch = controller ? controller->pitch : 0.f;

    const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                            std::sin(yaw) * std::cos(pitch)};

    RaycastHit hit;
    if (raycast(m_physics, m_registry, camTransform.translation, forward, 100.f, hit)) {
        if (hit.entity != entt::null) {
            const glm::vec3 impulse = (forward * 35.f + glm::vec3(0.f, 12.f, 0.f));
            applyImpulse(m_physics, m_registry, hit.entity, impulse, hit.position);
        }
    }
}

void SandboxApp::clearSpawnedObjects() {
    clearDynamicBodies(m_registry, m_physics);
}

void SandboxApp::updateFrame(float deltaTime) {
    m_inputMap.beginFrame(m_input);

    const bool mouseCaptured = ImGui::GetIO().WantCaptureMouse;
    const bool keyboardCaptured = ImGui::GetIO().WantCaptureKeyboard;

    if (!keyboardCaptured) {
        if (m_inputMap.actionPressed(m_input, Action::ToggleCameraMode)) {
            toggleCameraMode();
        }
        if (m_inputMap.actionPressed(m_input, Action::InspectObject)) {
            inspectObjectUnderCrosshair();
        }
    }

    if (m_cameraMode == CameraMode::FirstPerson) {
        auto view = m_registry.view<TransformLocal, CameraComponent, FreeFlyController>();
        for (const auto camEntity : view) {
            auto& camTransform = view.get<TransformLocal>(camEntity);
            auto& controller = view.get<FreeFlyController>(camEntity);

            if (!mouseCaptured && m_inputMap.actionDown(m_input, Action::Look)) {
                const glm::vec2 delta = m_inputMap.lookDelta();
                controller.yaw += delta.x * controller.lookSensitivity;
                controller.pitch -= delta.y * controller.lookSensitivity;
                controller.pitch = std::clamp(controller.pitch, -1.4f, 1.4f);
            }

            glm::vec2 moveInput{0.0f};
            bool jump = false;
            if (!keyboardCaptured) {
                if (m_inputMap.actionDown(m_input, Action::MoveForward)) moveInput.y += 1.0f;
                if (m_inputMap.actionDown(m_input, Action::MoveBack))    moveInput.y -= 1.0f;
                if (m_inputMap.actionDown(m_input, Action::MoveRight))   moveInput.x += 1.0f;
                if (m_inputMap.actionDown(m_input, Action::MoveLeft))    moveInput.x -= 1.0f;
                if (m_inputMap.actionPressed(m_input, Action::Jump))     jump = true;
            }

            m_character.update(m_physics, deltaTime, moveInput, controller.yaw, jump);
            camTransform.translation = m_character.position() + glm::vec3(0.0f, m_character.eyeHeight, 0.0f);
        }
    } else {
        updateFreeFlyCamera(m_registry, m_input, m_inputMap, deltaTime);
    }

    if (!keyboardCaptured) {
        if (m_inputMap.actionPressed(m_input, Action::SpawnBox) ||
            (m_cameraMode == CameraMode::FreeFly && m_input.keyPressed(GLFW_KEY_SPACE))) {
            spawnDynamicObject(m_cubeComp, {0.5f, 0.5f, 0.5f});
        }
        if (m_hasTeapot && m_inputMap.actionPressed(m_input, Action::SpawnTeapot)) {
            spawnDynamicConvexObject(m_teapotComp, m_teapotVertices);
        }
    }

    if (!mouseCaptured) {
        if (m_inputMap.actionPressed(m_input, Action::ShootSphere)) {
            shootSphere();
        }
        if (m_inputMap.actionPressed(m_input, Action::KickObject)) {
            kickObjectUnderCrosshair();
        }
    }

    if (m_animateLights) {
        m_lightAnimTime += deltaTime;
        if (m_demoPointLights.size() >= 2) {
            if (m_registry.valid(m_demoPointLights[0])) {
                if (auto* t = m_registry.try_get<TransformLocal>(m_demoPointLights[0])) {
                    t->translation.x = 2.0f + 1.2f * std::cos(m_lightAnimTime * 1.5f);
                    t->translation.z = 0.5f + 1.2f * std::sin(m_lightAnimTime * 1.5f);
                    t->translation.y = 1.4f + 0.3f * std::sin(m_lightAnimTime * 2.0f);
                }
            }
            if (m_registry.valid(m_demoPointLights[1])) {
                if (auto* t = m_registry.try_get<TransformLocal>(m_demoPointLights[1])) {
                    t->translation.x = -1.8f + 1.5f * std::cos(m_lightAnimTime * -1.2f);
                    t->translation.z = -0.5f + 1.5f * std::sin(m_lightAnimTime * -1.2f);
                    t->translation.y = 1.8f + 0.4f * std::cos(m_lightAnimTime * 1.8f);
                }
            }
        }
    }

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

    m_renderer.recordShadowPass(cmd, m_registry, *m_meshes);

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

    size_t dynamicBodiesCount = 0;
    for (const auto entity : m_registry.view<const RigidBodyComponent>()) {
        if (m_registry.get<const RigidBodyComponent>(entity).dynamic) {
            ++dynamicBodiesCount;
        }
    }
    ImGui::Text("Dynamic Bodies: %zu", dynamicBodiesCount);

    ImGui::Separator();
    ImGui::Text("Camera Mode (F1):");
    int currentMode = (m_cameraMode == CameraMode::FreeFly) ? 0 : 1;
    if (ImGui::RadioButton("Free-Fly", &currentMode, 0)) {
        if (m_cameraMode != CameraMode::FreeFly) toggleCameraMode();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("FPS Character", &currentMode, 1)) {
        if (m_cameraMode != CameraMode::FirstPerson) toggleCameraMode();
    }
    if (m_cameraMode == CameraMode::FirstPerson) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "WASD = Move, Space = Jump, RMB = Look");
        ImGui::Text("Grounded: %s", m_character.isGrounded() ? "YES" : "NO");
        const glm::vec3 p = m_character.position();
        ImGui::Text("Pos: (%.2f, %.2f, %.2f)", p.x, p.y, p.z);
    }

    ImGui::Separator();
    ImGui::Text("Physics Actions:");
    if (ImGui::Button("Spawn Cube (B / Space)")) {
        spawnDynamicObject(m_cubeComp, {0.5f, 0.5f, 0.5f});
    }
    ImGui::SameLine();
    if (m_hasTeapot && ImGui::Button("Spawn Teapot (T)")) {
        spawnDynamicConvexObject(m_teapotComp, m_teapotVertices);
    }
    ImGui::SameLine();
    if (ImGui::Button("Shoot Cannonball (LMB / F)")) {
        shootSphere();
    }

    if (ImGui::Button("Kick Crosshair (MMB / E)")) {
        kickObjectUnderCrosshair();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Dynamic Objects")) {
        if (m_selectedEntity != entt::null && m_registry.valid(m_selectedEntity)) {
            if (const auto* body = m_registry.try_get<RigidBodyComponent>(m_selectedEntity); body && body->dynamic) {
                m_selectedEntity = entt::null;
            }
        }
        clearSpawnedObjects();
    }

    ImGui::Separator();
    ImGui::Text("Entity & Material Inspector:");
    if (ImGui::Button("Inspect Under Crosshair (I)")) {
        inspectObjectUnderCrosshair();
    }

    if (m_selectedEntity != entt::null && m_registry.valid(m_selectedEntity)) {
        ImGui::Text("Selected Entity: %u", static_cast<uint32_t>(m_selectedEntity));

        if (auto* transform = m_registry.try_get<TransformLocal>(m_selectedEntity)) {
            ImGui::DragFloat3("Position", &transform->translation.x, 0.05f);
            ImGui::DragFloat3("Scale", &transform->scale.x, 0.05f, 0.01f, 100.0f);
        }

        if (auto* meshComp = m_registry.try_get<MeshComponent>(m_selectedEntity)) {
            ImGui::ColorEdit3("Tint", &meshComp->tint.x);
            ImGui::SliderFloat("Metallic", &meshComp->metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &meshComp->roughness, 0.0f, 1.0f);
        }

        if (auto* lightComp = m_registry.try_get<PointLightComponent>(m_selectedEntity)) {
            ImGui::Separator();
            ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Point Light Component");
            if (ImGui::ColorEdit3("Light Color", &lightComp->color.x)) {
                if (auto* mc = m_registry.try_get<MeshComponent>(m_selectedEntity)) {
                    mc->tint = lightComp->color;
                }
            }
            ImGui::SliderFloat("Light Intensity", &lightComp->intensity, 0.0f, 50.0f);
            ImGui::SliderFloat("Light Radius", &lightComp->radius, 0.5f, 30.0f);
        }

        if (ImGui::Button("Deselect")) {
            m_selectedEntity = entt::null;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete Entity")) {
            destroyPhysicsBody(m_physics, m_registry, m_selectedEntity);
            m_registry.destroy(m_selectedEntity);
            m_selectedEntity = entt::null;
        }
    } else {
        ImGui::TextDisabled("No entity selected. Press 'I' while aiming at an object.");
    }

    ImGui::Separator();
    ImGui::Text("Lighting & Render:");
    if (ImGui::SliderFloat3("Sun Dir", &m_sunDirection.x, -1.f, 1.f)) {
        m_renderer.setLightDir(m_sunDirection);
    }
    ImGui::Checkbox("Animate Demo Lights", &m_animateLights);
    if (ImGui::Button("Add Light at Camera")) {
        auto camView = m_registry.view<TransformLocal, CameraComponent>();
        if (camView.begin() != camView.end()) {
            const auto camEnt = *camView.begin();
            const auto& camTransform = camView.get<TransformLocal>(camEnt);
            const entt::entity newLight = m_registry.create();
            m_registry.emplace<TransformLocal>(newLight, TransformLocal{camTransform.translation});
            m_registry.emplace<TransformWorld>(newLight);
            m_registry.emplace<PointLightComponent>(newLight, PointLightComponent{
                .color = {1.0f, 0.9f, 0.7f},
                .intensity = 15.0f,
                .radius = 8.0f
            });
            MeshComponent markerComp{};
            markerComp.mesh = m_sphereComp.mesh;
            markerComp.tint = {1.0f, 0.9f, 0.7f};
            markerComp.metallic = 0.1f;
            markerComp.roughness = 0.2f;
            m_registry.emplace<MeshComponent>(newLight, markerComp);
            m_registry.emplace<RenderableTag>(newLight);
            m_selectedEntity = newLight;
        }
    }
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
