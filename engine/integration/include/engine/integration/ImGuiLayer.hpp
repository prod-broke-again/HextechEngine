#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace engine {

class VulkanContext;

class ImGuiLayer {
public:
    bool init(VulkanContext& ctx, GLFWwindow* window);
    void shutdown();

    void beginFrame();
    void endFrame(VkCommandBuffer cmd);

private:
    VulkanContext* m_ctx = nullptr;
    GLFWwindow* m_window = nullptr;
};

} // namespace engine
