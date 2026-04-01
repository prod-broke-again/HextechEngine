#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace engine {

class VulkanContext;

class PbrRenderer {
public:
    bool init(VulkanContext& ctx);
    void shutdown();

    void recordMainPass(VkCommandBuffer cmd, uint32_t swapchainImageIndex, float r, float g, float b);

    [[nodiscard]] bool iblReady() const { return m_iblReady; }

private:
    bool createPipelineLayout();
    bool loadShaderModules();
    bool createGraphicsPipeline();
    void destroyPipeline();

    VulkanContext* m_ctx = nullptr;
    VkShaderModule m_vertModule = VK_NULL_HANDLE;
    VkShaderModule m_fragModule = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    bool m_iblReady = false;
};

} // namespace engine
