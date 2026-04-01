#include "engine/integration/ImGuiLayer.hpp"

#include "engine/core/Log.hpp"
#include "engine/renderer/vulkan/VulkanContext.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>

namespace engine {

bool ImGuiLayer::init(VulkanContext& ctx, GLFWwindow* window) {
    m_ctx = &ctx;
    m_window = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = ctx.instance();
    initInfo.PhysicalDevice = ctx.physicalDevice();
    initInfo.Device = ctx.device();
    initInfo.QueueFamily = ctx.graphicsFamily();
    initInfo.Queue = ctx.graphicsQueue();
    initInfo.PipelineCache = VK_NULL_HANDLE;
    initInfo.DescriptorPool = ctx.imguiDescriptorPool();
    initInfo.RenderPass = ctx.renderPass();
    initInfo.Subpass = 0;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount =
        std::max(2u, static_cast<uint32_t>(ctx.framebuffers().size()));
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.UseDynamicRendering = false;

    if (!ImGui_ImplVulkan_Init(&initInfo)) {
        log(LogLevel::Error, "ImGui_ImplVulkan_Init failed");
        return false;
    }

    ImGui_ImplVulkan_CreateFontsTexture();
    return true;
}

void ImGuiLayer::shutdown() {
    if (m_ctx) {
        ImGui_ImplVulkan_Shutdown();
    }
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    m_ctx = nullptr;
    m_window = nullptr;
}

void ImGuiLayer::beginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiLayer::endFrame(VkCommandBuffer cmd) {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd, nullptr);
}

} // namespace engine
