#include "SandboxModule.hpp"

#include "engine/assets/GltfMeshLoader.hpp"
#include "engine/assets/GltfTextureLoader.hpp"
#include "engine/assets/MeshBuilder.hpp"
#include "engine/assets/ObjMeshLoader.hpp"
#include "engine/audio/AudioEngine.hpp"
#include "engine/core/Events.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Path.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/ecs/Systems.hpp"
#include "engine/integration/ImGuiLayer.hpp"
#include "engine/ui/ComponentInspector.hpp"
#include "engine/ui/EditorChrome.hpp"
#include "engine/ui/TransformGizmo.hpp"
#include "engine/modules/save/SaveModule.hpp"
#include "engine/integration/PlatformGLFW.hpp"
#include "engine/physics/CharacterController.hpp"
#include "engine/physics/JoltWorld.hpp"
#include "engine/renderer/vulkan/GpuMeshCache.hpp"
#include "engine/renderer/vulkan/GpuTextureCache.hpp"
#include "engine/renderer/vulkan/PbrRenderer.hpp"
#include "engine/renderer/vulkan/PostProcessPipeline.hpp"
#include "engine/renderer/vulkan/ShaderHotReload.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"
#include "engine/vfx/ParticleSystem.hpp"
#include "engine/world/bridges/PhysicsBridge.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>

namespace engine::sandbox {

namespace {

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

std::string shaderPath(const char* name) {
    return std::string(SHADER_DIR) + "/" + name;
}

void handleSceneRequests(World& world, engine::ui::EditorHistory& history,
                         engine::ui::SceneSaveControls& scene, glm::vec3& sun) {
    if (!scene.saveRequested && !scene.loadRequested) {
        return;
    }
    const std::filesystem::path path = scene.path;
    if (scene.saveRequested) {
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        if (SaveModule::saveSceneJson(path, world.registry(), world.types(), sun)) {
            scene.status = "Saved " + path.string();
        } else {
            scene.status = "Save failed: " + path.string();
        }
    }
    if (scene.loadRequested) {
        if (SaveModule::loadSceneJson(path, world.registry(), world.types(), sun)) {
            history.clear();
            scene.status = "Loaded " + path.string() + " (physics bodies not rebuilt)";
        } else {
            scene.status = "Load failed: " + path.string();
        }
    }
}

void tickEditorGizmo(engine::ui::GizmoState& gizmo, engine::ui::EditorHistory& history,
                     bool& wasDragging, glm::vec3& startT, glm::quat& startR,
                     entt::registry& registry, entt::entity selected, const CameraState& camera,
                     const Input& input, const glm::vec2& viewport, bool allowPick) {
    if (selected == entt::null || !registry.valid(selected)) {
        return;
    }
    auto* transform = registry.try_get<TransformLocal>(selected);
    if (!transform) {
        return;
    }

    engine::ui::GizmoCamera gizmoCam;
    gizmoCam.view = camera.view;
    gizmoCam.proj = camera.proj;
    gizmoCam.viewProj = camera.viewProj;
    gizmoCam.position = camera.position;
    gizmoCam.viewport = viewport;

    engine::ui::GizmoInput gizmoInput;
    gizmoInput.mouse = input.snapshot().mousePosition;
    gizmoInput.leftDown = input.mouseButtonDown(0);
    gizmoInput.leftPressed = allowPick && input.mouseButtonPressed(0);
    gizmoInput.leftReleased = !input.mouseButtonDown(0);

    if (!gizmo.dragging) {
        startT = transform->translation;
        startR = transform->rotation;
    }
    engine::ui::manipulateTransform(gizmo, transform->translation, transform->rotation, gizmoCam, gizmoInput);
    if (wasDragging && !gizmo.dragging) {
        engine::ui::commitTransformDiff(history, selected, startT, startR, transform->translation,
                                        transform->rotation);
    }
    wasDragging = gizmo.dragging;
    engine::ui::drawGizmoOverlay(gizmo, transform->translation, gizmoCam);
}

} // namespace

SandboxModule::SandboxModule(bool smokeTest)
    : m_smokeTest(smokeTest) {
}

SandboxModule::~SandboxModule() = default;

void SandboxModule::loadConfig() {
    const std::filesystem::path cfgPath = resolvePath("engine_config.json");
    if (std::filesystem::exists(cfgPath)) {
        m_config.loadFromFile(cfgPath);
    }
    setMinLogLevel(static_cast<LogLevel>(m_config.getInt("log.level", static_cast<int>(LogLevel::Info))));
    m_showDebug = m_config.getBool("debug.draw", true);
}

void SandboxModule::setupInput() {
    m_inputMap.bind(Actions::MoveForward, GLFW_KEY_W);
    m_inputMap.bind(Actions::MoveBack, GLFW_KEY_S);
    m_inputMap.bind(Actions::MoveLeft, GLFW_KEY_A);
    m_inputMap.bind(Actions::MoveRight, GLFW_KEY_D);
    m_inputMap.bind(Actions::Sprint, GLFW_KEY_LEFT_SHIFT);
    m_inputMap.bind(Actions::Jump, GLFW_KEY_SPACE);
    m_inputMap.bindMouse(Actions::Look, GLFW_MOUSE_BUTTON_RIGHT);

    m_inputMap.bind(SandboxActions::SpawnBox, GLFW_KEY_B);
    m_inputMap.bind(SandboxActions::ToggleCameraMode, GLFW_KEY_F1);
    m_inputMap.bind(SandboxActions::ToggleCursor, GLFW_KEY_ESCAPE);
    m_inputMap.bind(SandboxActions::InspectObject, GLFW_KEY_I);
    m_inputMap.bind(SandboxActions::SpawnMesh, GLFW_KEY_T);
    m_inputMap.bind(SandboxActions::ShootSphere, GLFW_KEY_F);
    m_inputMap.bind(SandboxActions::KickObject, GLFW_KEY_E);
    m_inputMap.bindMouse(SandboxActions::ShootSphere, GLFW_MOUSE_BUTTON_LEFT);
    m_inputMap.bindMouse(SandboxActions::KickObject, GLFW_MOUSE_BUTTON_MIDDLE);
}

void SandboxModule::registerTypes(TypeRegistry& registry) {
    registerEngineComponents(registry);
}

void SandboxModule::onAttach(World& world) {
    loadConfig();
    setupInput();

    auto& vulkan = world.resource<VulkanContext>();
    m_debugDraw.init(vulkan);

    shaderHotReloadWatchPath(shaderPath("pbr.vert").c_str());
    shaderHotReloadWatchPath(shaderPath("pbr.frag").c_str());

    auto& physics = world.resource<JoltWorld>();
    auto& particles = world.resource<ParticleSystem>();

    physics.setContactCallback([&particles](const glm::vec3& pos, float speed) {
        const float volume = std::clamp(speed / 12.0f, 0.15f, 1.0f);
        AudioEngine::instance().play3D("assets/sounds/impact.wav", pos, volume, 1.0f, 35.0f);
        particles.spawnImpactSparks(pos, glm::vec3(0.f, 1.f, 0.f), speed / 6.0f);
    });

    spawnScene(world);
    setCursorCapture(world, true);
    m_sceneControls.path = "saves/sandbox_scene.json";
}

void SandboxModule::onDetach(World& world) {
    auto& vulkan = world.resource<VulkanContext>();
    vkDeviceWaitIdle(vulkan.device());
    m_debugDraw.shutdown();
}

void SandboxModule::setCursorCapture(World& world, bool capture) {
    m_cursorCaptured = capture;
    world.resource<PlatformGLFW>().setCursorCaptured(capture);
}

void SandboxModule::respawnPlayer(World& world) {
    auto& character = world.resource<CharacterController>();
    character.setPosition(m_spawnPoint);

    auto& registry = world.registry();
    auto view = registry.view<TransformLocal, CameraComponent>();
    for (const auto camEnt : view) {
        auto& t = view.get<TransformLocal>(camEnt);
        t.translation = m_spawnPoint + glm::vec3(0.0f, character.eyeHeight, 0.0f);
        if (auto* ctrl = registry.try_get<FreeFlyController>(camEnt)) {
            ctrl->yaw = -1.5707963f;
            ctrl->pitch = 0.0f;
        }
    }
    AudioEngine::instance().play2D("assets/sounds/test.wav", 0.6f);
    log(LogLevel::Info, "Player reached boundary / killzone and was safely respawned at arena origin.");
}

void SandboxModule::spawnScene(World& world) {
    m_demoPointLights.clear();
    auto& registry = world.registry();
    auto& meshes = world.resource<GpuMeshCache>();
    auto& physics = world.resource<JoltWorld>();
    auto& character = world.resource<CharacterController>();

    const float arenaHalf = 50.0f;
    const float wallHeight = 6.0f;
    const float wallHalfH = wallHeight * 0.5f;

    auto createStaticBoxEntity = [&](const std::string& name, const glm::vec3& pos, const glm::vec3& halfExtents,
                                     uint32_t meshId, const glm::vec3& tint, float roughness = 0.5f,
                                     float metallic = 0.0f, float emissive = 0.0f) -> entt::entity {
        const entt::entity ent = registry.create();
        registry.emplace<TagComponent>(ent, TagComponent{name});
        registry.emplace<TransformLocal>(ent, TransformLocal{pos});
        registry.emplace<TransformWorld>(ent);
        MeshComponent mc{};
        mc.mesh = meshId;
        mc.tint = tint;
        mc.roughness = roughness;
        mc.metallic = metallic;
        mc.emissiveIntensity = emissive;
        registry.emplace<MeshComponent>(ent, mc);
        registry.emplace<MeshGeometryComponent>(ent, MeshGeometryComponent{MeshGeometryType::Box, "", halfExtents});
        registry.emplace<RenderableTag>(ent);
        registry.emplace<StaticColliderTag>(ent);
        registry.emplace<RigidBodyComponent>(ent, RigidBodyComponent{.bodyIndex = UINT32_MAX, .dynamic = false});
        registry.emplace<ColliderComponent>(ent, ColliderComponent{ColliderShapeType::Box, halfExtents, 0.0f, true, 0.0f});
        createStaticBox(physics, registry, ent, halfExtents);
        return ent;
    };

    // 1. Arena Floor (100x100m)
    const uint32_t floorMesh = meshes.upload(MeshBuilder::plane(arenaHalf, {0.16f, 0.18f, 0.22f}));
    const entt::entity floorVisual = spawnMeshEntity(registry, floorMesh, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f});
    registry.emplace<TagComponent>(floorVisual, TagComponent{"ArenaFloorVisual"});
    registry.emplace<MeshGeometryComponent>(floorVisual, MeshGeometryComponent{MeshGeometryType::Plane, "", {arenaHalf, 0.f, 0.f}});

