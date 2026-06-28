#pragma once

#include "engine/ecs/Components.hpp"

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

    VulkanContext* m_ctx = nullptr;
    VkShaderModule m_vertModule = VK_NULL_HANDLE;
    VkShaderModule m_fragModule = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_layout = VK_NULL_HANDLE;
};

} // namespace engine
