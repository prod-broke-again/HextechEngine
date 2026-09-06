#pragma once

#include "engine/ecs/Components.hpp"

#include <array>
#include <entt/entt.hpp>
#include <vulkan/vulkan.h>

namespace engine {

class VulkanContext;

class DebugDraw {
public:
    bool init(VulkanContext& ctx);
    void shutdown();

    void record(VkCommandBuffer cmd, entt::registry& registry, const CameraState& camera);

private:
    bool createPipeline();

    struct FrameBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        void* allocation = nullptr;
        VkDeviceSize capacity = 0;
    };

    VulkanContext* m_ctx = nullptr;
    VkShaderModule m_vertModule = VK_NULL_HANDLE;
    VkShaderModule m_fragModule = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
    std::array<FrameBuffer, 2> m_frameBuffers{};
};

} // namespace engine