    const entt::entity floorCollider = registry.create();
    registry.emplace<TagComponent>(floorCollider, TagComponent{"ArenaFloorCollider"});
    registry.emplace<TransformLocal>(floorCollider, TransformLocal{{0.f, -0.5f, 0.f}});
    registry.emplace<StaticColliderTag>(floorCollider);
    registry.emplace<RigidBodyComponent>(floorCollider, RigidBodyComponent{.bodyIndex = UINT32_MAX, .dynamic = false});
    registry.emplace<ColliderComponent>(floorCollider, ColliderComponent{ColliderShapeType::Box, {arenaHalf, 0.5f, arenaHalf}, 0.f, true, 0.f});
    createStaticBox(physics, registry, floorCollider, {arenaHalf, 0.5f, arenaHalf});

    // 2. Perimeter Boundary Walls (North, South, East, West - 6m high)
    const uint32_t wallMeshX = meshes.upload(MeshBuilder::box({arenaHalf, wallHalfH, 0.5f}, {0.14f, 0.16f, 0.20f}));
    const uint32_t wallMeshZ = meshes.upload(MeshBuilder::box({0.5f, wallHalfH, arenaHalf}, {0.14f, 0.16f, 0.20f}));
    const uint32_t trimMeshX = meshes.upload(MeshBuilder::box({arenaHalf, 0.08f, 0.55f}, {0.1f, 0.75f, 1.0f}));
    const uint32_t trimMeshZ = meshes.upload(MeshBuilder::box({0.55f, 0.08f, arenaHalf}, {0.1f, 0.75f, 1.0f}));

