#include "SandboxApp.hpp"

#include "engine/assets/GltfMeshLoader.hpp"
#include "engine/assets/GltfTextureLoader.hpp"
#include "engine/assets/MeshBuilder.hpp"
#include "engine/assets/ObjMeshLoader.hpp"
#include "engine/audio/AudioEngine.hpp"
#include "engine/core/Events.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Path.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/ecs/SceneSerializer.hpp"
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

std::vector<entt::entity> spawnGltfModel(entt::registry& registry, AssetManager& assets, GpuMeshCache& meshes,
                                         GpuTextureCache& textures, const std::filesystem::path& path,
                                         const glm::vec3& position, float targetSize) {
    std::vector<entt::entity> spawnedEntities;
    const AssetManager::GltfPtr gltf = assets.getOrLoadGltf(path);
    if (!gltf) {
        log(LogLevel::Warn, "spawnGltfModel: failed to load " + path.string());
        return spawnedEntities;
    }

    std::vector<GltfMeshPart> parts = GltfMeshLoader::extractMeshParts(*gltf);
    if (parts.empty()) {
        log(LogLevel::Warn, "spawnGltfModel: no mesh parts in " + path.filename().string());
        return spawnedEntities;
    }

    std::vector<GltfMeshPart> validParts;
    validParts.reserve(parts.size());
    for (GltfMeshPart& part : parts) {
        if (!part.mesh.empty()) {
            validParts.push_back(std::move(part));
        }
    }
    if (validParts.empty()) {
        return spawnedEntities;
    }

    std::vector<MeshCpuData> meshData;
    meshData.reserve(validParts.size());
    for (GltfMeshPart& part : validParts) {
        meshData.push_back(std::move(part.mesh));
    }
    ObjMeshLoader::normalize(meshData, targetSize);

    spawnedEntities.reserve(validParts.size());
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

        const entt::entity ent = spawnMeshEntity(registry, meshComp, position, {1.f, 1.f, 1.f});
        spawnedEntities.push_back(ent);
    }

    log(LogLevel::Info, "spawnGltfModel: spawned " + path.filename().string() + " (" +
                           std::to_string(validParts.size()) + " parts)");
    return spawnedEntities;
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
    m_inputMap.bind(Action::Sprint, GLFW_KEY_LEFT_SHIFT);
    m_inputMap.bind(Action::SpawnBox, GLFW_KEY_B);
    m_inputMap.bind(Action::Jump, GLFW_KEY_SPACE);
    m_inputMap.bind(Action::ToggleCameraMode, GLFW_KEY_F1);
    m_inputMap.bind(Action::ToggleCursor, GLFW_KEY_ESCAPE);
    m_inputMap.bind(Action::InspectObject, GLFW_KEY_I);
    m_inputMap.bind(Action::SpawnTeapot, GLFW_KEY_T);
    m_inputMap.bind(Action::ShootSphere, GLFW_KEY_F);
    m_inputMap.bind(Action::KickObject, GLFW_KEY_E);
    m_inputMap.bindMouse(Action::Look, GLFW_MOUSE_BUTTON_RIGHT);
    m_inputMap.bindMouse(Action::ShootSphere, GLFW_MOUSE_BUTTON_LEFT);
    m_inputMap.bindMouse(Action::KickObject, GLFW_MOUSE_BUTTON_MIDDLE);
}

void SandboxApp::handleResize(const WindowResizeEvent& e) {
    m_vulkan.handleResize(e);
    m_postProcess.handleResize(e.width, e.height);
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

    m_dispatcher.sink<WindowResizeEvent>().connect<&SandboxApp::handleResize>(*this);

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
    if (!m_postProcess.init(m_vulkan)) {
        return false;
    }
    if (!m_debugDraw.init(m_vulkan)) {
        return false;
    }

    shaderHotReloadWatchPath(shaderPath("pbr.vert").c_str());
    shaderHotReloadWatchPath(shaderPath("pbr.frag").c_str());

    m_particles.init(m_vulkan);

    AudioEngine::instance().init();
    m_physics.setContactCallback([this](const glm::vec3& pos, float speed) {
        const float volume = std::clamp(speed / 12.0f, 0.15f, 1.0f);
        AudioEngine::instance().play3D("assets/sounds/impact.wav", pos, volume, 1.0f, 35.0f);
        m_particles.spawnImpactSparks(pos, glm::vec3(0.f, 1.f, 0.f), speed / 6.0f);
    });

    m_time.reset();
    spawnScene();
    setCursorCapture(true);
    return true;
}

void SandboxApp::setCursorCapture(bool capture) {
    m_cursorCaptured = capture;
    m_platform.setCursorCaptured(capture);
}

void SandboxApp::respawnPlayer() {
    m_character.setPosition(m_spawnPoint);
    auto view = m_registry.view<TransformLocal, CameraComponent>();
    for (const auto camEnt : view) {
        auto& t = view.get<TransformLocal>(camEnt);
        t.translation = m_spawnPoint + glm::vec3(0.0f, m_character.eyeHeight, 0.0f);
        if (auto* ctrl = m_registry.try_get<FreeFlyController>(camEnt)) {
            ctrl->yaw = -1.5707963f;
            ctrl->pitch = 0.0f;
        }
    }
    AudioEngine::instance().play2D("assets/sounds/test.wav", 0.6f);
    log(LogLevel::Info, "Player reached boundary / killzone and was safely respawned at arena origin.");
}

