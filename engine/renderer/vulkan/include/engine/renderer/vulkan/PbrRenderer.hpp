#pragma once

#include "engine/ecs/Components.hpp"
#include "engine/renderer/vulkan/GpuMeshCache.hpp"
#include "engine/renderer/vulkan/GpuTextureCache.hpp"

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>
#include <vulkan/vulkan.h>

namespace engine {

class VulkanContext;

class PbrRenderer {
public:
    bool init(VulkanContext& ctx);
    void shutdown();

    bool initTextureCache(GpuTextureCache& textures);

    void tryReloadShaders();
    void recordScene(VkCommandBuffer cmd, entt::registry& registry, GpuMeshCache& meshes,
                     GpuTextureCache& textures, const CameraState& camera);

    [[nodiscard]] bool ready() const { return m_ready; }

private:
    struct DrawPushConstants {
        glm::mat4 mvp{1.f};
        glm::vec4 tint{1.f};
        glm::vec4 lightDir{-0.35f, -1.f, -0.25f, 0.f};
        glm::vec4 cameraPos{0.f};
        glm::vec4 material{0.f, 0.5f, 0.f, 0.f};
    };

    bool loadShaderModules();
    bool createDescriptorResources();
    bool createPipelineLayout();
    bool createGraphicsPipeline();
    void destroyPipeline();
    void destroyDescriptorResources();
    bool compileShaderSources();

    VulkanContext* m_ctx = nullptr;
    VkShaderModule m_vertModule = VK_NULL_HANDLE;
    VkShaderModule m_fragModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    bool m_ready = false;
};

} // namespace engine