    createStaticBoxEntity("WallNorth", {0.f, wallHalfH, -arenaHalf}, {arenaHalf, wallHalfH, 0.5f}, wallMeshX, {0.14f, 0.16f, 0.20f}, 0.7f);
    createStaticBoxEntity("WallSouth", {0.f, wallHalfH, arenaHalf}, {arenaHalf, wallHalfH, 0.5f}, wallMeshX, {0.14f, 0.16f, 0.20f}, 0.7f);
    createStaticBoxEntity("WallEast", {arenaHalf, wallHalfH, 0.f}, {0.5f, wallHalfH, arenaHalf}, wallMeshZ, {0.14f, 0.16f, 0.20f}, 0.7f);
    createStaticBoxEntity("WallWest", {-arenaHalf, wallHalfH, 0.f}, {0.5f, wallHalfH, arenaHalf}, wallMeshZ, {0.14f, 0.16f, 0.20f}, 0.7f);

    createStaticBoxEntity("TrimNorth", {0.f, wallHeight + 0.08f, -arenaHalf}, {arenaHalf, 0.08f, 0.55f}, trimMeshX, {0.1f, 0.75f, 1.0f}, 0.2f, 0.1f, 6.0f);
    createStaticBoxEntity("TrimSouth", {0.f, wallHeight + 0.08f, arenaHalf}, {arenaHalf, 0.08f, 0.55f}, trimMeshX, {0.1f, 0.75f, 1.0f}, 0.2f, 0.1f, 6.0f);
    createStaticBoxEntity("TrimEast", {arenaHalf, wallHeight + 0.08f, 0.f}, {0.55f, 0.08f, arenaHalf}, trimMeshZ, {0.1f, 0.75f, 1.0f}, 0.2f, 0.1f, 6.0f);
    createStaticBoxEntity("TrimWest", {-arenaHalf, wallHeight + 0.08f, 0.f}, {0.55f, 0.08f, arenaHalf}, trimMeshZ, {0.1f, 0.75f, 1.0f}, 0.2f, 0.1f, 6.0f);

    // 3. Central Showroom Pavilion Platform (48x0.8x28m)
    const glm::vec3 platformCenter{0.f, 0.4f, -6.f};
    const glm::vec3 platformHalf{24.0f, 0.4f, 14.0f};
    const uint32_t platformMesh = meshes.upload(MeshBuilder::box(platformHalf, {0.22f, 0.24f, 0.28f}));
    createStaticBoxEntity("CentralPlatform", platformCenter, platformHalf, platformMesh, {0.22f, 0.24f, 0.28f}, 0.3f, 0.3f);

    // Platform Stairs on South Side
    const uint32_t stepMesh1 = meshes.upload(MeshBuilder::box({8.0f, 0.13f, 0.7f}, {0.26f, 0.28f, 0.32f}));
    const uint32_t stepMesh2 = meshes.upload(MeshBuilder::box({8.0f, 0.26f, 0.6f}, {0.26f, 0.28f, 0.32f}));
    const uint32_t stepMesh3 = meshes.upload(MeshBuilder::box({8.0f, 0.39f, 0.5f}, {0.26f, 0.28f, 0.32f}));
    createStaticBoxEntity("PlatformStep1", {0.f, 0.13f, 9.4f}, {8.0f, 0.13f, 0.7f}, stepMesh1, {0.26f, 0.28f, 0.32f}, 0.4f);
    createStaticBoxEntity("PlatformStep2", {0.f, 0.26f, 8.4f}, {8.0f, 0.26f, 0.6f}, stepMesh2, {0.26f, 0.28f, 0.32f}, 0.4f);
    createStaticBoxEntity("PlatformStep3", {0.f, 0.39f, 7.6f}, {8.0f, 0.39f, 0.5f}, stepMesh3, {0.26f, 0.28f, 0.32f}, 0.4f);

    // 4. Architectural Columns with Emissive Neon Rings & Point Lights
    const uint32_t colMesh = meshes.upload(MeshBuilder::box({0.7f, 3.5f, 0.7f}, {0.18f, 0.20f, 0.24f}));
    const uint32_t beaconRingMesh = meshes.upload(MeshBuilder::box({0.85f, 0.20f, 0.85f}, {1.0f, 1.0f, 1.0f}));
    const uint32_t lightMarkerMesh = meshes.upload(MeshBuilder::sphere(0.20f, 16, 16, {1.f, 1.f, 1.f}));

    struct PillarConfig {
        glm::vec3 pos;
        glm::vec3 color;
        const char* name;
    };
    const PillarConfig pillars[4] = {
        { {-20.f, 3.5f, -17.f}, {0.1f, 0.85f, 1.0f}, "Pillar_Cyan" },
        { { 20.f, 3.5f, -17.f}, {1.0f, 0.20f, 0.85f}, "Pillar_Magenta" },
        { {-20.f, 3.5f,   5.f}, {1.0f, 0.70f, 0.15f}, "Pillar_Amber" },
        { { 20.f, 3.5f,   5.f}, {0.15f, 1.0f, 0.50f}, "Pillar_Emerald" },
    };

    for (const auto& p : pillars) {
        createStaticBoxEntity(std::string(p.name) + "_Base", p.pos, {0.7f, 3.5f, 0.7f}, colMesh, {0.18f, 0.20f, 0.24f}, 0.5f);
        createStaticBoxEntity(std::string(p.name) + "_Neon", {p.pos.x, 6.2f, p.pos.z}, {0.85f, 0.20f, 0.85f}, beaconRingMesh, p.color, 0.1f, 0.1f, 12.0f);

        const entt::entity lightEnt = registry.create();
        registry.emplace<TagComponent>(lightEnt, TagComponent{std::string(p.name) + "_Light"});
        registry.emplace<TransformLocal>(lightEnt, TransformLocal{{p.pos.x, 6.7f, p.pos.z}});
        registry.emplace<TransformWorld>(lightEnt);
        registry.emplace<PointLightComponent>(lightEnt, PointLightComponent{
            .color = p.color,
            .intensity = 26.0f,
            .radius = 18.0f
        });
        MeshComponent markerComp{};
        markerComp.mesh = lightMarkerMesh;
        markerComp.tint = p.color;
        markerComp.metallic = 0.1f;
        markerComp.roughness = 0.1f;
        markerComp.emissiveIntensity = 10.0f;
        registry.emplace<MeshComponent>(lightEnt, markerComp);
        registry.emplace<MeshGeometryComponent>(lightEnt, MeshGeometryComponent{MeshGeometryType::Sphere, "", {0.20f, 0.f, 0.f}});
        registry.emplace<RenderableTag>(lightEnt);
        m_demoPointLights.push_back(lightEnt);
    }

