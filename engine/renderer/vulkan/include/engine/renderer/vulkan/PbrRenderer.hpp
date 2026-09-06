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
    void recordShadowPass(VkCommandBuffer cmd, entt::registry& registry, GpuMeshCache& meshes);
    void recordScene(VkCommandBuffer cmd, entt::registry& registry, GpuMeshCache& meshes,
                     GpuTextureCache& textures, const CameraState& camera);

    [[nodiscard]] bool ready() const { return m_ready; }

    void setCullMode(VkCullModeFlags cullMode);
    [[nodiscard]] VkCullModeFlags cullMode() const { return m_cullMode; }

    void setLightDir(const glm::vec3& dir) { m_lightDir = dir; }
    [[nodiscard]] glm::vec3 lightDir() const { return m_lightDir; }

private:
    struct DrawPushConstants {
        glm::mat4 mvp{1.f};
        glm::mat4 model{1.f};
        glm::vec4 tint{1.f};
        glm::vec4 material{0.f, 0.5f, 0.f, 0.f};
    };

    struct GpuPointLight {
        glm::vec4 positionRadius{0.f};
        glm::vec4 colorIntensity{0.f};
    };

    struct LightUboData {
        glm::mat4 lightSpaceMatrix{1.f};
        glm::vec4 cameraPos{0.f};
        glm::vec4 sunDir{0.f};
        glm::vec4 lightParams{0.f};
        GpuPointLight pointLights[16]{};
    };

    struct ShadowPushConstants {
        glm::mat4 lightMvp{1.f};
    };

    struct SkyPushConstants {
        glm::mat4 invViewProj{1.f};
        glm::vec4 sunDir{0.f};
    };

    bool loadShaderModules();
    bool createDescriptorResources();
    bool createPipelineLayout();
    bool createGraphicsPipeline();
    void destroyPipeline();
    void destroyDescriptorResources();
    bool compileShaderSources();

    bool createLightUboResources();
    void destroyLightUboResources();

    bool createShadowResources();
    void destroyShadowResources();
    bool createShadowPipeline();
    void destroyShadowPipeline();

    bool createSkyPipeline();
    void destroySkyPipeline();

    void recordSkyPass(VkCommandBuffer cmd, const CameraState& camera);

    [[nodiscard]] glm::mat4 computeLightViewProj(const glm::vec3& lightDir) const;
    [[nodiscard]] glm::mat4 computeLightSpaceMatrix(const glm::mat4& lightViewProj) const;

    static constexpr uint32_t kShadowMapResolution = 2048;

    VulkanContext* m_ctx = nullptr;
    VkShaderModule m_vertModule = VK_NULL_HANDLE;
    VkShaderModule m_fragModule = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkSampler m_sampler = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
    VkCullModeFlags m_cullMode = VK_CULL_MODE_NONE;

    // Shadow mapping
    VkImage m_shadowImage = VK_NULL_HANDLE;
    void* m_shadowAllocation = nullptr;
    VkImageView m_shadowImageView = VK_NULL_HANDLE;
    VkSampler m_shadowSampler = VK_NULL_HANDLE;
    VkRenderPass m_shadowRenderPass = VK_NULL_HANDLE;
    VkFramebuffer m_shadowFramebuffer = VK_NULL_HANDLE;
    VkShaderModule m_shadowVertModule = VK_NULL_HANDLE;
    VkPipelineLayout m_shadowPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_shadowPipeline = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_shadowDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_shadowDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_shadowDescriptorSet = VK_NULL_HANDLE;

    // Procedural sky
    VkShaderModule m_skyVertModule = VK_NULL_HANDLE;
    VkShaderModule m_skyFragModule = VK_NULL_HANDLE;
    VkPipelineLayout m_skyPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_skyPipeline = VK_NULL_HANDLE;

    // Point lights UBO (Set 2)
    VkBuffer m_lightUboBuffer = VK_NULL_HANDLE;
    void* m_lightUboAllocation = nullptr;
    void* m_lightUboMapped = nullptr;
    VkDescriptorSetLayout m_lightDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_lightDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet m_lightDescriptorSet = VK_NULL_HANDLE;

    glm::vec3 m_lightDir{-0.35f, -1.f, -0.25f};
    bool m_ready = false;
};

} // namespace engine
