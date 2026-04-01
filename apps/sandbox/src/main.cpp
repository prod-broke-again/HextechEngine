#include "engine/assets/AssetManager.hpp"
#include "engine/core/Config.hpp"
#include "engine/core/Events.hpp"
#include "engine/core/Log.hpp"
#include "engine/ecs/Components.hpp"
#include "engine/integration/ImGuiLayer.hpp"
#include "engine/integration/PlatformGLFW.hpp"
#include "engine/physics/JoltWorld.hpp"
#include "engine/renderer/vulkan/PbrRenderer.hpp"
#include "engine/renderer/vulkan/ShaderHotReload.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"

#include <GLFW/glfw3.h>
#include <entt/entt.hpp>
#include <entt/signal/dispatcher.hpp>
#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>

namespace {

std::string shaderPath(const char* name) {
    return std::string(SHADER_DIR) + "/" + name;
}

} // namespace

int main() {
    engine::Config cfg;
    const std::filesystem::path cfgPath = "engine_config.json";
    if (std::filesystem::exists(cfgPath)) {
        cfg.loadFromFile(cfgPath);
    }

    engine::PlatformGLFW platform;
    engine::Input input;
    entt::dispatcher dispatcher;
    entt::registry registry;

    if (!platform.init(1280, 720, "untitled_engine sandbox", &input, &dispatcher)) {
        return 1;
    }

    engine::VulkanContext vulkan;
    if (!vulkan.init(platform.window(), true)) {
        return 1;
    }

    dispatcher.sink<engine::WindowResizeEvent>().connect(
        [&vulkan](const engine::WindowResizeEvent& e) { vulkan.handleResize(e); });

    engine::ImGuiLayer imgui;
    if (!imgui.init(vulkan, platform.window())) {
        return 1;
    }

    engine::PbrRenderer pbr;
    pbr.init(vulkan);

    engine::JoltWorld physics;
    engine::AssetManager assets;

    const auto demoEntity = registry.create();
    registry.emplace<engine::TransformLocal>(demoEntity);
    registry.emplace<engine::TransformWorld>(demoEntity);
    registry.emplace<engine::CameraComponent>(demoEntity);

    glm::vec3 camPos{0.f, 2.f, 6.f};
    float yaw = -1.57f;
    float pitch = -0.2f;

    engine::shaderHotReloadWatchPath(shaderPath("pbr.frag").c_str());
    engine::shaderHotReloadWatchPath(shaderPath("pbr.vert").c_str());

    auto last = std::chrono::steady_clock::now();

    while (!platform.shouldClose()) {
        input.beginFrame();
        platform.pollEvents();

        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - last).count();
        last = now;

        physics.step(dt);

        if (input.keyDown(GLFW_KEY_W)) {
            camPos += glm::normalize(glm::vec3{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                                                 std::sin(yaw) * std::cos(pitch)}) *
                      (dt * 8.f);
        }
        if (input.keyDown(GLFW_KEY_S)) {
            camPos -= glm::normalize(glm::vec3{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                                                 std::sin(yaw) * std::cos(pitch)}) *
                      (dt * 8.f);
        }
        if (input.keyDown(GLFW_KEY_D)) {
            camPos += glm::normalize(glm::cross(
                glm::vec3{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                          std::sin(yaw) * std::cos(pitch)},
                glm::vec3(0.f, 1.f, 0.f))) *
                      (dt * 8.f);
        }
        if (input.keyDown(GLFW_KEY_A)) {
            camPos -= glm::normalize(glm::cross(
                glm::vec3{std::cos(yaw) * std::cos(pitch), std::sin(pitch),
                          std::sin(yaw) * std::cos(pitch)},
                glm::vec3(0.f, 1.f, 0.f))) *
                      (dt * 8.f);
        }
        if (input.mouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT)) {
            const auto& snap = input.snapshot();
            yaw += snap.mouseDelta.x * 0.005f;
            pitch -= snap.mouseDelta.y * 0.005f;
            pitch = std::clamp(pitch, -1.4f, 1.4f);
        }

        if (engine::shaderHotReloadPollChanged(shaderPath("pbr.frag").c_str()) ||
            engine::shaderHotReloadPollChanged(shaderPath("pbr.vert").c_str())) {
            engine::log(engine::LogLevel::Info, "Shader file changed (restart pipeline in dev)");
        }

        uint32_t imageIndex = 0;
        const VkResult acq = vulkan.acquireNextImage(&imageIndex);
        if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
            const auto sz = platform.framebufferSize();
            vulkan.handleResize(engine::WindowResizeEvent{sz.x, sz.y});
            ImGui_ImplVulkan_SetMinImageCount(
                static_cast<uint32_t>(std::max(2, static_cast<int>(vulkan.framebuffers().size()))));
            continue;
        }
        if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) {
            break;
        }

        VkCommandBuffer cmd = vulkan.commandBuffer(vulkan.currentFrame());
        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(cmd, &beginInfo);

        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color = {{0.06f, 0.07f, 0.09f, 1.0f}};
        clearValues[1].depthStencil = {1.0f, 0};

        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = vulkan.renderPass();
        rpInfo.framebuffer = vulkan.framebuffers()[imageIndex];
        rpInfo.renderArea.extent = vulkan.swapchainExtent();
        rpInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        rpInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        pbr.recordMainPass(cmd, imageIndex, 0.1f, 0.2f, 0.3f);

        imgui.beginFrame();
        ImGui::Begin("Sandbox");
        ImGui::Text("FPS: %.1f", dt > 0.f ? 1.f / dt : 0.f);
        ImGui::Text("Camera: %.2f %.2f %.2f", camPos.x, camPos.y, camPos.z);
        ImGui::Text("ECS entities: %zu", registry.size());
        ImGui::End();
        imgui.endFrame(cmd);

        vkCmdEndRenderPass(cmd);
        vkEndCommandBuffer(cmd);

        const VkResult res = vulkan.submitAndPresent(imageIndex);
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
            const auto sz = platform.framebufferSize();
            vulkan.handleResize(engine::WindowResizeEvent{sz.x, sz.y});
            ImGui_ImplVulkan_SetMinImageCount(
                static_cast<uint32_t>(std::max(2, static_cast<int>(vulkan.framebuffers().size()))));
        }

        input.endFrame();
    }

    vkDeviceWaitIdle(vulkan.device());
    pbr.shutdown();
    imgui.shutdown();
    vulkan.shutdown();
    platform.shutdown();
    return 0;
}