    // 5. Physics Playground (Dynamic Crates Pyramid on Left Wing, X = -34)
    m_cubeComp.mesh = meshes.upload(MeshBuilder::box({0.5f, 0.5f, 0.5f}, {0.75f, 0.45f, 0.25f}));
    m_cubeComp.metallic = 0.1f;
    m_cubeComp.roughness = 0.7f;

    m_sphereComp.mesh = meshes.upload(MeshBuilder::sphere(0.4f, 24, 24, {0.2f, 0.75f, 1.0f}));
    m_sphereComp.tint = {0.25f, 0.75f, 1.0f};
    m_sphereComp.metallic = 0.9f;
    m_sphereComp.roughness = 0.15f;

    auto spawnDynamicCrate = [&](const glm::vec3& pos, const glm::vec3& tint) {
        const entt::entity ent = registry.create();
        registry.emplace<TagComponent>(ent, TagComponent{"DynamicCrate"});
        registry.emplace<TransformLocal>(ent, TransformLocal{pos});
        registry.emplace<TransformWorld>(ent);
        MeshComponent mc = m_cubeComp;
        mc.tint = tint;
        registry.emplace<MeshComponent>(ent, mc);
        registry.emplace<MeshGeometryComponent>(ent, MeshGeometryComponent{MeshGeometryType::Box, "", {0.5f, 0.5f, 0.5f}});
        registry.emplace<RenderableTag>(ent);
        registry.emplace<RigidBodyComponent>(ent, RigidBodyComponent{.bodyIndex = UINT32_MAX, .dynamic = true});
        registry.emplace<ColliderComponent>(ent, ColliderComponent{ColliderShapeType::Box, {0.5f, 0.5f, 0.5f}, 0.5f, false, 2.0f});
        createDynamicBox(physics, registry, ent, {0.5f, 0.5f, 0.5f}, 2.0f);
    };

    const float pyrX = -34.0f;
    const float pyrZ = -6.0f;
    spawnDynamicCrate({pyrX, 0.55f, pyrZ - 1.15f}, {0.8f, 0.4f, 0.2f});
    spawnDynamicCrate({pyrX, 0.55f, pyrZ},         {0.75f, 0.5f, 0.25f});
    spawnDynamicCrate({pyrX, 0.55f, pyrZ + 1.15f}, {0.85f, 0.45f, 0.2f});
    spawnDynamicCrate({pyrX, 1.6f,  pyrZ - 0.58f}, {0.7f, 0.35f, 0.18f});
    spawnDynamicCrate({pyrX, 1.6f,  pyrZ + 0.58f}, {0.7f, 0.35f, 0.18f});
    spawnDynamicCrate({pyrX, 2.65f, pyrZ},         {0.9f, 0.6f, 0.3f});

    // 6. Target Range on Right Wing (X = +34)
    const float rangeX = 34.0f;
    const float rangeZ = -6.0f;
    const uint32_t tableMesh = meshes.upload(MeshBuilder::box({1.0f, 0.4f, 3.5f}, {0.3f, 0.32f, 0.36f}));
    createStaticBoxEntity("TargetStand", {rangeX, 0.4f, rangeZ}, {1.0f, 0.4f, 3.5f}, tableMesh, {0.3f, 0.32f, 0.36f}, 0.4f);

    auto spawnTargetSphere = [&](const glm::vec3& pos, const glm::vec3& tint) {
        const entt::entity ent = registry.create();
        registry.emplace<TagComponent>(ent, TagComponent{"TargetSphere"});
        registry.emplace<TransformLocal>(ent, TransformLocal{pos});
        registry.emplace<TransformWorld>(ent);
        MeshComponent mc = m_sphereComp;
        mc.tint = tint;
        registry.emplace<MeshComponent>(ent, mc);
        registry.emplace<MeshGeometryComponent>(ent, MeshGeometryComponent{MeshGeometryType::Sphere, "", {0.4f, 0.f, 0.f}});
        registry.emplace<RenderableTag>(ent);
        registry.emplace<RigidBodyComponent>(ent, RigidBodyComponent{.bodyIndex = UINT32_MAX, .dynamic = true});
        registry.emplace<ColliderComponent>(ent, ColliderComponent{ColliderShapeType::Sphere, {0.4f, 0.4f, 0.4f}, 0.5f, false, 1.5f});
        createDynamicSphere(physics, registry, ent, 0.4f, 1.5f);
    };

    spawnTargetSphere({rangeX, 1.25f, rangeZ - 2.0f}, {0.95f, 0.2f, 0.2f});
    spawnTargetSphere({rangeX, 1.25f, rangeZ - 0.7f}, {0.2f, 0.9f, 0.3f});
    spawnTargetSphere({rangeX, 1.25f, rangeZ + 0.7f}, {0.2f, 0.4f, 1.0f});
    spawnTargetSphere({rangeX, 1.25f, rangeZ + 2.0f}, {1.0f, 0.85f, 0.1f});

    world.resource<PbrRenderer>().setLightDir(m_sunDirection);

    // 8. Player & Camera Initialization (facing North towards the arena)
    m_spawnPoint = {0.0f, 0.8f, 22.0f};

    const entt::entity camera = registry.create();
    registry.emplace<TagComponent>(camera, TagComponent{"MainCamera"});
    registry.emplace<TransformLocal>(camera, TransformLocal{m_spawnPoint + glm::vec3(0.f, character.eyeHeight, 0.f)});
    registry.emplace<TransformWorld>(camera);
    registry.emplace<CameraComponent>(camera);
    FreeFlyController controller{};
    controller.yaw = -1.5707963f; // -90 deg: looking North towards -Z
    controller.pitch = 0.0f;
    controller.lookSensitivity = m_mouseSensitivity;
    registry.emplace<FreeFlyController>(camera, controller);

    character.init(physics, m_spawnPoint);
    m_cameraMode = CameraMode::FirstPerson;
}

