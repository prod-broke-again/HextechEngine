#include "ui/TopHudPanel.hpp"
#include "ui/EvolutionBannerPanel.hpp"
#include "ui/BuildDockPanel.hpp"
#include "ui/InspectorPanel.hpp"
#include "ui/HoverTooltipPanel.hpp"
#include "EraSandboxModule.hpp"

#include "engine/assets/MeshBuilder.hpp"
#include "engine/ui/ComponentInspector.hpp"
#include "engine/ui/EditorChrome.hpp"
#include "engine/ui/TransformGizmo.hpp"
#include "engine/modules/save/SaveModule.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/core/Log.hpp"
#include "engine/core/Events.hpp"
#include "engine/core/Input.hpp"
#include "engine/core/Path.hpp"
#include "engine/ecs/Systems.hpp"

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

namespace {

void handleSceneRequests(engine::World& world, engine::ui::EditorHistory& history,
                         engine::ui::SceneSaveControls& scene, glm::vec3& sun) {
    if (!scene.saveRequested && !scene.loadRequested) {
        return;
    }
    const std::filesystem::path path = scene.path;
    if (scene.saveRequested) {
        if (path.has_parent_path()) {
            std::filesystem::create_directories(path.parent_path());
        }
        if (engine::SaveModule::saveSceneJson(path, world.registry(), world.types(), sun)) {
            scene.status = "Saved " + path.string();
        } else {
            scene.status = "Save failed: " + path.string();
        }
    }
    if (scene.loadRequested) {
        if (engine::SaveModule::loadSceneJson(path, world.registry(), world.types(), sun)) {
            history.clear();
            scene.status = "Loaded " + path.string();
        } else {
            scene.status = "Load failed: " + path.string();
        }
    }
}

void tickEditorGizmo(engine::ui::GizmoState& gizmo, engine::ui::EditorHistory& history,
                     bool& wasDragging, glm::vec3& startT, glm::quat& startR,
                     entt::registry& registry, entt::entity selected, const engine::CameraState& camera,
                     const engine::Input& input, const glm::vec2& viewport, bool allowPick) {
    if (selected == entt::null || !registry.valid(selected)) {
        return;
    }
    auto* transform = registry.try_get<engine::TransformLocal>(selected);
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

namespace engine::era {

namespace {

MeshCpuData createGridMesh(int gridSize, float tileSize) {
    MeshCpuData mesh;
    mesh.vertices.reserve(gridSize * gridSize * 4 + 16);
    mesh.indices.reserve(gridSize * gridSize * 6 + 24);

    const glm::vec3 normal{0.0f, 1.0f, 0.0f};

    // 1. Playable 32x32 checkered terrain tiles
    for (int z = 0; z < gridSize; ++z) {
        for (int x = 0; x < gridSize; ++x) {
            const float x0 = static_cast<float>(x) * tileSize;
            const float z0 = static_cast<float>(z) * tileSize;
            const float x1 = x0 + tileSize;
            const float z1 = z0 + tileSize;

            const bool isEven = ((x + z) % 2 == 0);
            const glm::vec3 tileColor = isEven ? glm::vec3(0.28f, 0.46f, 0.22f)
                                               : glm::vec3(0.24f, 0.40f, 0.19f);

            const uint32_t base = static_cast<uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back({glm::vec3(x0, 0.0f, z0), normal, glm::vec2(0.f, 0.f), tileColor});
            mesh.vertices.push_back({glm::vec3(x1, 0.0f, z0), normal, glm::vec2(1.f, 0.f), tileColor});
            mesh.vertices.push_back({glm::vec3(x1, 0.0f, z1), normal, glm::vec2(1.f, 1.f), tileColor});
            mesh.vertices.push_back({glm::vec3(x0, 0.0f, z1), normal, glm::vec2(0.f, 1.f), tileColor});

            mesh.indices.push_back(base);
            mesh.indices.push_back(base + 2);
            mesh.indices.push_back(base + 1);
            mesh.indices.push_back(base);
            mesh.indices.push_back(base + 3);
            mesh.indices.push_back(base + 2);
        }
    }

    // 2. Surrounding skirt / forest border (gives a nice island/tabletop boundary)
    const float borderMin = -12.0f;
    const float borderMax = static_cast<float>(gridSize) * tileSize + 12.0f;
    const float ySkirt = -0.04f;
    const glm::vec3 skirtColor{0.14f, 0.24f, 0.12f};

    auto addQuad = [&](float x0, float z0, float x1, float z1) {
        const uint32_t b = static_cast<uint32_t>(mesh.vertices.size());
        mesh.vertices.push_back({glm::vec3(x0, ySkirt, z0), normal, glm::vec2(0.f, 0.f), skirtColor});
        mesh.vertices.push_back({glm::vec3(x1, ySkirt, z0), normal, glm::vec2(1.f, 0.f), skirtColor});
        mesh.vertices.push_back({glm::vec3(x1, ySkirt, z1), normal, glm::vec2(1.f, 1.f), skirtColor});
        mesh.vertices.push_back({glm::vec3(x0, ySkirt, z1), normal, glm::vec2(0.f, 1.f), skirtColor});

        mesh.indices.push_back(b);
        mesh.indices.push_back(b + 2);
        mesh.indices.push_back(b + 1);
        mesh.indices.push_back(b);
        mesh.indices.push_back(b + 3);
        mesh.indices.push_back(b + 2);
    };

    const float gMax = static_cast<float>(gridSize) * tileSize;
    addQuad(borderMin, borderMin, borderMax, 0.0f);     // North skirt
    addQuad(borderMin, gMax, borderMax, borderMax);     // South skirt
    addQuad(borderMin, 0.0f, 0.0f, gMax);               // West skirt
    addQuad(gMax, 0.0f, borderMax, gMax);               // East skirt

    return mesh;
}

MeshCpuData createHoverQuadMesh() {
    MeshCpuData mesh;
    const float h = 0.47f; // slightly smaller than 0.5 to show tile boundaries
    const glm::vec3 normal{0.0f, 1.0f, 0.0f};
    const glm::vec3 color{0.2f, 1.0f, 0.85f};

    mesh.vertices = {
        {glm::vec3(-h, 0.0f, -h), normal, glm::vec2(0.f, 0.f), color},
        {glm::vec3(h,  0.0f, -h), normal, glm::vec2(1.f, 0.f), color},
        {glm::vec3(h,  0.0f,  h), normal, glm::vec2(1.f, 1.f), color},
        {glm::vec3(-h, 0.0f,  h), normal, glm::vec2(0.f, 1.f), color},
    };
    mesh.indices = {0, 2, 1, 0, 3, 2};
    return mesh;
}

} // namespace

void EraSandboxModule::registerTypes(TypeRegistry& registry) {
    registerEngineComponents(registry);
    CitySystems::registerCityTypes(registry);
}

void EraSandboxModule::registerCommands(CommandRegistry& registry) {
    registerCityCommands(registry);
}

void EraSandboxModule::onAttach(World& world) {
    m_world = &world;
    m_sceneControls.path = "saves/era_scene.json";
    m_world->resource<PlatformGLFW>().setCursorCaptured(false);
    
    // Bind events
    world.events().connect<BuildingPlacedEvent, &EraSandboxModule::spawnVisualBuilding>(this);
    world.events().connect<BuildingRemovedEvent, &EraSandboxModule::removeVisualBuilding>(this);
    world.events().connect<CarrierSpawnedEvent, &EraSandboxModule::spawnVisualCarrier>(this);
    world.events().connect<BuildingStatusEvent, &EraSandboxModule::spawnAlertIndicator>(this);
    world.events().connect<EraEvolvedEvent, &EraSandboxModule::onEraEvolved>(this);

    initEraData();
    CitySystems::initCity(world);

    setupScene(world);
}

void EraSandboxModule::onDetach(World& world) {
    world.events().disconnect<BuildingPlacedEvent>(this);
    world.events().disconnect<BuildingRemovedEvent>(this);
    world.events().disconnect<CarrierSpawnedEvent>(this);
    world.events().disconnect<BuildingStatusEvent>(this);
    world.events().disconnect<EraEvolvedEvent>(this);

    m_alertEntities.clear();
    if (m_world) {
        vkDeviceWaitIdle(m_world->resource<VulkanContext>().device());
    }
    m_world = nullptr;
}

void EraSandboxModule::tick(World& world) {
    CitySystems::update(world, world.tick());
}

void EraSandboxModule::render(World& world, float /*alpha*/) {
    if (world.resource<PlatformGLFW>().shouldClose()) return;
    if (m_smokeTest) {
        if (++m_frameCount >= 100) {
            log(LogLevel::Info, "Smoke test completed 100 frames successfully.");
            world.resource<PlatformGLFW>().setWindowShouldClose(true);
            world.events().enqueue(WindowCloseEvent{});
            return;
        }
    }
    updateFrame(world, 1.0f / 30.0f);
    renderFrame(world);
}
void EraSandboxModule::spawnVisualBuilding(const BuildingPlacedEvent& ev) {
    World& world = *m_world;
    auto& state = world.resource<CityState>();
    auto& registry = world.registry();
    
    auto& mc = registry.emplace<MeshComponent>(ev.entity);
    mc.mesh = m_cityMeshes.getBuildingMesh(ev.type, state.currentEra);

    auto& t = registry.emplace<TransformLocal>(ev.entity);
    t.translation = glm::vec3(
        static_cast<float>(ev.gridX) * kTileSize + 0.5f * kTileSize,
        0.0f,
        static_cast<float>(ev.gridZ) * kTileSize + 0.5f * kTileSize
    );
    
    const float randomYaw = (static_cast<float>((ev.gridX * 73856093) ^ (ev.gridZ * 19349663)) / static_cast<float>(0xFFFFFFFF)) * 6.28f;
    t.rotation = glm::angleAxis(randomYaw, glm::vec3(0.0f, 1.0f, 0.0f));

    if (ev.type == BuildingIds::TownCenter) {
        mc.tint = glm::vec3(1.1f, 1.0f, 0.8f);
    }
}

void EraSandboxModule::removeVisualBuilding(const BuildingRemovedEvent& ev) {
    removeAlertIndicator(ev.entity);
}

void EraSandboxModule::spawnVisualCarrier(const CarrierSpawnedEvent& ev) {
    World& world = *m_world;
    auto& registry = world.registry();
    auto& mc = registry.emplace<MeshComponent>(ev.entity);
    mc.mesh = m_cityMeshes.getCarrierMesh(world.resource<CityState>().currentEra);
    mc.tint = glm::vec3(0.9f, 0.5f, 0.2f);
    registry.emplace<TransformLocal>(ev.entity);
}

void EraSandboxModule::spawnAlertIndicator(const BuildingStatusEvent& ev) {
    World& world = *m_world;
    if (!ev.active) {
        removeAlertIndicator(ev.entity);
        return;
    }
    
    if (m_alertEntities.contains(ev.entity)) {
        return; 
    }

    entt::entity alertEnt = world.registry().create();
    auto& mc = world.registry().emplace<MeshComponent>(alertEnt);
    mc.mesh = m_cityMeshes.getAlertIconMesh();

    if (ev.alert == BuildingAlertKind::StorageFull) {
        mc.tint = glm::vec3(1.0f, 0.8f, 0.1f);
        mc.emissiveIntensity = 1.0f;
    } else if (ev.alert == BuildingAlertKind::MissingInput) {
        mc.tint = glm::vec3(1.0f, 0.2f, 0.2f);
        mc.emissiveIntensity = 1.0f;
    }

    world.registry().emplace<TransformLocal>(alertEnt);
    m_alertEntities[ev.entity] = alertEnt;
}

void EraSandboxModule::removeAlertIndicator(entt::entity buildingId) {
    World& world = *m_world;
    auto it = m_alertEntities.find(buildingId);
    if (it != m_alertEntities.end()) {
        if (world.registry().valid(it->second)) {
            world.registry().destroy(it->second);
        }
        m_alertEntities.erase(it);
    }
}

void EraSandboxModule::onEraEvolved(const EraEvolvedEvent& ev) {
    World& world = *m_world;
    auto view = world.registry().view<BuildingComponent, MeshComponent>();
    for (auto [e, b, mc] : view.each()) {
        mc.mesh = m_cityMeshes.getBuildingMesh(b.type, ev.newEra);
    }
}

void EraSandboxModule::setupScene(World& world) {
    // 1. Terrain Grid (32x32 tiles + outer border)
    const MeshCpuData gridMesh = createGridMesh(kGridSize, kTileSize);
    const uint32_t gridMeshId = m_world->resource<GpuMeshCache>().upload(gridMesh);

    const entt::entity gridEnt = m_world->registry().create();
    m_world->registry().emplace<TagComponent>(gridEnt, TagComponent{"TerrainGrid"});
    m_world->registry().emplace<TransformLocal>(gridEnt, TransformLocal{glm::vec3(0.0f)});
    m_world->registry().emplace<TransformWorld>(gridEnt);

    MeshComponent gridComp{};
    gridComp.mesh = gridMeshId;
    gridComp.roughness = 0.9f;
    gridComp.metallic = 0.0f;
    m_world->registry().emplace<MeshComponent>(gridEnt, gridComp);
    m_world->registry().emplace<RenderableTag>(gridEnt);

    // 2. Cursor Hover Tile Quad
    const MeshCpuData hoverMesh = createHoverQuadMesh();
    const uint32_t hoverMeshId = m_world->resource<GpuMeshCache>().upload(hoverMesh);

    m_hoverTileEntity = m_world->registry().create();
    m_world->registry().emplace<TagComponent>(m_hoverTileEntity, TagComponent{"HoverTile"});
    m_world->registry().emplace<TransformLocal>(m_hoverTileEntity,
                                      TransformLocal{glm::vec3(0.0f), glm::quat{1.f, 0.f, 0.f, 0.f}, glm::vec3(0.0f)});
    m_world->registry().emplace<TransformWorld>(m_hoverTileEntity);

    MeshComponent hoverComp{};
    hoverComp.mesh = hoverMeshId;
    hoverComp.tint = glm::vec3(0.2f, 1.0f, 0.85f);
    hoverComp.emissiveIntensity = 2.5f;
    hoverComp.roughness = 0.2f;
    m_world->registry().emplace<MeshComponent>(m_hoverTileEntity, hoverComp);
    m_world->registry().emplace<RenderableTag>(m_hoverTileEntity);

    // 3. Ghost Preview Entity
    m_ghostEntity = m_world->registry().create();
    m_world->registry().emplace<TagComponent>(m_ghostEntity, TagComponent{"GhostPreview"});
    m_world->registry().emplace<TransformLocal>(m_ghostEntity,
                                      TransformLocal{glm::vec3(0.0f), glm::quat{1.f, 0.f, 0.f, 0.f}, glm::vec3(0.0f)});
    m_world->registry().emplace<TransformWorld>(m_ghostEntity);

    MeshComponent ghostComp{};
    ghostComp.mesh = m_cityMeshes.getBuildingMesh(BuildingIds::Residence, world.resource<CityState>().currentEra);
    ghostComp.tint = glm::vec3(0.3f, 1.0f, 0.3f);
    ghostComp.emissiveIntensity = 0.8f;
    ghostComp.roughness = 0.4f;
    m_world->registry().emplace<MeshComponent>(m_ghostEntity, ghostComp);
    m_world->registry().emplace<RenderableTag>(m_ghostEntity);

    // 4. Camera Entity
    m_cameraEntity = m_world->registry().create();
    m_world->registry().emplace<TagComponent>(m_cameraEntity, TagComponent{"RtsCamera"});
    m_world->registry().emplace<TransformLocal>(m_cameraEntity, TransformLocal{m_camera.eyePosition()});
    m_world->registry().emplace<TransformWorld>(m_cameraEntity);

    CameraComponent camComp{};
    camComp.fovDegrees = 50.0f;
    camComp.nearPlane = 0.1f;
    camComp.farPlane = 300.0f;
    m_world->registry().emplace<CameraComponent>(m_cameraEntity, camComp);

    FreeFlyController ctrl{};
    ctrl.yaw = m_camera.yaw();
    ctrl.pitch = m_camera.pitch();
    m_world->registry().emplace<FreeFlyController>(m_cameraEntity, ctrl);

    updateTransforms(m_world->registry());
}

void EraSandboxModule::updateFrame(World& world, float deltaTime) {
    m_time += deltaTime;
    const bool mouseCaptured = ImGui::GetIO().WantCaptureMouse;
    const bool keyboardCaptured = ImGui::GetIO().WantCaptureKeyboard;

    m_camera.update(m_world->resource<Input>(), deltaTime, !keyboardCaptured);

    const auto fbSize = m_world->resource<PlatformGLFW>().framebufferSize();
    const glm::vec2 screenSize{
        static_cast<float>(std::max(1, fbSize.x)),
        static_cast<float>(std::max(1, fbSize.y))
    };
    const float aspect = screenSize.x / screenSize.y;
    const CameraState camState = m_camera.getCameraState(aspect);

    if (m_world->registry().valid(m_cameraEntity)) {
        auto& t = m_world->registry().get<TransformLocal>(m_cameraEntity);
        t.translation = camState.position;
        if (auto* ctrl = m_world->registry().try_get<FreeFlyController>(m_cameraEntity)) {
            ctrl->yaw = m_camera.yaw();
            ctrl->pitch = m_camera.pitch();
        }
    }

    const glm::vec2 mousePos = m_world->resource<Input>().snapshot().mousePosition;
    if (!mouseCaptured && m_camera.unprojectCursorToPlane(mousePos, screenSize, 0.0f, m_groundHitPos, camState)) {
        int gx = 0, gz = 0;
        if (m_camera.getGridCoords(m_groundHitPos, kTileSize, kGridSize, gx, gz)) {
            m_hasHoverTile = true;
            m_hoverX = gx;
            m_hoverZ = gz;
        } else {
            m_hasHoverTile = false;
            m_hoverX = -1;
            m_hoverZ = -1;
        }
    } else {
        m_hasHoverTile = false;
        m_hoverX = -1;
        m_hoverZ = -1;
    }

    if (!mouseCaptured && m_hasHoverTile) {
        if (m_world->resource<Input>().mouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT)) {
            if (m_demolishMode) {
                m_world->commandQueue().enqueue(DemolishBuildingCmd{m_hoverX, m_hoverZ});
                const auto& grid = m_world->resource<GridIndex>();
                if (grid.cells[m_hoverZ][m_hoverX].entity == m_inspectedBuildingId) {
                    m_inspectedBuildingId = entt::null;
                }
            } else if (m_selectedBuildType != BuildingIds::None) {
                m_world->commandQueue().enqueue(PlaceBuildingCmd{m_hoverX, m_hoverZ, m_selectedBuildType});
                if (!m_world->resource<Input>().keyDown(GLFW_KEY_LEFT_SHIFT)) {
                    m_selectedBuildType = BuildingIds::None;
                }
            } else {
                const auto& grid = m_world->resource<GridIndex>();
                entt::entity e = grid.cells[m_hoverZ][m_hoverX].entity;
                m_inspectedBuildingId = (e != entt::null && m_world->registry().valid(e)) ? e : entt::null;
            }
        }
    }

    if (m_world->resource<Input>().mouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) || m_world->resource<Input>().keyPressed(GLFW_KEY_ESCAPE)) {
        if (m_selectedBuildType != BuildingIds::None || m_demolishMode) {
            m_selectedBuildType = BuildingIds::None;
            m_demolishMode = false;
        }
    }

    if (m_world->resource<Input>().keyPressed(GLFW_KEY_F5)) {
        std::cout << "[EraSandboxModule] F5 pressed: reloading configuration data...\n";
        m_world->commandQueue().enqueue(ReloadDataCmd{});
    }

    m_world->resource<ParticleSystem>().update(deltaTime);

    auto cView = m_world->registry().view<CarrierComponent, CarrierJourneyComponent, engine::statemachine::StateMachineComponent, TransformLocal>();
    glm::vec3 tcPos = CitySystems::getTownCenterPosition(*m_world);
    for (auto [e, c, j, sm, t] : cView.each()) {
        if (sm.current == CarrierStates::IdleAtWarehouse) {
            t.translation = j.currentPos;
            const glm::vec3 lookAway = j.currentPos - tcPos;
            if (glm::length(glm::vec2(lookAway.x, lookAway.z)) > 0.001f) {
                const float yaw = std::atan2(lookAway.x, lookAway.z);
                t.rotation = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
            }
        } else {
            const float bobY = std::abs(std::sin(j.bobbingTimer)) * 0.04f;
            t.translation = glm::vec3(j.currentPos.x, bobY, j.currentPos.z);

            const glm::vec3 moveDir = j.targetPos - j.startPos;
            if (glm::length(glm::vec2(moveDir.x, moveDir.z)) > 0.001f) {
                const float yaw = std::atan2(moveDir.x, moveDir.z);
                t.rotation = glm::angleAxis(yaw, glm::vec3(0.0f, 1.0f, 0.0f));
            }
        }
    }

    const float alertTime = static_cast<float>(m_time);
    for (auto it = m_alertEntities.begin(); it != m_alertEntities.end(); ) {
        entt::entity bId = it->first;
        entt::entity ent = it->second;
        if (m_world->registry().valid(bId) && m_world->registry().valid(ent)) {
            auto& pos = m_world->registry().get<GridPosition>(bId);
            auto& t = m_world->registry().get<TransformLocal>(ent);
            const float bob = std::sin(alertTime * 4.0f + static_cast<float>(entt::to_integral(bId)) * 0.7f) * 0.08f;
            t.translation = glm::vec3(
                static_cast<float>(pos.x) * kTileSize + 0.5f * kTileSize,
                1.25f + bob,
                static_cast<float>(pos.z) * kTileSize + 0.5f * kTileSize
            );
            t.rotation = glm::angleAxis(alertTime * 2.5f, glm::vec3(0.0f, 1.0f, 0.0f));
            ++it;
        } else {
            if(m_world->registry().valid(ent)) m_world->registry().destroy(ent);
            it = m_alertEntities.erase(it);
        }
    }

    if (m_world->registry().valid(m_hoverTileEntity)) {
        auto& t = m_world->registry().get<TransformLocal>(m_hoverTileEntity);
        if (m_hasHoverTile) {
            t.translation = glm::vec3(
                static_cast<float>(m_hoverX) * kTileSize + 0.5f * kTileSize,
                0.015f,
                static_cast<float>(m_hoverZ) * kTileSize + 0.5f * kTileSize
            );
            t.scale = glm::vec3(1.0f);
        } else {
            t.scale = glm::vec3(0.0f);
        }
    }

    if (m_world->registry().valid(m_ghostEntity)) {
        auto& t = m_world->registry().get<TransformLocal>(m_ghostEntity);
        auto& mc = m_world->registry().get<MeshComponent>(m_ghostEntity);

        if (m_selectedBuildType != BuildingIds::None && m_hasHoverTile) {
            t.translation = glm::vec3(
                static_cast<float>(m_hoverX) * kTileSize + 0.5f * kTileSize,
                0.0f,
                static_cast<float>(m_hoverZ) * kTileSize + 0.5f * kTileSize
            );
            t.scale = glm::vec3(1.0f);
            
            auto& state = m_world->resource<CityState>();
            mc.mesh = m_cityMeshes.getBuildingMesh(m_selectedBuildType, state.currentEra);

            if (validate(*m_world, PlaceBuildingCmd{m_hoverX, m_hoverZ, m_selectedBuildType}).ok()) {
                mc.tint = glm::vec3(0.3f, 1.0f, 0.4f);
                mc.emissiveIntensity = 0.8f;
            } else {
                mc.tint = glm::vec3(1.0f, 0.2f, 0.2f);
                mc.emissiveIntensity = 0.8f;
            }
        } else {
            t.scale = glm::vec3(0.0f);
        }
    }
}

void EraSandboxModule::renderUi(const World& world, CommandQueue& commands) {
    const auto fbSize = world.resource<PlatformGLFW>().framebufferSize();
    const float winWidth = static_cast<float>(std::max(1, fbSize.x));
    const float winHeight = static_cast<float>(std::max(1, fbSize.y));

    ui::drawTopHud(world, commands);
    ui::drawEvolutionBanner(world, commands, winWidth);
    ui::drawBuildDock(world, commands, winWidth, winHeight, m_selectedBuildType, m_demolishMode, m_inspectedBuildingId);
    ui::drawInspector(world, commands, winWidth, m_inspectedBuildingId);

    // Quick debug hot-reload trigger in UI
    ImGui::SetNextWindowPos(ImVec2(winWidth - 110.0f, winHeight - 55.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);
    if (ImGui::Begin("DebugDataReload", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
        if (ImGui::Button("Reload (F5)")) {
            commands.enqueue(ReloadDataCmd{});
        }
    }
    ImGui::End();

    ui::drawHoverTooltip(world, m_hasHoverTile, m_hoverX, m_hoverZ, m_selectedBuildType, m_demolishMode, m_inspectedBuildingId);
}

bool EraSandboxModule::renderFrame(World& world) {
    const glm::ivec2 currentSize = world.resource<PlatformGLFW>().framebufferSize();
    if (currentSize.x <= 0 || currentSize.y <= 0) {
        return true;
    }

    uint32_t imageIndex = 0;
    const VkResult acquired = world.resource<VulkanContext>().acquireNextImage(&imageIndex);
    if (acquired == VK_ERROR_OUT_OF_DATE_KHR) {
        world.resource<VulkanContext>().handleResize(WindowResizeEvent{currentSize.x, currentSize.y});
        ImGui_ImplVulkan_SetMinImageCount(
            static_cast<uint32_t>(std::max(2, static_cast<int>(world.resource<VulkanContext>().framebuffers().size()))));
        return true;
    }
    if (acquired != VK_SUCCESS && acquired != VK_SUBOPTIMAL_KHR) {
        return false;
    }

    VkCommandBuffer cmd = world.resource<VulkanContext>().commandBuffer(world.resource<VulkanContext>().currentFrame());
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);

    if (m_enableShadows) {
        world.resource<PbrRenderer>().recordShadowPass(cmd, world.registry(), world.resource<GpuMeshCache>());
    }

    // 1. HDR Render Pass
    std::array<VkClearValue, 2> hdrClearValues{};
    hdrClearValues[0].color = {{0.09f, 0.12f, 0.16f, 1.f}};
    hdrClearValues[1].depthStencil = {1.f, 0};

    VkRenderPassBeginInfo hdrPassInfo{};
    hdrPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    hdrPassInfo.renderPass = world.resource<VulkanContext>().hdrRenderPass();
    hdrPassInfo.framebuffer = world.resource<VulkanContext>().hdrFramebuffer();
    hdrPassInfo.renderArea.extent = world.resource<VulkanContext>().swapchainExtent();
    hdrPassInfo.clearValueCount = static_cast<uint32_t>(hdrClearValues.size());
    hdrPassInfo.pClearValues = hdrClearValues.data();

    vkCmdBeginRenderPass(cmd, &hdrPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    const float aspect =
        static_cast<float>(world.resource<VulkanContext>().swapchainExtent().width) /
        static_cast<float>(std::max(1u, world.resource<VulkanContext>().swapchainExtent().height));
    const CameraState camera = m_camera.getCameraState(aspect);

    world.resource<PbrRenderer>().recordScene(cmd, world.registry(), world.resource<GpuMeshCache>(), world.resource<GpuTextureCache>(), camera);
    world.resource<ParticleSystem>().record(cmd, camera);

    vkCmdEndRenderPass(cmd);

    // 2. Post process bloom
    world.resource<PostProcessPipeline>().recordBloom(cmd);

    // 3. Swapchain Presentation Pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = world.resource<VulkanContext>().renderPass();
    renderPassInfo.framebuffer = world.resource<VulkanContext>().framebuffers()[imageIndex];
    renderPassInfo.renderArea.extent = world.resource<VulkanContext>().swapchainExtent();
    renderPassInfo.clearValueCount = 0;
    renderPassInfo.pClearValues = nullptr;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    world.resource<PostProcessPipeline>().recordComposite(cmd);

    world.resource<ImGuiLayer>().beginFrame();
    renderUi(world, world.commandQueue());
    engine::ui::drawEditorToolbar(m_editorHistory, world.registry(), world.types(), m_gizmo, m_sceneControls);
    handleSceneRequests(world, m_editorHistory, m_sceneControls, m_sunDirection);
    engine::ui::drawComponentInspector(world.registry(), world.types(), m_inspectedBuildingId, &m_editorHistory);
    {
        const glm::ivec2 fb = world.resource<PlatformGLFW>().framebufferSize();
        const bool allowPick = !ImGui::GetIO().WantCaptureMouse;
        tickEditorGizmo(m_gizmo, m_editorHistory, m_gizmoWasDragging, m_gizmoStartTranslation,
                        m_gizmoStartRotation, world.registry(), m_inspectedBuildingId, camera,
                        world.resource<Input>(),
                        glm::vec2{static_cast<float>(std::max(1, fb.x)), static_cast<float>(std::max(1, fb.y))},
                        allowPick);
    }
    world.resource<ImGuiLayer>().endFrame(cmd);

    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);

    const VkResult presentResult = world.resource<VulkanContext>().submitAndPresent(imageIndex);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        const auto size = world.resource<PlatformGLFW>().framebufferSize();
        if (size.x > 0 && size.y > 0) {
            world.resource<VulkanContext>().handleResize(WindowResizeEvent{size.x, size.y});
            ImGui_ImplVulkan_SetMinImageCount(
                static_cast<uint32_t>(std::max(2, static_cast<int>(world.resource<VulkanContext>().framebuffers().size()))));
        }
    }

    return true;
}


} // namespace engine::era
