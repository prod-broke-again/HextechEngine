#include "EraSandboxModule.hpp"

#include "engine/assets/MeshBuilder.hpp"
#include "engine/core/Log.hpp"
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

void EraSandboxModule::onAttach(World& world) {
    m_world = &world;
    m_world->resource<PlatformGLFW>().setCursorCaptured(false);
    
    // Bind events
    world.events().connect<BuildingPlacedEvent, &EraSandboxModule::spawnVisualBuilding>(this);
    world.events().connect<BuildingRemovedEvent, &EraSandboxModule::removeVisualBuilding>(this);
    world.events().connect<CarrierSpawnedEvent, &EraSandboxModule::spawnVisualCarrier>(this);
    world.events().connect<BuildingStatusEvent, &EraSandboxModule::spawnAlertIndicator>(this);
    world.events().connect<EraEvolvedEvent, &EraSandboxModule::onEraEvolved>(this);

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
    updateFrame(world, 1.0f / 30.0f);
}

void EraSandboxModule::render(World& world, float alpha) {
    if (world.resource<PlatformGLFW>().shouldClose()) return;
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

    if (ev.type == BuildingType::TownCenter) {
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
    ghostComp.mesh = m_cityMeshes.getBuildingMesh(BuildingType::Residence, world.resource<CityState>().currentEra);
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
                if (CitySystems::demolishBuilding(*m_world, m_hoverX, m_hoverZ)) {
                    // Check if we demolished the inspected building
                    auto& grid = m_world->resource<GridIndex>();
                    if (grid.cells[m_hoverZ][m_hoverX].entity == m_inspectedBuildingId) {
                        m_inspectedBuildingId = entt::null;
                    }
                }
            } else if (m_selectedBuildType != BuildingType::None) {
                if (CitySystems::placeBuilding(*m_world, m_hoverX, m_hoverZ, m_selectedBuildType)) {
                    auto& grid = m_world->resource<GridIndex>();
                    m_inspectedBuildingId = grid.cells[m_hoverZ][m_hoverX].entity;
                    if (!m_world->resource<Input>().keyDown(GLFW_KEY_LEFT_SHIFT)) {
                        m_selectedBuildType = BuildingType::None;
                    }
                }
            } else {
                auto& grid = m_world->resource<GridIndex>();
                entt::entity e = grid.cells[m_hoverZ][m_hoverX].entity;
                m_inspectedBuildingId = (e != entt::null && m_world->registry().valid(e)) ? e : entt::null;
            }
        }
    }

    if (m_world->resource<Input>().mouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT) || m_world->resource<Input>().keyPressed(GLFW_KEY_ESCAPE)) {
        if (m_selectedBuildType != BuildingType::None || m_demolishMode) {
            m_selectedBuildType = BuildingType::None;
            m_demolishMode = false;
        }
    }

    CitySystems::update(*m_world, deltaTime);
    m_world->events().drain(); // Apply all events
    m_world->resource<ParticleSystem>().update(deltaTime);

    auto cView = m_world->registry().view<CarrierComponent, CarrierJourneyComponent, TransformLocal>();
    glm::vec3 tcPos = CitySystems::getTownCenterPosition(*m_world);
    for (auto [e, c, j, t] : cView.each()) {
        if (c.state == CarrierState::IdleAtWarehouse) {
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

        if (m_selectedBuildType != BuildingType::None && m_hasHoverTile) {
            t.translation = glm::vec3(
                static_cast<float>(m_hoverX) * kTileSize + 0.5f * kTileSize,
                0.0f,
                static_cast<float>(m_hoverZ) * kTileSize + 0.5f * kTileSize
            );
            t.scale = glm::vec3(1.0f);
            
            auto& state = m_world->resource<CityState>();
            mc.mesh = m_cityMeshes.getBuildingMesh(m_selectedBuildType, state.currentEra);

            if (CitySystems::canPlace(*m_world, m_hoverX, m_hoverZ, m_selectedBuildType)) {
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

void EraSandboxModule::renderUi(World& world) {
    const float winWidth = static_cast<float>(m_world->resource<PlatformGLFW>().framebufferSize().x);
    const float winHeight = static_cast<float>(m_world->resource<PlatformGLFW>().framebufferSize().y);

    auto& state = m_world->resource<CityState>();

    // 1. Top HUD (Global Resources & Era)
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.85f);
    if (ImGui::Begin("City Status HUD", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings)) {
        
        ImGui::TextColored(ImVec4(1.0f, 0.82f, 0.2f, 1.0f), "[%s]", getEraName(state.currentEra).data());
        ImGui::SameLine();
        ImGui::Text("  |  ");
        ImGui::SameLine();
        ImGui::Text("Settlers: %d/%d", state.totalPopulation, state.maxPopulation);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "(%.0f%% happy)", state.averageSatisfaction * 100.0f);
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float goldRate = state.taxIncomePerMinute;
        ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "Gold: %.0f  (%.1f / min)", 
                           state.storage.get(ResourceType::Gold), goldRate);

        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        
        const float woodRate = state.productionRatesPerMin[static_cast<size_t>(ResourceType::Wood)] - state.consumptionRatesPerMin[static_cast<size_t>(ResourceType::Wood)];
        ImGui::TextColored(woodRate >= 0 ? ImVec4(0.6f, 0.8f, 0.4f, 1.0f) : ImVec4(0.9f, 0.4f, 0.4f, 1.0f), 
                           "(%.1f / min)", woodRate);
        ImGui::SameLine();
        ImGui::Text("Wood: %.1f", state.storage.get(ResourceType::Wood));
        
        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        
        const float fishRate = state.productionRatesPerMin[static_cast<size_t>(ResourceType::Fish)] - state.consumptionRatesPerMin[static_cast<size_t>(ResourceType::Fish)];
        ImGui::TextColored(fishRate >= 0 ? ImVec4(0.6f, 0.8f, 0.4f, 1.0f) : ImVec4(0.9f, 0.4f, 0.4f, 1.0f), 
                           "(%.1f / min)", fishRate);
        ImGui::SameLine();
        ImGui::Text("Fish: %.1f", state.storage.get(ResourceType::Fish));

        ImGui::SameLine(); ImGui::Text("  |  "); ImGui::SameLine();
        
        const float stoneRate = state.productionRatesPerMin[static_cast<size_t>(ResourceType::Stone)] - state.consumptionRatesPerMin[static_cast<size_t>(ResourceType::Stone)];
        ImGui::TextColored(stoneRate >= 0 ? ImVec4(0.6f, 0.8f, 0.4f, 1.0f) : ImVec4(0.9f, 0.4f, 0.4f, 1.0f), 
                           "(%.1f / min)", stoneRate);
        ImGui::SameLine();
        ImGui::Text("Stone: %.1f", state.storage.get(ResourceType::Stone));

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        auto& cPool = m_world->resource<CarrierPool>();
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Global Warehouse Couriers: %d / %d active", 
                           cPool.activeCarrierCount, cPool.totalCarrierCount);

    }
    ImGui::End();

    // 2. Era Evolution Banner
    if (CitySystems::canEvolve(*m_world)) {
        ImGui::SetNextWindowPos(ImVec2(winWidth / 2.0f, 80.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
        ImGui::SetNextWindowBgAlpha(0.9f);
        if (ImGui::Begin("Era Evolution", nullptr, 
                         ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize)) {
            
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "★★★ Your city is ready to evolve! ★★★");
            if (ImGui::Button("Evolve to Next Era!", ImVec2(280, 40))) {
                CitySystems::evolveToNextEra(*m_world);
            }
        }
        ImGui::End();
    }

    // 3. Build Dock (Bottom HUD)
    ImGui::SetNextWindowPos(ImVec2(winWidth / 2.0f, winHeight - 10.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.85f);
    if (ImGui::Begin("Build Dock", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
        
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "CONSTRUCTION DOCK");
        ImGui::Separator();
        ImGui::Spacing();

        const auto buildings = getAvailableBuildingsForEra(state.currentEra);
        
        for (size_t i = 0; i < buildings.size(); ++i) {
            const BuildingType bType = buildings[i];
            const BuildingDef& def = getBuildingDef(bType);

            const bool canAfford = CitySystems::canAfford(*m_world, bType);
            
            if (!canAfford) {
                ImGui::BeginDisabled();
            }
            
            if (m_selectedBuildType == bType) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.7f, 0.3f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.35f, 1.0f));
            }

            if (ImGui::Button(def.name.data(), ImVec2(100, 40))) {
                m_selectedBuildType = bType;
                m_demolishMode = false;
                m_inspectedBuildingId = entt::null;
            }
            
            ImGui::PopStyleColor(2);

            if (!canAfford) {
                ImGui::EndDisabled();
            }

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(def.name.data());
                ImGui::Separator();
                ImGui::Text("Cost:");
                for (size_t resIdx = 0; resIdx < kResourceCount; ++resIdx) {
                    float amount = def.cost.amounts[resIdx];
                    if (amount > 0.0f) {
                        ImGui::Text(" - %.0f %s", amount, getResourceName(static_cast<ResourceType>(resIdx)).data());
                    }
                }
                ImGui::Spacing();
                ImGui::Text("%s", def.description.data());
                ImGui::EndTooltip();
            }

            if (i < buildings.size() - 1) {
                ImGui::SameLine();
            }
        }

        ImGui::SameLine(0, 30.0f); // Gap before demolish button

        if (m_demolishMode) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.2f, 0.2f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.15f, 0.15f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.5f, 0.2f, 0.2f, 1.0f));
        }

        if (ImGui::Button("Demolish", ImVec2(80, 40))) {
            m_demolishMode = !m_demolishMode;
            if (m_demolishMode) {
                m_selectedBuildType = BuildingType::None;
                m_inspectedBuildingId = entt::null;
            }
        }
        ImGui::PopStyleColor(2);

    }
    ImGui::End();

    // 4. Inspector Panel (Right HUD)
    if (m_world->registry().valid(m_inspectedBuildingId)) {
        auto& b = m_world->registry().get<BuildingComponent>(m_inspectedBuildingId);
        auto& pos = m_world->registry().get<GridPosition>(m_inspectedBuildingId);
        
        ImGui::SetNextWindowPos(ImVec2(winWidth - 10.0f, 10.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.9f);

        if (ImGui::Begin("Inspector", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize)) {
            
            const BuildingDef& def = getBuildingDef(b.type);
            
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "%s", def.name.data());
            ImGui::Text("Position: [%d, %d]", pos.x, pos.z);
            ImGui::Separator();
            ImGui::Spacing();

            if (b.type == BuildingType::TownCenter) {
                ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "TOWN CENTER HUB");
                ImGui::Spacing();
                
                ImGui::Text("Current Era: %s", getEraName(state.currentEra).data());
                ImGui::Spacing();
                
                if (state.currentEra == EraType::StoneAge) {
                    const EraDefinition& eraDef = getEraDefinition(EraType::BronzeAge);
                    
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "Evolution Requirements:");
                    
                    const bool popOk = (state.totalPopulation >= eraDef.requiredPopulation);
                    ImGui::TextColored(popOk ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 
                                       "[%s] Population: %d / %d", 
                                       popOk ? "x" : " ", state.totalPopulation, eraDef.requiredPopulation);
                    
                    for (const auto& req : eraDef.evolutionRequirements) {
                        const float current = state.storage.get(req.resource);
                        const bool resOk = current >= static_cast<float>(req.requiredAmount);
                        ImGui::TextColored(resOk ? ImVec4(0.2f, 1.0f, 0.2f, 1.0f) : ImVec4(1.0f, 0.2f, 0.2f, 1.0f), 
                                           "[%s] %s: %.0f / %.0f", 
                                           resOk ? "x" : " ", getResourceName(req.resource).data(), current, req.requiredAmount);
                    }
                    
                    ImGui::Spacing();
                    const bool ready = CitySystems::canEvolve(*m_world);
                    if (!ready) ImGui::BeginDisabled();
                    if (ImGui::Button("Evolve to Bronze Age!", ImVec2(-1.0f, 40.0f))) {
                        CitySystems::evolveToNextEra(*m_world);
                    }
                    if (!ready) ImGui::EndDisabled();
                    
                    ImGui::Spacing();
                    ImGui::Separator();
                    ImGui::Spacing();
                }

                auto& cPool = m_world->resource<CarrierPool>();
                ImGui::Text("Warehouse Fleet: %d / %d busy", cPool.activeCarrierCount, cPool.totalCarrierCount);
                
                if (ImGui::TreeNodeEx("Active Couriers", ImGuiTreeNodeFlags_DefaultOpen)) {
                    auto view = m_world->registry().view<CarrierComponent>();
                    for (auto e : view) {
                        auto& c = view.get<CarrierComponent>(e);
                        if (c.state != CarrierState::IdleAtWarehouse) {
                            ImGui::BulletText("Courier fetching %s", getResourceName(c.carriedResource).data());
                        }
                    }
                    ImGui::TreePop();
                }

            } else if (b.type == BuildingType::Residence) {
                if (auto* res = m_world->registry().try_get<ResidenceComponent>(m_inspectedBuildingId)) {
                    ImGui::Text("Inhabitants: %d / %d", res->currentInhabitants, res->maxInhabitants);
                    ImGui::Spacing();
                    
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Needs Fulfillment:");
                    
                    ImGui::Text("Food (Fish): %.0f%%", res->foodSatisfaction * 100.0f);
                    ImGui::ProgressBar(res->foodSatisfaction, ImVec2(-1.0f, 12.0f), "");
                    
                    ImGui::Spacing();
                    
                    ImGui::Text("Warmth (Firewood): %.0f%%", res->warmthSatisfaction * 100.0f);
                    ImGui::ProgressBar(res->warmthSatisfaction, ImVec2(-1.0f, 12.0f), "");

                    ImGui::Spacing();
                    const float currentTax = def.baseTaxIncomePerMinute * res->overallSatisfaction * 
                                             (static_cast<float>(res->currentInhabitants) / static_cast<float>(res->maxInhabitants));
                    ImGui::TextColored(ImVec4(1.0f, 0.9f, 0.4f, 1.0f), "Generating %.1f gold/min", currentTax);
                }

            } else if (def.production.outputPerMinute > 0.0f) {
                if (auto* prod = m_world->registry().try_get<ProductionComponent>(m_inspectedBuildingId)) {
                    ImGui::Text("Production: %s", getResourceName(def.production.outputResource).data());
                    
                    const float buf = prod->internalBuffer;
                    const float maxBuf = prod->maxBuffer;
                    ImGui::Text("Output Buffer: %.1f / %.1f", buf, maxBuf);
                    ImGui::ProgressBar(buf / maxBuf, ImVec2(-1.0f, 12.0f), "");
                    
                    if (prod->isBufferFull) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.1f, 1.0f), "WARNING: Buffer Full! Waiting for pickup.");
                    } else if (prod->hasCourierAssigned) {
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Courier is en-route for pickup.");
                    } else {
                        ImGui::Text("Producing normally.");
                    }

                    if (prod->isWorking) {
                        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[WORKING]");
                        const float progressFrac = prod->progress / std::max(0.1f, def.production.cycleSeconds);
                        ImGui::ProgressBar(progressFrac, ImVec2(-1.0f, 8.0f), "Cycle");
                    } else if (prod->isBufferFull) {
                        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.1f, 1.0f), "[HALTED - STORAGE FULL]");
                    } else {
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "[HALTED - MISSING INPUTS]");
                    }
                }
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (b.type != BuildingType::TownCenter) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
                if (ImGui::Button("Demolish Building", ImVec2(-1.0f, 30.0f))) {
                    CitySystems::demolishBuilding(*m_world, pos.x, pos.z);
                    m_inspectedBuildingId = entt::null;
                }
                ImGui::PopStyleColor();
            }
        }
        ImGui::End();
    } else {
        m_inspectedBuildingId = entt::null;
    }

    // 5. Tooltips for hover
    if (!ImGui::GetIO().WantCaptureMouse && m_hasHoverTile && !m_demolishMode && m_selectedBuildType == BuildingType::None) {
        if (!m_world->registry().valid(m_inspectedBuildingId)) {
            auto& grid = m_world->resource<GridIndex>();
            entt::entity hoveredId = grid.cells[m_hoverZ][m_hoverX].entity;
            if (m_world->registry().valid(hoveredId)) {
                ImGui::BeginTooltip();
                auto& hb = m_world->registry().get<BuildingComponent>(hoveredId);
                const BuildingDef& hDef = getBuildingDef(hb.type);
                ImGui::TextUnformatted(hDef.name.data());
                ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Click to inspect");
                ImGui::EndTooltip();
            }
        }
    }
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
    renderUi(world);
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