void SandboxModule::toggleCameraMode(World& world) {
    auto& registry = world.registry();
    auto& character = world.resource<CharacterController>();

    auto view = registry.view<TransformLocal, CameraComponent, FreeFlyController>();
    if (view.begin() == view.end()) return;
    auto camEntity = *view.begin();
    auto& camTransform = view.get<TransformLocal>(camEntity);

    if (m_cameraMode == CameraMode::FreeFly) {
        m_cameraMode = CameraMode::FirstPerson;
        glm::vec3 charPos = camTransform.translation - glm::vec3(0.0f, character.eyeHeight, 0.0f);
        character.setPosition(charPos);
        log(LogLevel::Info, "Camera mode: FirstPerson (FPS Controller)");
    } else {
        m_cameraMode = CameraMode::FreeFly;
        log(LogLevel::Info, "Camera mode: FreeFly");
    }
}

void SandboxModule::shootSphere(World& world) {
    auto& registry = world.registry();
    auto& physics = world.resource<JoltWorld>();
    auto& particles = world.resource<ParticleSystem>();

    auto view = registry.view<TransformLocal, CameraComponent>();
    if (view.begin() == view.end()) return;
    auto camEntity = *view.begin();
    const auto& camTransform = view.get<TransformLocal>(camEntity);

    const auto* controller = registry.try_get<FreeFlyController>(camEntity);
    const float yaw = controller ? controller->yaw : 0.f;
    const float pitch = controller ? controller->pitch : 0.f;

    const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                            std::sin(yaw) * std::cos(pitch)};

    const glm::vec3 spawnPos = camTransform.translation + forward * 1.5f;
    const glm::vec3 launchVelocity = forward * 28.0f;

    const entt::entity entity = registry.create();
    registry.emplace<TagComponent>(entity, TagComponent{"Cannonball"});
    registry.emplace<TransformLocal>(entity, TransformLocal{spawnPos, glm::quat{1.f, 0.f, 0.f, 0.f}, glm::vec3(1.f)});
    registry.emplace<TransformWorld>(entity);

    MeshComponent mc = m_sphereComp;
    mc.tint = {1.0f, 0.35f, 0.05f};
    mc.metallic = 0.8f;
    mc.roughness = 0.2f;
    mc.emissiveIntensity = 3.0f;
    registry.emplace<MeshComponent>(entity, mc);
    registry.emplace<MeshGeometryComponent>(entity, MeshGeometryComponent{MeshGeometryType::Sphere, "", {0.4f, 0.f, 0.f}});
    registry.emplace<RenderableTag>(entity);
    registry.emplace<RigidBodyComponent>(entity, RigidBodyComponent{.bodyIndex = UINT32_MAX, .dynamic = true});
    registry.emplace<ColliderComponent>(entity, ColliderComponent{ColliderShapeType::Sphere, {0.4f, 0.4f, 0.4f}, 0.4f, false, 5.0f});

    createDynamicSphere(physics, registry, entity, 0.4f, 5.0f, launchVelocity);

    AudioEngine::instance().play2D("assets/sounds/shoot.wav", 0.9f);
    particles.spawnMuzzleFlash(spawnPos, forward);
}

void SandboxModule::kickObjectUnderCrosshair(World& world) {
    auto& registry = world.registry();
    auto& physics = world.resource<JoltWorld>();
    auto& particles = world.resource<ParticleSystem>();

    auto view = registry.view<TransformLocal, CameraComponent>();
    if (view.begin() == view.end()) return;
    auto camEntity = *view.begin();
    const auto& camTransform = view.get<TransformLocal>(camEntity);

    const auto* controller = registry.try_get<FreeFlyController>(camEntity);
    const float yaw = controller ? controller->yaw : 0.f;
    const float pitch = controller ? controller->pitch : 0.f;

    const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                            std::sin(yaw) * std::cos(pitch)};

    RaycastHit hit;
    if (raycast(physics, registry, camTransform.translation, forward, 40.0f, hit)) {
        if (hit.entity != entt::null && registry.valid(hit.entity)) {
            const glm::vec3 impulse = (forward + glm::vec3(0.f, 0.35f, 0.f)) * 45.0f;
            applyImpulse(physics, registry, hit.entity, impulse, hit.position);
            AudioEngine::instance().play3D("assets/sounds/test.wav", hit.position, 1.0f, 1.4f, 25.0f);
            particles.spawnImpactSparks(hit.position, hit.normal, 2.5f);
        }
    }
}

void SandboxModule::inspectObjectUnderCrosshair(World& world) {
    auto& registry = world.registry();
    auto& physics = world.resource<JoltWorld>();

    auto view = registry.view<TransformLocal, CameraComponent>();
    if (view.begin() == view.end()) return;
    auto camEntity = *view.begin();
    const auto& camTransform = view.get<TransformLocal>(camEntity);

    const auto* controller = registry.try_get<FreeFlyController>(camEntity);
    const float yaw = controller ? controller->yaw : 0.f;
    const float pitch = controller ? controller->pitch : 0.f;

    const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                            std::sin(yaw) * std::cos(pitch)};

    RaycastHit hit;
    if (raycast(physics, registry, camTransform.translation, forward, 100.0f, hit)) {
        m_selectedEntity = hit.entity;
    } else {
        m_selectedEntity = entt::null;
    }
}

void SandboxModule::spawnDynamicObject(World& world, const MeshComponent& meshComp, const glm::vec3& halfExtents) {
    auto& registry = world.registry();
    auto& physics = world.resource<JoltWorld>();

    auto view = registry.view<TransformLocal, CameraComponent>();
    if (view.begin() == view.end()) return;
    auto camEntity = *view.begin();
    const auto& camTransform = view.get<TransformLocal>(camEntity);

    const auto* controller = registry.try_get<FreeFlyController>(camEntity);
    const float yaw = controller ? controller->yaw : 0.f;
    const float pitch = controller ? controller->pitch : 0.f;

    const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                            std::sin(yaw) * std::cos(pitch)};
    const glm::vec3 spawnPos = camTransform.translation + forward * 3.0f;

    const entt::entity entity = registry.create();
    registry.emplace<TagComponent>(entity, TagComponent{"SpawnedBox"});
    registry.emplace<TransformLocal>(entity, TransformLocal{spawnPos, glm::quat{1.f, 0.f, 0.f, 0.f}, glm::vec3(1.f)});
    registry.emplace<TransformWorld>(entity);
    registry.emplace<MeshComponent>(entity, meshComp);
    registry.emplace<MeshGeometryComponent>(entity, MeshGeometryComponent{MeshGeometryType::Box, "", halfExtents});
    registry.emplace<RenderableTag>(entity);
    registry.emplace<RigidBodyComponent>(entity, RigidBodyComponent{.bodyIndex = UINT32_MAX, .dynamic = true});
    registry.emplace<ColliderComponent>(entity, ColliderComponent{ColliderShapeType::Box, halfExtents, 0.5f, false, 2.0f});

    createDynamicBox(physics, registry, entity, halfExtents, 2.0f);
    AudioEngine::instance().play2D("assets/sounds/test.wav", 0.5f);
}