void SandboxApp::spawnScene() {
    m_demoPointLights.clear();

    const float arenaHalf = 50.0f;
    const float wallHeight = 6.0f;
    const float wallHalfH = wallHeight * 0.5f;

    // Helper lambda to create static physical boxes with visuals
    auto createStaticBoxEntity = [this](const std::string& name, const glm::vec3& pos, const glm::vec3& halfExtents,
                                         uint32_t meshId, const glm::vec3& tint, float roughness = 0.5f,
                                         float metallic = 0.0f, float emissive = 0.0f) -> entt::entity {
        const entt::entity ent = m_registry.create();
        m_registry.emplace<TagComponent>(ent, TagComponent{name});
        m_registry.emplace<TransformLocal>(ent, TransformLocal{pos});
        m_registry.emplace<TransformWorld>(ent);
        MeshComponent mc{};
        mc.mesh = meshId;
        mc.tint = tint;
        mc.roughness = roughness;
        mc.metallic = metallic;
        mc.emissiveIntensity = emissive;
        m_registry.emplace<MeshComponent>(ent, mc);
        m_registry.emplace<MeshGeometryComponent>(ent, MeshGeometryComponent{MeshGeometryType::Box, "", halfExtents});
        m_registry.emplace<RenderableTag>(ent);
        m_registry.emplace<StaticColliderTag>(ent);
        m_registry.emplace<RigidBodyComponent>(ent, RigidBodyComponent{.bodyIndex = UINT32_MAX, .dynamic = false});
        m_registry.emplace<ColliderComponent>(ent, ColliderComponent{ColliderShapeType::Box, halfExtents, 0.0f, true, 0.0f});
        createStaticBox(m_physics, m_registry, ent, halfExtents);
        return ent;
    };

    // 1. Arena Floor (100x100m)
    const uint32_t floorMesh = m_meshes->upload(MeshBuilder::plane(arenaHalf, {0.16f, 0.18f, 0.22f}));
    const entt::entity floorVisual = spawnMeshEntity(m_registry, floorMesh, {0.f, 0.f, 0.f}, {1.f, 1.f, 1.f}, {1.f, 1.f, 1.f});
    m_registry.emplace<TagComponent>(floorVisual, TagComponent{"ArenaFloorVisual"});
    m_registry.emplace<MeshGeometryComponent>(floorVisual, MeshGeometryComponent{MeshGeometryType::Plane, "", {arenaHalf, 0.f, 0.f}});

    const entt::entity floorCollider = m_registry.create();
    m_registry.emplace<TagComponent>(floorCollider, TagComponent{"ArenaFloorCollider"});
    m_registry.emplace<TransformLocal>(floorCollider, TransformLocal{{0.f, -0.5f, 0.f}});
    m_registry.emplace<StaticColliderTag>(floorCollider);
    m_registry.emplace<RigidBodyComponent>(floorCollider, RigidBodyComponent{.bodyIndex = UINT32_MAX, .dynamic = false});
    m_registry.emplace<ColliderComponent>(floorCollider, ColliderComponent{ColliderShapeType::Box, {arenaHalf, 0.5f, arenaHalf}, 0.f, true, 0.f});
    createStaticBox(m_physics, m_registry, floorCollider, {arenaHalf, 0.5f, arenaHalf});

    // 2. Perimeter Boundary Walls (North, South, East, West - 6m high)
    const uint32_t wallMeshX = m_meshes->upload(MeshBuilder::box({arenaHalf, wallHalfH, 0.5f}, {0.14f, 0.16f, 0.20f}));
    const uint32_t wallMeshZ = m_meshes->upload(MeshBuilder::box({0.5f, wallHalfH, arenaHalf}, {0.14f, 0.16f, 0.20f}));
    const uint32_t trimMeshX = m_meshes->upload(MeshBuilder::box({arenaHalf, 0.08f, 0.55f}, {0.1f, 0.75f, 1.0f}));
    const uint32_t trimMeshZ = m_meshes->upload(MeshBuilder::box({0.55f, 0.08f, arenaHalf}, {0.1f, 0.75f, 1.0f}));

    // North & South walls
    createStaticBoxEntity("WallNorth", {0.f, wallHalfH, -arenaHalf}, {arenaHalf, wallHalfH, 0.5f}, wallMeshX, {0.14f, 0.16f, 0.20f}, 0.7f);
    createStaticBoxEntity("WallSouth", {0.f, wallHalfH, arenaHalf}, {arenaHalf, wallHalfH, 0.5f}, wallMeshX, {0.14f, 0.16f, 0.20f}, 0.7f);
    // East & West walls
    createStaticBoxEntity("WallEast", {arenaHalf, wallHalfH, 0.f}, {0.5f, wallHalfH, arenaHalf}, wallMeshZ, {0.14f, 0.16f, 0.20f}, 0.7f);
    createStaticBoxEntity("WallWest", {-arenaHalf, wallHalfH, 0.f}, {0.5f, wallHalfH, arenaHalf}, wallMeshZ, {0.14f, 0.16f, 0.20f}, 0.7f);

    // Glowing perimeter neon trims along wall tops
    createStaticBoxEntity("TrimNorth", {0.f, wallHeight + 0.08f, -arenaHalf}, {arenaHalf, 0.08f, 0.55f}, trimMeshX, {0.1f, 0.75f, 1.0f}, 0.2f, 0.1f, 6.0f);
    createStaticBoxEntity("TrimSouth", {0.f, wallHeight + 0.08f, arenaHalf}, {arenaHalf, 0.08f, 0.55f}, trimMeshX, {0.1f, 0.75f, 1.0f}, 0.2f, 0.1f, 6.0f);
    createStaticBoxEntity("TrimEast", {arenaHalf, wallHeight + 0.08f, 0.f}, {0.55f, 0.08f, arenaHalf}, trimMeshZ, {0.1f, 0.75f, 1.0f}, 0.2f, 0.1f, 6.0f);
    createStaticBoxEntity("TrimWest", {-arenaHalf, wallHeight + 0.08f, 0.f}, {0.55f, 0.08f, arenaHalf}, trimMeshZ, {0.1f, 0.75f, 1.0f}, 0.2f, 0.1f, 6.0f);

    // 3. Central Showroom Pavilion Platform (48x0.8x28m)
    const glm::vec3 platformCenter{0.f, 0.4f, -6.f};
    const glm::vec3 platformHalf{24.0f, 0.4f, 14.0f};
    const uint32_t platformMesh = m_meshes->upload(MeshBuilder::box(platformHalf, {0.22f, 0.24f, 0.28f}));
    createStaticBoxEntity("CentralPlatform", platformCenter, platformHalf, platformMesh, {0.22f, 0.24f, 0.28f}, 0.3f, 0.3f);

    // Platform Stairs on South Side (facing player spawn)
    const uint32_t stepMesh1 = m_meshes->upload(MeshBuilder::box({8.0f, 0.13f, 0.7f}, {0.26f, 0.28f, 0.32f}));
    const uint32_t stepMesh2 = m_meshes->upload(MeshBuilder::box({8.0f, 0.26f, 0.6f}, {0.26f, 0.28f, 0.32f}));
    const uint32_t stepMesh3 = m_meshes->upload(MeshBuilder::box({8.0f, 0.39f, 0.5f}, {0.26f, 0.28f, 0.32f}));
    createStaticBoxEntity("PlatformStep1", {0.f, 0.13f, 9.4f}, {8.0f, 0.13f, 0.7f}, stepMesh1, {0.26f, 0.28f, 0.32f}, 0.4f);
    createStaticBoxEntity("PlatformStep2", {0.f, 0.26f, 8.4f}, {8.0f, 0.26f, 0.6f}, stepMesh2, {0.26f, 0.28f, 0.32f}, 0.4f);
    createStaticBoxEntity("PlatformStep3", {0.f, 0.39f, 7.6f}, {8.0f, 0.39f, 0.5f}, stepMesh3, {0.26f, 0.28f, 0.32f}, 0.4f);

    // 4. Architectural Columns with Emissive Neon Rings & Point Lights
    const uint32_t colMesh = m_meshes->upload(MeshBuilder::box({0.7f, 3.5f, 0.7f}, {0.18f, 0.20f, 0.24f}));
    const uint32_t beaconRingMesh = m_meshes->upload(MeshBuilder::box({0.85f, 0.20f, 0.85f}, {1.0f, 1.0f, 1.0f}));
    const uint32_t lightMarkerMesh = m_meshes->upload(MeshBuilder::sphere(0.20f, 16, 16, {1.f, 1.f, 1.f}));

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
        // Base column
        createStaticBoxEntity(std::string(p.name) + "_Base", p.pos, {0.7f, 3.5f, 0.7f}, colMesh, {0.18f, 0.20f, 0.24f}, 0.5f);
        // Emissive neon ring near top
        createStaticBoxEntity(std::string(p.name) + "_Neon", {p.pos.x, 6.2f, p.pos.z}, {0.85f, 0.20f, 0.85f}, beaconRingMesh, p.color, 0.1f, 0.1f, 12.0f);

        // Point Light above column
        const entt::entity lightEnt = m_registry.create();
        m_registry.emplace<TagComponent>(lightEnt, TagComponent{std::string(p.name) + "_Light"});
        m_registry.emplace<TransformLocal>(lightEnt, TransformLocal{{p.pos.x, 6.7f, p.pos.z}});
        m_registry.emplace<TransformWorld>(lightEnt);
        m_registry.emplace<PointLightComponent>(lightEnt, PointLightComponent{
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
        m_registry.emplace<MeshComponent>(lightEnt, markerComp);
        m_registry.emplace<MeshGeometryComponent>(lightEnt, MeshGeometryComponent{MeshGeometryType::Sphere, "", {0.20f, 0.f, 0.f}});
        m_registry.emplace<RenderableTag>(lightEnt);
        m_demoPointLights.push_back(lightEnt);
    }

    // 5. Physics Playground (Dynamic Crates Pyramid on Left Wing, X = -34)
    m_cubeComp.mesh = m_meshes->upload(MeshBuilder::box({0.5f, 0.5f, 0.5f}, {0.75f, 0.45f, 0.25f}));
    m_cubeComp.metallic = 0.1f;
    m_cubeComp.roughness = 0.7f;

    m_sphereComp.mesh = m_meshes->upload(MeshBuilder::sphere(0.4f, 24, 24, {0.2f, 0.75f, 1.0f}));
    m_sphereComp.tint = {0.25f, 0.75f, 1.0f};
    m_sphereComp.metallic = 0.9f;
    m_sphereComp.roughness = 0.15f;

    auto spawnDynamicCrate = [this](const glm::vec3& pos, const glm::vec3& tint) {
        const entt::entity ent = m_registry.create();
        m_registry.emplace<TagComponent>(ent, TagComponent{"DynamicCrate"});
        m_registry.emplace<TransformLocal>(ent, TransformLocal{pos});
        m_registry.emplace<TransformWorld>(ent);
        MeshComponent mc = m_cubeComp;
        mc.tint = tint;
        m_registry.emplace<MeshComponent>(ent, mc);
        m_registry.emplace<MeshGeometryComponent>(ent, MeshGeometryComponent{MeshGeometryType::Box, "", {0.5f, 0.5f, 0.5f}});
        m_registry.emplace<RenderableTag>(ent);
        m_registry.emplace<RigidBodyComponent>(ent, RigidBodyComponent{.bodyIndex = UINT32_MAX, .dynamic = true});
        m_registry.emplace<ColliderComponent>(ent, ColliderComponent{ColliderShapeType::Box, {0.5f, 0.5f, 0.5f}, 0.5f, false, 2.0f});
        createDynamicBox(m_physics, m_registry, ent, {0.5f, 0.5f, 0.5f}, 2.0f);
    };

    // 3-2-1 pyramid of crates at X = -34, Z = -6
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
    const uint32_t tableMesh = m_meshes->upload(MeshBuilder::box({1.0f, 0.4f, 3.5f}, {0.3f, 0.32f, 0.36f}));
    createStaticBoxEntity("TargetStand", {rangeX, 0.4f, rangeZ}, {1.0f, 0.4f, 3.5f}, tableMesh, {0.3f, 0.32f, 0.36f}, 0.4f);

    auto spawnTargetSphere = [this](const glm::vec3& pos, const glm::vec3& tint) {
        const entt::entity ent = m_registry.create();
        m_registry.emplace<TagComponent>(ent, TagComponent{"TargetSphere"});
        m_registry.emplace<TransformLocal>(ent, TransformLocal{pos});
        m_registry.emplace<TransformWorld>(ent);
        MeshComponent mc = m_sphereComp;
        mc.tint = tint;
        m_registry.emplace<MeshComponent>(ent, mc);
        m_registry.emplace<MeshGeometryComponent>(ent, MeshGeometryComponent{MeshGeometryType::Sphere, "", {0.4f, 0.f, 0.f}});
        m_registry.emplace<RenderableTag>(ent);
        m_registry.emplace<RigidBodyComponent>(ent, RigidBodyComponent{.bodyIndex = UINT32_MAX, .dynamic = true});
        m_registry.emplace<ColliderComponent>(ent, ColliderComponent{ColliderShapeType::Sphere, {0.4f, 0.4f, 0.4f}, 0.5f, false, 1.5f});
        createDynamicSphere(m_physics, m_registry, ent, 0.4f, 1.5f);
    };

    spawnTargetSphere({rangeX, 1.25f, rangeZ - 2.0f}, {0.95f, 0.2f, 0.2f});
    spawnTargetSphere({rangeX, 1.25f, rangeZ - 0.7f}, {0.2f, 0.9f, 0.3f});
    spawnTargetSphere({rangeX, 1.25f, rangeZ + 0.7f}, {0.2f, 0.4f, 1.0f});
    spawnTargetSphere({rangeX, 1.25f, rangeZ + 2.0f}, {1.0f, 0.85f, 0.1f});

    // 7. Showroom Turntable Stands & BMW Cars
    const float standY = 0.8f + 0.2f; // on top of platform (Y=0.8) + halfHeight (0.2) = 1.0f
    const float turntableRadius = 3.6f;
    const float turntableHalfH = 0.2f;
    const uint32_t turntableMesh = m_meshes->upload(MeshBuilder::cylinder(turntableRadius, turntableHalfH, 48, {0.18f, 0.20f, 0.24f}));
    const uint32_t ringMeshCyan = m_meshes->upload(MeshBuilder::cylinder(turntableRadius + 0.08f, 0.04f, 48, {0.1f, 0.75f, 1.0f}));
    const uint32_t ringMeshAmber = m_meshes->upload(MeshBuilder::cylinder(turntableRadius + 0.08f, 0.04f, 48, {1.0f, 0.75f, 0.15f}));

    // Helper for non-colliding emissive visual meshes
    auto spawnGlowingRing = [this](const std::string& name, const glm::vec3& pos, uint32_t meshId, const glm::vec3& tint, float emissive) {
        MeshComponent mc{};
        mc.mesh = meshId;
        mc.tint = tint;
        mc.roughness = 0.1f;
        mc.metallic = 0.1f;
        mc.emissiveIntensity = emissive;
        const entt::entity ent = spawnMeshEntity(m_registry, mc, pos, {1.f, 1.f, 1.f});
        m_registry.emplace<TagComponent>(ent, TagComponent{name});
        return ent;
    };

    // Stand 1: BMW M3 GTR (Left turntable, X = -12.0)
    const glm::vec3 stand1Pos{-12.0f, standY, -6.0f};
    createStaticBoxEntity("Turntable1_Base", stand1Pos, {turntableRadius, turntableHalfH, turntableRadius}, turntableMesh, {0.18f, 0.20f, 0.24f}, 0.25f, 0.2f);
    spawnGlowingRing("Turntable1_Rim", {stand1Pos.x, stand1Pos.y + turntableHalfH + 0.02f, stand1Pos.z}, ringMeshCyan, {0.1f, 0.75f, 1.0f}, 8.0f);

    // Stand 2: BMW M3 NFS (Right turntable, X = +12.0)
    const glm::vec3 stand2Pos{12.0f, standY, -6.0f};
    createStaticBoxEntity("Turntable2_Base", stand2Pos, {turntableRadius, turntableHalfH, turntableRadius}, turntableMesh, {0.18f, 0.20f, 0.24f}, 0.25f, 0.2f);
    spawnGlowingRing("Turntable2_Rim", {stand2Pos.x, stand2Pos.y + turntableHalfH + 0.02f, stand2Pos.z}, ringMeshAmber, {1.0f, 0.75f, 0.15f}, 8.0f);

    // Car surface level: standY + turntableHalfH = 1.0f + 0.2f = 1.2f
    const float carSpawnY = standY + turntableHalfH;
    m_car1Entities = spawnGltfModel(m_registry, m_assets, *m_meshes, *m_textures,
                                    "assets/bmw_m3_gtr.glb", {stand1Pos.x, carSpawnY, stand1Pos.z}, 4.8f);
    m_car2Entities = spawnGltfModel(m_registry, m_assets, *m_meshes, *m_textures,
                                    "assets/bmw_m3_nfs.glb", {stand2Pos.x, carSpawnY, stand2Pos.z}, 4.8f);

    // Showroom Overhead Spotlights above each car
    auto spawnShowroomSpotlight = [this](const std::string& name, const glm::vec3& pos, const glm::vec3& color, float intensity) {
        const entt::entity lightEnt = m_registry.create();
        m_registry.emplace<TagComponent>(lightEnt, TagComponent{name});
        m_registry.emplace<TransformLocal>(lightEnt, TransformLocal{pos});
        m_registry.emplace<TransformWorld>(lightEnt);
        m_registry.emplace<PointLightComponent>(lightEnt, PointLightComponent{
            .color = color,
            .intensity = intensity,
            .radius = 22.0f
        });
        m_demoPointLights.push_back(lightEnt);
    };
    spawnShowroomSpotlight("Showroom_Light_GTR", {stand1Pos.x, 6.2f, stand1Pos.z}, {0.9f, 0.95f, 1.0f}, 35.0f);
    spawnShowroomSpotlight("Showroom_Light_NFS", {stand2Pos.x, 6.2f, stand2Pos.z}, {1.0f, 0.92f, 0.82f}, 35.0f);

    m_renderer.setLightDir(m_sunDirection);

    // 8. Player & Camera Initialization (facing North towards the arena)
    m_spawnPoint = {0.0f, 0.8f, 22.0f};

    const entt::entity camera = m_registry.create();
    m_registry.emplace<TagComponent>(camera, TagComponent{"MainCamera"});
    m_registry.emplace<TransformLocal>(camera, TransformLocal{m_spawnPoint + glm::vec3(0.f, m_character.eyeHeight, 0.f)});
    m_registry.emplace<TransformWorld>(camera);
    m_registry.emplace<CameraComponent>(camera);
    FreeFlyController controller{};
    controller.yaw = -1.5707963f; // -90 deg: looking North towards -Z
    controller.pitch = 0.0f;
    controller.lookSensitivity = m_mouseSensitivity;
    m_registry.emplace<FreeFlyController>(camera, controller);

    m_character.init(m_physics, m_spawnPoint);
    m_cameraMode = CameraMode::FirstPerson;
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
    m_registry.emplace<TagComponent>(entity, TagComponent{"DynamicCube"});
    m_registry.emplace<TransformLocal>(entity, TransformLocal{spawnPos});
    m_registry.emplace<TransformWorld>(entity);
    m_registry.emplace<MeshComponent>(entity, meshComp);
    m_registry.emplace<MeshGeometryComponent>(entity, MeshGeometryComponent{MeshGeometryType::Box, "", halfExtents});
    m_registry.emplace<RenderableTag>(entity);
    m_registry.emplace<RigidBodyComponent>(entity);
    m_registry.emplace<ColliderComponent>(entity, ColliderComponent{ColliderShapeType::Box, halfExtents, 0.5f, false, 1.0f});
    createDynamicBox(m_physics, m_registry, entity, halfExtents, 1.0f);
}

void SandboxApp::spawnDynamicConvexObject(const MeshComponent& meshComp, const std::vector<glm::vec3>& vertices) {
    if (vertices.empty()) return;
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
    m_registry.emplace<TagComponent>(entity, TagComponent{"DynamicConvex"});
    m_registry.emplace<TransformLocal>(entity, TransformLocal{spawnPos});
    m_registry.emplace<TransformWorld>(entity);
    m_registry.emplace<MeshComponent>(entity, meshComp);
    m_registry.emplace<RenderableTag>(entity);
    m_registry.emplace<RigidBodyComponent>(entity);
    m_registry.emplace<ColliderComponent>(entity, ColliderComponent{ColliderShapeType::ConvexHull, {0.5f, 0.5f, 0.5f}, 0.5f, false, 1.0f});
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
    m_registry.emplace<TagComponent>(entity, TagComponent{"Cannonball"});
    m_registry.emplace<TransformLocal>(entity, TransformLocal{spawnPos});
    m_registry.emplace<TransformWorld>(entity);
    m_registry.emplace<MeshComponent>(entity, m_sphereComp);
    m_registry.emplace<MeshGeometryComponent>(entity, MeshGeometryComponent{MeshGeometryType::Sphere, "", {0.4f, 0.f, 0.f}});
    m_registry.emplace<RenderableTag>(entity);
    m_registry.emplace<RigidBodyComponent>(entity);
    m_registry.emplace<ColliderComponent>(entity, ColliderComponent{ColliderShapeType::Sphere, {0.4f, 0.4f, 0.4f}, 0.4f, false, 4.0f});
    createDynamicSphere(m_physics, m_registry, entity, 0.4f, 4.0f, velocity);
    AudioEngine::instance().play2D("assets/sounds/shoot.wav", 0.75f);
    m_particles.spawnMuzzleFlash(spawnPos, forward);
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

    // Toggle cursor capture with ESC or ToggleCursor
    if (m_inputMap.actionPressed(m_input, Action::ToggleCursor) || m_input.keyPressed(GLFW_KEY_ESCAPE)) {
        setCursorCapture(!m_cursorCaptured);
    }

    // In UI mode, clicking on empty 3D viewport recaptures cursor
    if (!m_cursorCaptured && !mouseCaptured && m_input.mouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        setCursorCapture(true);
    }

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

            // CS2 / Apex Legends style direct mouse steering
            if (m_cursorCaptured) {
                const glm::vec2 delta = m_inputMap.lookDelta();
                if (delta.x != 0.f || delta.y != 0.f) {
                    controller.yaw += delta.x * m_mouseSensitivity;
                    controller.pitch -= delta.y * m_mouseSensitivity;
                    controller.pitch = std::clamp(controller.pitch, -1.52f, 1.52f);
                }
            } else if (!mouseCaptured && m_inputMap.actionDown(m_input, Action::Look)) {
                // Secondary fallback when cursor is released in UI mode: hold RMB to look
                const glm::vec2 delta = m_inputMap.lookDelta();
                controller.yaw += delta.x * m_mouseSensitivity;
                controller.pitch -= delta.y * m_mouseSensitivity;
                controller.pitch = std::clamp(controller.pitch, -1.52f, 1.52f);
            }

            // Tactical Sprint (Shift)
            const bool isSprinting = !keyboardCaptured && m_inputMap.actionDown(m_input, Action::Sprint);
            m_character.walkSpeed = isSprinting ? 9.5f : 5.5f;

            glm::vec2 moveInput{0.0f};
            bool jump = false;
            if (!keyboardCaptured) {
                if (m_inputMap.actionDown(m_input, Action::MoveForward)) moveInput.y += 1.0f;
                if (m_inputMap.actionDown(m_input, Action::MoveBack))    moveInput.y -= 1.0f;
                if (m_inputMap.actionDown(m_input, Action::MoveRight))   moveInput.x += 1.0f;
                if (m_inputMap.actionDown(m_input, Action::MoveLeft))    moveInput.x -= 1.0f;
                if (m_inputMap.actionPressed(m_input, Action::Jump)) {
                    jump = true;
                    if (m_character.isGrounded()) {
                        AudioEngine::instance().play2D("assets/sounds/jump.wav", 0.7f);
                    }
                }
            }

            m_character.update(m_physics, deltaTime, moveInput, controller.yaw, jump);
            camTransform.translation = m_character.position() + glm::vec3(0.0f, m_character.eyeHeight, 0.0f);
        }
    } else {
        updateFreeFlyCamera(m_registry, m_input, m_inputMap, deltaTime, m_cursorCaptured);
    }

    // Update 3D audio listener from active camera
    auto camView = m_registry.view<TransformLocal, CameraComponent>();
    if (camView.begin() != camView.end()) {
        const auto camEnt = *camView.begin();
        const auto& camTransform = camView.get<TransformLocal>(camEnt);
        const auto* controller = m_registry.try_get<FreeFlyController>(camEnt);
        const float yaw = controller ? controller->yaw : 0.f;
        const float pitch = controller ? controller->pitch : 0.f;
        const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch)};
        const glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.f, 1.f, 0.f)));
        const glm::vec3 up = glm::normalize(glm::cross(right, forward));
        AudioEngine::instance().updateListener(camTransform.translation, forward, up);
    }

    if (m_rotateTurntables) {
        m_carRotationAngle += deltaTime * m_turntableSpeed;
        if (m_carRotationAngle > glm::two_pi<float>()) {
            m_carRotationAngle -= glm::two_pi<float>();
        }
        const glm::quat carRot = glm::angleAxis(m_carRotationAngle, glm::vec3(0.f, 1.f, 0.f));
        for (const entt::entity ent : m_car1Entities) {
            if (m_registry.valid(ent)) {
                if (auto* t = m_registry.try_get<TransformLocal>(ent)) {
                    t->rotation = carRot;
                }
            }
        }
        for (const entt::entity ent : m_car2Entities) {
            if (m_registry.valid(ent)) {
                if (auto* t = m_registry.try_get<TransformLocal>(ent)) {
                    t->rotation = carRot;
                }
            }
        }
    }

    if (!keyboardCaptured) {
        if (m_inputMap.actionPressed(m_input, Action::SpawnBox) ||
            (m_cameraMode == CameraMode::FreeFly && m_input.keyPressed(GLFW_KEY_SPACE))) {
            spawnDynamicObject(m_cubeComp, {0.5f, 0.5f, 0.5f});
        }
    }

    if (m_cursorCaptured || !mouseCaptured) {
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

    // Fail-safe boundary check / Killzone (prevents falling off the world)
    const glm::vec3 charPos = m_character.position();
    if (charPos.y < -5.0f || std::abs(charPos.x) > 65.0f || std::abs(charPos.z) > 65.0f) {
        respawnPlayer();
    }
    if (m_cameraMode == CameraMode::FreeFly) {
        auto freeFlyCamView = m_registry.view<TransformLocal, CameraComponent>();
        for (const auto camEnt : freeFlyCamView) {
            auto& t = freeFlyCamView.get<TransformLocal>(camEnt);
            if (t.translation.y < -5.0f || std::abs(t.translation.x) > 90.0f || std::abs(t.translation.z) > 90.0f) {
                t.translation = m_spawnPoint + glm::vec3(0.0f, m_character.eyeHeight, 0.0f);
            }
        }
    }
    m_renderer.tryReloadShaders();

    m_particles.update(deltaTime);

    auto emitterView = m_registry.view<const TransformWorld, ParticleEmitterComponent>();
    for (const auto entity : emitterView) {
        const auto& tw = emitterView.get<const TransformWorld>(entity);
        auto& emitter = emitterView.get<ParticleEmitterComponent>(entity);
        if (!emitter.active) continue;
        emitter.timer += deltaTime;
        const float interval = 1.0f / std::max(1.0f, emitter.spawnRate);
        while (emitter.timer >= interval) {
            emitter.timer -= interval;
            const glm::vec3 pos = glm::vec3(tw.matrix[3]);
            m_particles.spawn(pos, emitter.initialVelocity, emitter.startColor, emitter.endColor,
                              emitter.startSize, emitter.endSize, emitter.lifetime);
        }
    }

    if (m_sceneStatusTimer > 0.f) {
        m_sceneStatusTimer -= deltaTime;
        if (m_sceneStatusTimer <= 0.f) {
            m_sceneStatusMessage.clear();
        }
    }
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

    if (m_enableShadows) {
        m_renderer.recordShadowPass(cmd, m_registry, *m_meshes);
    }

    // 1. 3D Scene HDR Offscreen Render Pass
    std::array<VkClearValue, 2> hdrClearValues{};
    hdrClearValues[0].color = {{0.06f, 0.07f, 0.09f, 1.f}};
    hdrClearValues[1].depthStencil = {1.f, 0};

    VkRenderPassBeginInfo hdrPassInfo{};
    hdrPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    hdrPassInfo.renderPass = m_vulkan.hdrRenderPass();
    hdrPassInfo.framebuffer = m_vulkan.hdrFramebuffer();
    hdrPassInfo.renderArea.extent = m_vulkan.swapchainExtent();
    hdrPassInfo.clearValueCount = static_cast<uint32_t>(hdrClearValues.size());
    hdrPassInfo.pClearValues = hdrClearValues.data();

    vkCmdBeginRenderPass(cmd, &hdrPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    const float aspect =
        static_cast<float>(m_vulkan.swapchainExtent().width) /
        static_cast<float>(std::max(1u, m_vulkan.swapchainExtent().height));
    const CameraState camera = findActiveCamera(m_registry, aspect);

    m_renderer.recordScene(cmd, m_registry, *m_meshes, *m_textures, camera);
    m_particles.record(cmd, camera);
    if (m_showDebug) {
        m_debugDraw.record(cmd, m_registry, camera);
    }

    vkCmdEndRenderPass(cmd);

    // 2. Multi-pass Bloom Pyramid (Downsampling & Upsampling)
    m_postProcess.recordBloom(cmd);

    // 3. Swapchain Presentation Pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_vulkan.renderPass();
    renderPassInfo.framebuffer = m_vulkan.framebuffers()[imageIndex];
    renderPassInfo.renderArea.extent = m_vulkan.swapchainExtent();
    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues = nullptr;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 4. Tone Mapping & Bloom Composite Quad
    m_postProcess.recordComposite(cmd);

    m_imgui.beginFrame();

    // Tactical Crosshair (CS2 / Apex style) in FPS mode with locked cursor
    if (m_cameraMode == CameraMode::FirstPerson && m_cursorCaptured) {
        ImDrawList* drawList = ImGui::GetForegroundDrawList();
        const ImVec2 center = ImVec2(
            static_cast<float>(m_vulkan.swapchainExtent().width) * 0.5f,
            static_cast<float>(m_vulkan.swapchainExtent().height) * 0.5f
        );
        const float gap = 4.0f;
        const float length = 8.0f;
        const ImU32 crossColor = IM_COL32(0, 255, 140, 230);
        const ImU32 shadowColor = IM_COL32(0, 0, 0, 180);

        // Center dot
        drawList->AddCircleFilled(center, 1.5f, crossColor);

        // Shadow outline lines
        drawList->AddLine(ImVec2(center.x, center.y - gap - length), ImVec2(center.x, center.y - gap), shadowColor, 2.5f);
        drawList->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + length), shadowColor, 2.5f);
        drawList->AddLine(ImVec2(center.x - gap - length, center.y), ImVec2(center.x - gap, center.y), shadowColor, 2.5f);
        drawList->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + length, center.y), shadowColor, 2.5f);

        // Crisp inner lines
        drawList->AddLine(ImVec2(center.x, center.y - gap - length), ImVec2(center.x, center.y - gap), crossColor, 1.5f);
        drawList->AddLine(ImVec2(center.x, center.y + gap), ImVec2(center.x, center.y + gap + length), crossColor, 1.5f);
        drawList->AddLine(ImVec2(center.x - gap - length, center.y), ImVec2(center.x - gap, center.y), crossColor, 1.5f);
        drawList->AddLine(ImVec2(center.x + gap, center.y), ImVec2(center.x + gap + length, center.y), crossColor, 1.5f);
    }

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
    ImGui::Text("Camera & Mouse Controls:");
    int currentMode = (m_cameraMode == CameraMode::FreeFly) ? 0 : 1;
    if (ImGui::RadioButton("Free-Fly (F1)", &currentMode, 0)) {
        if (m_cameraMode != CameraMode::FreeFly) toggleCameraMode();
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("FPS Mode (F1)", &currentMode, 1)) {
        if (m_cameraMode != CameraMode::FirstPerson) toggleCameraMode();
    }

    if (ImGui::Button(m_cursorCaptured ? "Release Mouse (ESC)" : "Capture Mouse (ESC / Click Viewport)")) {
        setCursorCapture(!m_cursorCaptured);
    }
    ImGui::SameLine();
    if (ImGui::Button("Respawn at Origin")) {
        respawnPlayer();
    }

    ImGui::SliderFloat("Mouse Sensitivity", &m_mouseSensitivity, 0.0005f, 0.01f, "%.4f");

    if (m_cameraMode == CameraMode::FirstPerson) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "WASD: Move | Shift: Sprint | Space: Jump | Mouse: Aim | LMB: Shoot");
        ImGui::Text("Grounded: %s", m_character.isGrounded() ? "YES" : "NO");
        const glm::vec3 p = m_character.position();
        ImGui::Text("Pos: (%.2f, %.2f, %.2f)", p.x, p.y, p.z);
    }

    ImGui::Separator();
    ImGui::Text("Showroom Turntables:");
    ImGui::Checkbox("Rotate Cars", &m_rotateTurntables);
    ImGui::SameLine();
    ImGui::SliderFloat("Speed", &m_turntableSpeed, 0.05f, 2.0f, "%.2f rad/s");

    ImGui::Separator();
    ImGui::Text("Physics Actions:");
    if (ImGui::Button("Spawn Cube (B / Space)")) {
        spawnDynamicObject(m_cubeComp, {0.5f, 0.5f, 0.5f});
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

        if (auto* tagComp = m_registry.try_get<TagComponent>(m_selectedEntity)) {
            char tagBuf[128]{};
            strncpy_s(tagBuf, tagComp->tag.c_str(), sizeof(tagBuf) - 1);
            if (ImGui::InputText("Name / Tag", tagBuf, sizeof(tagBuf))) {
                tagComp->tag = tagBuf;
            }
        }

        if (auto* transform = m_registry.try_get<TransformLocal>(m_selectedEntity)) {
            ImGui::DragFloat3("Position", &transform->translation.x, 0.05f);
            ImGui::DragFloat3("Scale", &transform->scale.x, 0.05f, 0.01f, 100.0f);
        }

        if (auto* meshComp = m_registry.try_get<MeshComponent>(m_selectedEntity)) {
            ImGui::ColorEdit3("Tint", &meshComp->tint.x);
            ImGui::SliderFloat("Metallic", &meshComp->metallic, 0.0f, 1.0f);
            ImGui::SliderFloat("Roughness", &meshComp->roughness, 0.0f, 1.0f);
            ImGui::SliderFloat("Emissive Intensity", &meshComp->emissiveIntensity, 0.0f, 30.0f, "%.1f");
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
    ImGui::Text("Scene Management:");
    ImGui::InputText("Scene File", m_sceneFilename, sizeof(m_sceneFilename));
    if (ImGui::Button("Save Scene")) {
        saveScene(m_sceneFilename);
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Scene")) {
        loadScene(m_sceneFilename);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Scene")) {
        resetScene();
    }
    if (!m_sceneStatusMessage.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", m_sceneStatusMessage.c_str());
    }

    ImGui::Separator();
    ImGui::Text("Lighting & Render:");
    if (ImGui::SliderFloat3("Sun Dir", &m_sunDirection.x, -1.f, 1.f)) {
        m_renderer.setLightDir(m_sunDirection);
    }
    ImGui::Checkbox("Enable Shadows", &m_enableShadows);
    ImGui::Checkbox("Animate Demo Lights", &m_animateLights);
    if (ImGui::Button("Add Light at Camera")) {
        auto camView = m_registry.view<TransformLocal, CameraComponent>();
        if (camView.begin() != camView.end()) {
            const auto camEnt = *camView.begin();
            const auto& camTransform = camView.get<TransformLocal>(camEnt);
            const entt::entity newLight = m_registry.create();
            m_registry.emplace<TagComponent>(newLight, TagComponent{"CustomPointLight"});
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
            markerComp.emissiveIntensity = 8.0f;
            m_registry.emplace<MeshComponent>(newLight, markerComp);
            m_registry.emplace<MeshGeometryComponent>(newLight, MeshGeometryComponent{MeshGeometryType::Sphere, "", {0.4f, 0.f, 0.f}});
            m_registry.emplace<RenderableTag>(newLight);
            m_selectedEntity = newLight;
        }
    }
    ImGui::Checkbox("Debug draw", &m_showDebug);
    bool cullBackfaces = (m_renderer.cullMode() != VK_CULL_MODE_NONE);
    if (ImGui::Checkbox("Cull backfaces", &cullBackfaces)) {
        m_renderer.setCullMode(cullBackfaces ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
    }

    ImGui::Separator();
    ImGui::Text("Cinematic Post-Processing (HDR & Bloom):");
    auto& pp = m_postProcess.settings();
    ImGui::Checkbox("Enable Bloom", &pp.bloomEnabled);
    if (pp.bloomEnabled) {
        ImGui::SliderFloat("Bloom Intensity", &pp.bloomIntensity, 0.0f, 3.0f, "%.2f");
        ImGui::SliderFloat("Bloom Threshold", &pp.bloomThreshold, 0.1f, 5.0f, "%.2f");
        ImGui::SliderFloat("Bloom Knee (Softness)", &pp.bloomKnee, 0.01f, 2.0f, "%.2f");
        ImGui::SliderFloat("Filter Radius", &pp.filterRadius, 0.1f, 3.0f, "%.2f");
    }
    ImGui::SliderFloat("Exposure", &pp.exposure, 0.1f, 5.0f, "%.2f");
    const char* toneMappers[] = {"ACES Filmic", "Khronos PBR Neutral", "Reinhard", "Linear (Unclamped)"};
    ImGui::Combo("Tone Mapper", &pp.toneMapper, toneMappers, IM_ARRAYSIZE(toneMappers));

    ImGui::Separator();
    ImGui::Text("Particle VFX System:");
    ImGui::Text("Active Particles: %zu", m_particles.activeParticleCount());
    if (ImGui::Button("Sparks Burst at Origin")) {
        m_particles.spawnBurst(glm::vec3(0.f, 1.5f, 0.f), 60, glm::vec4(1.f, 0.7f, 0.2f, 1.f), 6.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Fire Muzzle at Camera")) {
        auto camView = m_registry.view<TransformLocal, CameraComponent>();
        if (camView.begin() != camView.end()) {
            const auto camEnt = *camView.begin();
            const auto& camTransform = camView.get<TransformLocal>(camEnt);
            const auto* controller = m_registry.try_get<FreeFlyController>(camEnt);
            const float yaw = controller ? controller->yaw : 0.f;
            const float pitch = controller ? controller->pitch : 0.f;
            const glm::vec3 forward{std::cos(yaw) * std::cos(pitch), std::sin(pitch), std::sin(yaw) * std::cos(pitch)};
            m_particles.spawnMuzzleFlash(camTransform.translation + forward * 1.5f, forward);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear VFX")) {
        m_particles.clear();
    }

    ImGui::Separator();
    ImGui::Text("Audio System (miniaudio):");
    if (ImGui::SliderFloat("Master Volume", &m_masterVolume, 0.0f, 1.0f, "%.2f")) {
        AudioEngine::instance().setMasterVolume(m_masterVolume);
    }
    if (ImGui::Button("Play 2D Test")) {
        AudioEngine::instance().play2D("assets/sounds/test.wav", 0.8f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Play 3D Origin")) {
        AudioEngine::instance().play3D("assets/sounds/test.wav", glm::vec3(0.0f, 1.0f, 0.0f), 1.0f, 1.0f, 25.0f);
    }
    ImGui::SameLine();
    if (ImGui::Button("Play Shoot")) {
        AudioEngine::instance().play2D("assets/sounds/shoot.wav", 0.8f);
    }
    ImGui::End();
    m_imgui.endFrame(cmd);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    const VkResult presentResult = m_vulkan.submitAndPresent(imageIndex);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        const auto size = m_platform.framebufferSize();
        if (size.x > 0 && size.y > 0) {
            handleResize(WindowResizeEvent{size.x, size.y});
            ImGui_ImplVulkan_SetMinImageCount(
                static_cast<uint32_t>(std::max(2, static_cast<int>(m_vulkan.framebuffers().size()))));
        }
    }
    return true;
}

void SandboxApp::saveScene(const std::string& filename) {
    const std::filesystem::path scenePath = std::filesystem::path("assets/scenes") / filename;
    if (SceneSerializer::serialize(scenePath, m_registry, m_sunDirection)) {
        m_sceneStatusMessage = "Saved: " + scenePath.string();
        m_sceneStatusTimer = 4.0f;
    } else {
        m_sceneStatusMessage = "Error saving scene: " + scenePath.string();
        m_sceneStatusTimer = 4.0f;
    }
}

void SandboxApp::loadScene(const std::string& filename) {
    const std::filesystem::path scenePath = std::filesystem::path("assets/scenes") / filename;
    if (!std::filesystem::exists(scenePath)) {
        m_sceneStatusMessage = "File not found: " + scenePath.string();
        m_sceneStatusTimer = 4.0f;
        return;
    }

    m_selectedEntity = entt::null;
    m_demoPointLights.clear();

    SceneResourceContext resCtx{};
    resCtx.uploadMesh = [this](const MeshCpuData& cpuData) -> uint32_t {
        return m_meshes->upload(cpuData);
    };
    resCtx.spawnModel = [this](const std::filesystem::path& path, const glm::vec3& pos, float targetSize) {
        spawnGltfModel(m_registry, m_assets, *m_meshes, *m_textures, path, pos, targetSize);
    };
    resCtx.createBoxCollider = [this](entt::entity entity, const glm::vec3& halfExtents, bool isStatic, float mass) {
        m_registry.emplace<RigidBodyComponent>(entity);
        if (isStatic) {
            m_registry.emplace<StaticColliderTag>(entity);
            createStaticBox(m_physics, m_registry, entity, halfExtents);
        } else {
            createDynamicBox(m_physics, m_registry, entity, halfExtents, mass);
        }
    };
    resCtx.createSphereCollider = [this](entt::entity entity, float radius, bool isStatic, float mass) {
        m_registry.emplace<RigidBodyComponent>(entity);
        if (isStatic) {
            m_registry.emplace<StaticColliderTag>(entity);
        } else {
            createDynamicSphere(m_physics, m_registry, entity, radius, mass);
        }
    };
    resCtx.createConvexHullCollider = [](entt::entity /*entity*/, float /*mass*/) {
    };
    resCtx.destroyPhysicsBody = [this](entt::entity entity) {
        destroyPhysicsBody(m_physics, m_registry, entity);
    };
    resCtx.clearPhysicsBodies = [this]() {
        destroyPhysicsBodies(m_registry, m_physics);
    };
    resCtx.teapotMeshData = nullptr;

    if (SceneSerializer::deserialize(scenePath, m_registry, m_sunDirection, resCtx)) {
        m_renderer.setLightDir(m_sunDirection);
        auto lightView = m_registry.view<PointLightComponent>();
        for (const auto lightEnt : lightView) {
            m_demoPointLights.push_back(lightEnt);
        }
        auto camView = m_registry.view<TransformLocal, CameraComponent>();
        if (camView.begin() != camView.end()) {
            auto camEnt = *camView.begin();
            m_character.setPosition(m_registry.get<TransformLocal>(camEnt).translation - glm::vec3(0.f, m_character.eyeHeight, 0.f));
        }
        m_sceneStatusMessage = "Loaded: " + scenePath.string();
        m_sceneStatusTimer = 4.0f;
    } else {
        m_sceneStatusMessage = "Error loading scene: " + scenePath.string();
        m_sceneStatusTimer = 4.0f;
    }
}

void SandboxApp::resetScene() {
    m_selectedEntity = entt::null;
    m_demoPointLights.clear();
    destroyPhysicsBodies(m_registry, m_physics);
    m_registry.clear();
    spawnScene();
    m_sceneStatusMessage = "Scene reset to default";
    m_sceneStatusTimer = 3.0f;
}

void SandboxApp::shutdownEngine() {
    vkDeviceWaitIdle(m_vulkan.device());
    destroyPhysicsBodies(m_registry, m_physics);
    m_debugDraw.shutdown();
    if (m_textures) {
        m_textures->shutdown();
    }
    m_renderer.shutdown();
    m_postProcess.shutdown();
    if (m_meshes) {
        m_meshes->clear();
    }
    m_imgui.shutdown();
    m_particles.shutdown();
    AudioEngine::instance().shutdown();
    m_vulkan.shutdown();
    m_platform.shutdown();
}

int SandboxApp::run(int argc, char** argv) {
    bool smokeTest = false;
    for (int i = 0; i < argc; ++i) {
        if (std::string(argv[i]) == "--smoke-test") {
            smokeTest = true;
        }
    }

    loadConfig();
    setupInput();
    if (!initEngine()) {
        return 1;
    }

    int frameCount = 0;
    while (!m_platform.shouldClose()) {
        m_input.beginFrame();
        m_platform.pollEvents();
        m_time.tick();
        updateFrame(m_time.delta());
        if (!renderFrame()) {
            break;
        }
        m_input.endFrame();

        if (smokeTest && ++frameCount >= 100) {
            log(LogLevel::Info, "Smoke test completed 100 frames successfully.");
            break;
        }
    }

    shutdownEngine();
    return 0;
}

} // namespace engine