void SandboxModule::clearSpawnedObjects(World& world) {
    m_selectedEntity = entt::null;
    clearDynamicBodies(world.registry(), world.resource<JoltWorld>());
    log(LogLevel::Info, "Cleared all dynamic objects.");
}

void SandboxModule::tick(World& world) {
    const float deltaTime = 1.0f / 30.0f;
    auto& input = world.resource<Input>();
    auto& platform = world.resource<PlatformGLFW>();
    auto& registry = world.registry();
    auto& character = world.resource<CharacterController>();
    auto& physics = world.resource<JoltWorld>();

    m_inputMap.beginFrame(input);

    const bool keyboardCaptured = ImGui::GetIO().WantCaptureKeyboard;
    const bool mouseCaptured = ImGui::GetIO().WantCaptureMouse;

    if (!keyboardCaptured) {
        if (m_inputMap.actionPressed(input, SandboxActions::ToggleCameraMode)) {
            toggleCameraMode(world);
        }
        if (m_inputMap.actionPressed(input, SandboxActions::ToggleCursor)) {
            setCursorCapture(world, !m_cursorCaptured);
        }
    }

    if (m_cursorCaptured && !platform.isCursorCaptured()) {
        platform.setCursorCaptured(true);
    } else if (!m_cursorCaptured && platform.isCursorCaptured()) {
        platform.setCursorCaptured(false);
    }

    if (!m_cursorCaptured && !mouseCaptured && input.mouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        setCursorCapture(world, true);
    }

    // Camera & Player update
    if (m_cameraMode == CameraMode::FirstPerson) {
        auto view = registry.view<TransformLocal, CameraComponent, FreeFlyController>();
        for (const auto camEntity : view) {
            auto& camTransform = view.get<TransformLocal>(camEntity);
            auto& controller = view.get<FreeFlyController>(camEntity);

            if (m_cursorCaptured) {
                const glm::vec2 delta = m_inputMap.lookDelta();
                if (delta.x != 0.f || delta.y != 0.f) {
                    controller.yaw += delta.x * m_mouseSensitivity;
                    controller.pitch -= delta.y * m_mouseSensitivity;
                    controller.pitch = std::clamp(controller.pitch, -1.52f, 1.52f);
                }
            } else if (!mouseCaptured && m_inputMap.actionDown(input, Actions::Look)) {
                const glm::vec2 delta = m_inputMap.lookDelta();
                controller.yaw += delta.x * m_mouseSensitivity;
                controller.pitch -= delta.y * m_mouseSensitivity;
                controller.pitch = std::clamp(controller.pitch, -1.52f, 1.52f);
            }

            const bool isSprinting = !keyboardCaptured && m_inputMap.actionDown(input, Actions::Sprint);
            character.walkSpeed = isSprinting ? 9.5f : 5.5f;

            glm::vec2 moveInput{0.0f};
            bool jump = false;
            if (!keyboardCaptured) {
                if (m_inputMap.actionDown(input, Actions::MoveForward)) moveInput.y += 1.0f;
                if (m_inputMap.actionDown(input, Actions::MoveBack))    moveInput.y -= 1.0f;
                if (m_inputMap.actionDown(input, Actions::MoveRight))   moveInput.x += 1.0f;
                if (m_inputMap.actionDown(input, Actions::MoveLeft))    moveInput.x -= 1.0f;
                if (m_inputMap.actionPressed(input, Actions::Jump)) {
                    jump = true;
                    if (character.isGrounded()) {
                        AudioEngine::instance().play2D("assets/sounds/jump.wav", 0.7f);
                    }
                }
            }

            character.update(physics, deltaTime, moveInput, controller.yaw, jump);
            camTransform.translation = character.position() + glm::vec3(0.0f, character.eyeHeight, 0.0f);
        }
    } else {
        updateFreeFlyCamera(registry, input, m_inputMap, deltaTime, m_cursorCaptured);
    }

    // Update 3D audio listener from active camera
    auto camView = registry.view<TransformLocal, CameraComponent>();
    if (camView.begin() != camView.end()) {
        const auto camEnt = *camView.begin();
        const auto& camTransform = camView.get<TransformLocal>(camEnt);
        const auto* controller = registry.try_get<FreeFlyController>(camEnt);
        const float yaw = controller ? controller->yaw : 0.f;
        const float pitch = controller ? controller->pitch : 0.f;
        const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch)};
        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));
        AudioEngine::instance().updateListener(camTransform.translation, forward, up);
    }

    // Actions
    if (!keyboardCaptured) {
        if (m_inputMap.actionPressed(input, SandboxActions::SpawnBox)) {
            spawnDynamicObject(world, m_cubeComp, {0.5f, 0.5f, 0.5f});
        }
        if (m_inputMap.actionPressed(input, SandboxActions::SpawnMesh)) {
            spawnDynamicObject(world, m_sphereComp, {0.4f, 0.4f, 0.4f});
        }
        if (m_inputMap.actionPressed(input, SandboxActions::InspectObject)) {
            inspectObjectUnderCrosshair(world);
        }
    }
    if (!mouseCaptured) {
        if (m_inputMap.actionPressed(input, SandboxActions::ShootSphere)) {
            shootSphere(world);
        }
        if (m_inputMap.actionPressed(input, SandboxActions::KickObject)) {
            kickObjectUnderCrosshair(world);
        }
    }

    // Animate point lights
    if (m_animateLights && !m_demoPointLights.empty()) {
        m_lightAnimTime += deltaTime;
        const float orbitRadius = 2.5f;
        const float baseHeight = 6.7f;
        const float speeds[4] = {1.2f, -1.0f, 1.5f, -1.3f};

        struct BasePos { float x, z; };
        const BasePos basePositions[4] = {
            {-20.f, -17.f}, {20.f, -17.f}, {-20.f, 5.f}, {20.f, 5.f}
        };

        for (size_t i = 0; i < m_demoPointLights.size() && i < 4; ++i) {
            const entt::entity lightEnt = m_demoPointLights[i];
            if (registry.valid(lightEnt) && registry.all_of<TransformLocal>(lightEnt)) {
                auto& t = registry.get<TransformLocal>(lightEnt);
                const float angle = m_lightAnimTime * speeds[i] + static_cast<float>(i) * 1.57f;
                t.translation.x = basePositions[i].x + std::cos(angle) * orbitRadius;
                t.translation.z = basePositions[i].z + std::sin(angle) * orbitRadius;
                t.translation.y = baseHeight + std::sin(m_lightAnimTime * 2.0f + static_cast<float>(i)) * 0.4f;
            }
        }
    }

    updateTransforms(registry);

    // Fail-safe boundary check / Killzone
    const glm::vec3 charPos = character.position();
    if (charPos.y < -5.0f || std::abs(charPos.x) > 65.0f || std::abs(charPos.z) > 65.0f) {
        respawnPlayer(world);
    }
}

void SandboxModule::render(World& world, float /*alpha*/) {
    auto& platform = world.resource<PlatformGLFW>();
    if (platform.shouldClose()) return;

    if (m_smokeTest) {
        if (++m_frameCount >= 100) {
            log(LogLevel::Info, "Smoke test completed 100 frames successfully.");
            platform.setWindowShouldClose(true);
            world.events().enqueue(WindowCloseEvent{});
            return;
        }
    }

    world.resource<PbrRenderer>().tryReloadShaders();
    world.resource<ParticleSystem>().update(1.0f / 60.0f);

    renderFrame(world);
}

bool SandboxModule::renderFrame(World& world) {
    auto& platform = world.resource<PlatformGLFW>();
    auto& vulkan = world.resource<VulkanContext>();
    auto& renderer = world.resource<PbrRenderer>();
    auto& postProcess = world.resource<PostProcessPipeline>();
    auto& meshes = world.resource<GpuMeshCache>();
    auto& textures = world.resource<GpuTextureCache>();
    auto& particles = world.resource<ParticleSystem>();
    auto& imgui = world.resource<ImGuiLayer>();
    auto& registry = world.registry();

    const auto currentSize = platform.framebufferSize();
    if (currentSize.x == 0 || currentSize.y == 0) {
        return true;
    }

    uint32_t imageIndex = 0;
    const VkResult acquired = vulkan.acquireNextImage(&imageIndex);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        vulkan.handleResize(WindowResizeEvent{currentSize.x, currentSize.y});
        postProcess.handleResize(currentSize.x, currentSize.y);
        ImGui_ImplVulkan_SetMinImageCount(
            static_cast<uint32_t>(std::max(2, static_cast<int>(vulkan.framebuffers().size()))));
        return true;
    }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    VkCommandBuffer cmd = vulkan.commandBuffer(vulkan.currentFrame());
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    if (m_enableShadows) {
        renderer.recordShadowPass(cmd, registry, meshes);
    }

    // 1. 3D Scene HDR Offscreen Render Pass
    std::array<VkClearValue, 2> hdrClearValues{};
    hdrClearValues[0].color = {{0.06f, 0.07f, 0.09f, 1.f}};
    hdrClearValues[1].depthStencil = {1.f, 0};

    VkRenderPassBeginInfo hdrPassInfo{};
    hdrPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    hdrPassInfo.renderPass = vulkan.hdrRenderPass();
    hdrPassInfo.framebuffer = vulkan.hdrFramebuffer();
    hdrPassInfo.renderArea.extent = vulkan.swapchainExtent();
    hdrPassInfo.clearValueCount = static_cast<uint32_t>(hdrClearValues.size());
    hdrPassInfo.pClearValues = hdrClearValues.data();

    vkCmdBeginRenderPass(cmd, &hdrPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    const float aspect =
        static_cast<float>(vulkan.swapchainExtent().width) /
        static_cast<float>(std::max(1u, vulkan.swapchainExtent().height));
    const CameraState camera = findActiveCamera(registry, aspect);

    renderer.recordScene(cmd, registry, meshes, textures, camera);
    particles.record(cmd, camera);
    if (m_showDebug) {
        m_debugDraw.record(cmd, registry, camera, m_selectedEntity);
    }

    vkCmdEndRenderPass(cmd);

    // 2. Multi-pass Bloom Pyramid
    postProcess.recordBloom(cmd);

    // 3. Swapchain Presentation Pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = vulkan.renderPass();
    renderPassInfo.framebuffer = vulkan.framebuffers()[imageIndex];
    renderPassInfo.renderArea.extent = vulkan.swapchainExtent();
    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues = nullptr;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 4. Tone Mapping & Bloom Composite
    postProcess.recordComposite(cmd);

    imgui.beginFrame();

    // Tactical Crosshair in FPS mode with locked cursor
    if (m_cameraMode == CameraMode::FirstPerson && m_cursorCaptured) {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        const ImVec2 center = ImVec2(
            static_cast<float>(vulkan.swapchainExtent().width) * 0.5f,
            static_cast<float>(vulkan.swapchainExtent().height) * 0.5f
        );
        const float gap = 4.0f;
        const float length = 8.0f;
        const ImU32 crossColor = IM_COL32(0, 255, 140, 230);
        const ImU32 shadowColor = IM_COL32(0, 0, 0, 180);

        drawList->AddCircleFilled(center, 1.5f, crossColor);

        drawList->AddLine(ImVec2(center.x, center.y - gap - length), ImVec2(center.x, center.y - gap), shadowColor, 2.5f);
        drawList->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + length), shadowColor, 2.5f);
        drawList->AddLine(ImVec2(center.x - gap - length, center.y), ImVec2(center.x - gap, center.y), shadowColor, 2.5f);
        drawList->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + length, center.y), shadowColor, 2.5f);

        drawList->AddLine(ImVec2(center.x, center.y - gap - length), ImVec2(center.x, center.y - gap), crossColor, 1.5f);
        drawList->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + length), crossColor, 1.5f);
        drawList->AddLine(ImVec2(center.x - gap - length, center.y), ImVec2(center.x - gap, center.y), crossColor, 1.5f);
        drawList->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + length, center.y), crossColor, 1.5f);
    }

    renderUi(world);

    imgui.endFrame(cmd);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    const VkResult presentResult = vulkan.submitAndPresent(imageIndex);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        const auto size = platform.framebufferSize();
        if (size.x > 0 && size.y > 0) {
            vulkan.handleResize(WindowResizeEvent{size.x, size.y});
            postProcess.handleResize(size.x, size.y);
            ImGui_ImplVulkan_SetMinImageCount(
                static_cast<uint32_t>(std::max(2, static_cast<int>(vulkan.framebuffers().size()))));
        }
    }
    return true;
}

void SandboxModule::renderUi(World& world) {
    auto& registry = world.registry();
    auto& character = world.resource<CharacterController>();

    ImGui::Begin("Sandbox");
    ImGui::Text("Entities: %zu", registry.view<TransformLocal>().size());

    size_t dynamicBodiesCount = 0;
    for (const auto entity : registry.view<const RigidBodyComponent>()) {
        if (registry.get<const RigidBodyComponent>(entity).dynamic) {
            ++dynamicBodiesCount;
        }
    }
    ImGui::Text("Dynamic Bodies: %zu", dynamicBodiesCount);

    ImGui::Separator();
    ImGui::Text("Camera & Mouse Controls:");
    int currentMode = (m_cameraMode == CameraMode::FreeFly) ? 0 : 1;
    if (ImGui::RadioButton("Free-Fly (F1)", &currentMode, 0)) {
        if (m_cameraMode != CameraMode::FreeFly) toggleCameraMode(world);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("FPS Mode (F1)", &currentMode, 1)) {
        if (m_cameraMode != CameraMode::FirstPerson) toggleCameraMode(world);
    }

    if (ImGui::Button(m_cursorCaptured ? "Release Mouse (ESC)" : "Capture Mouse (ESC / Click Viewport)")) {
        setCursorCapture(world, !m_cursorCaptured);
    }
    ImGui::SameLine();
    if (ImGui::Button("Respawn at Origin")) {
        respawnPlayer(world);
    }

    ImGui::SliderFloat("Mouse Sensitivity", &m_mouseSensitivity, 0.0005f, 0.01f, "%.4f");

    if (m_cameraMode == CameraMode::FirstPerson) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "WASD: Move | Shift: Sprint | Space: Jump | Mouse: Aim | LMB: Shoot");
        ImGui::Text("Grounded: %s", character.isGrounded() ? "YES" : "NO");
        const glm::vec3 p = character.position();
        ImGui::Text("Pos: (%.2f, %.2f, %.2f)", p.x, p.y, p.z);
    }

    ImGui::Separator();
    ImGui::Text("Physics Actions:");
    if (ImGui::Button("Spawn Cube (B)")) {
        spawnDynamicObject(world, m_cubeComp, {0.5f, 0.5f, 0.5f});
    }
    ImGui::SameLine();
    if (ImGui::Button("Shoot Cannonball (LMB / F)")) {
        shootSphere(world);
    }

    if (ImGui::Button("Kick Crosshair (MMB / E)")) {
        kickObjectUnderCrosshair(world);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Dynamic Bodies")) {
        clearSpawnedObjects(world);
    }

    ImGui::Separator();
    ImGui::Checkbox("Show Debug Wireframes", &m_showDebug);
    ImGui::Checkbox("Animate Point Lights", &m_animateLights);
    ImGui::Checkbox("Enable Directional Shadows", &m_enableShadows);

    if (ImGui::CollapsingHeader("Sun Light Settings")) {
        if (ImGui::SliderFloat3("Direction", &m_sunDirection.x, -1.f, 1.f)) {
            world.resource<PbrRenderer>().setLightDir(m_sunDirection);
        }
    }

    if (m_selectedEntity != entt::null && registry.valid(m_selectedEntity)) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Selected Entity: %u", static_cast<uint32_t>(m_selectedEntity));
        if (auto* tag = registry.try_get<TagComponent>(m_selectedEntity)) {
            ImGui::Text("Tag: %s", tag->tag.c_str());
        }
        if (auto* t = registry.try_get<TransformLocal>(m_selectedEntity)) {
            ImGui::Text("Position: (%.2f, %.2f, %.2f)", t->translation.x, t->translation.y, t->translation.z);
        }
        if (auto* mc = registry.try_get<MeshComponent>(m_selectedEntity)) {
            ImGui::ColorEdit3("Tint", &mc->tint.x);
            ImGui::SliderFloat("Roughness", &mc->roughness, 0.0f, 1.0f);
            ImGui::SliderFloat("Metallic", &mc->metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Emissive", &mc->emissiveIntensity, 0.0f, 20.0f);
        }
    }

    ImGui::End();

    engine::ui::drawEditorToolbar(m_editorHistory, world.registry(), world.types(), m_gizmo, m_sceneControls);
    handleSceneRequests(world, m_editorHistory, m_sceneControls, m_sunDirection);
    if (m_sceneControls.saveRequested || m_sceneControls.loadRequested) {
        world.resource<PbrRenderer>().setLightDir(m_sunDirection);
    }
    engine::ui::drawComponentInspector(world.registry(), world.types(), m_selectedEntity, &m_editorHistory);

    const glm::ivec2 fb = world.resource<PlatformGLFW>().framebufferSize();
    const float aspect = static_cast<float>(std::max(1, fb.x)) / static_cast<float>(std::max(1, fb.y));
    const bool allowPick = !m_cursorCaptured && !ImGui::GetIO().WantCaptureMouse;
    tickEditorGizmo(m_gizmo, m_editorHistory, m_gizmoWasDragging, m_gizmoStartTranslation, m_gizmoStartRotation,
                    registry, m_selectedEntity, findActiveCamera(registry, aspect), world.resource<Input>(),
                    glm::vec2{static_cast<float>(fb.x), static_cast<float>(fb.y)}, allowPick);
}

} // namespace engine::sandbox
