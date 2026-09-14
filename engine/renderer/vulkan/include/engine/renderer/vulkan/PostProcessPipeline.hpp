#pragma once

#include <vulkan/vulkan.h>
#include <array>
#include <cstdint>
#include <vector>

namespace engine {

class VulkanContext;

struct PostProcessSettings {
    bool bloomEnabled = true;
    float bloomIntensity = 0.8f;
    float bloomThreshold = 1.0f;
    float bloomKnee = 0.5f;
    float filterRadius = 1.0f;
    float exposure = 1.0f;
    int toneMapper = 0; // 0 = ACES, 1 = Khronos PBR Neutral, 2 = Reinhard, 3 = Linear
};

class PostProcessPipeline {
public:
    static constexpr uint32_t kBloomLevels = 5;

    bool init(VulkanContext& ctx);
    void shutdown();

    void handleResize(uint32_t width, uint32_t height);

    void recordBloom(VkCommandBuffer cmd);
    void recordComposite(VkCommandBuffer cmd);

    [[nodiscard]] PostProcessSettings& settings() { return m_settings; }
    [[nodiscard]] const PostProcessSettings& settings() const { return m_settings; }

    [[nodiscard]] bool ready() const { return m_ready; }

private:
    struct BloomMip {
        VkExtent2D extent{0, 0};

        VkImage downsampleImage = VK_NULL_HANDLE;
        void* downsampleAlloc = nullptr;
        VkImageView downsampleView = VK_NULL_HANDLE;
        VkFramebuffer downsampleFramebuffer = VK_NULL_HANDLE;
        VkDescriptorSet downsampleDescriptorSet = VK_NULL_HANDLE;

        VkImage upsampleImage = VK_NULL_HANDLE;
        void* upsampleAlloc = nullptr;
        VkImageView upsampleView = VK_NULL_HANDLE;
        VkFramebuffer upsampleFramebuffer = VK_NULL_HANDLE;
        VkDescriptorSet upsampleDescriptorSet = VK_NULL_HANDLE;
    };

    struct DownsamplePushConstants {
        float srcTexelSize[4]; // xy: 1.0 / width, 1.0 / height, zw: unused
        float params[4];       // x: threshold, y: threshold - knee, z: 2 * knee, w: 0.25 / (knee + 1e-5)
        int32_t isPrefilter;
        int32_t pad[3];
    };

    struct UpsamplePushConstants {
        float texelSize[4]; // xy: 1.0 / lowerMip width, height; z: filterRadius, w: unused
    };

    struct CompositePushConstants {
        float exposure = 1.0f;
        float bloomIntensity = 0.8f;
        int32_t toneMapper = 0;
        int32_t bloomEnabled = 1;
    };

    bool createShaders();
    bool createSampler();
    bool createDescriptorLayouts();
    bool createBloomRenderPass();
    bool createPipelines();

    bool createBloomImages(uint32_t baseWidth, uint32_t baseHeight);
    void destroyBloomImages();
    bool updateDescriptorSets();

    VulkanContext* m_ctx = nullptr;
    bool m_ready = false;
    PostProcessSettings m_settings;

    VkShaderModule m_vertShader = VK_NULL_HANDLE;
    VkShaderModule m_downsampleFragShader = VK_NULL_HANDLE;
    VkShaderModule m_upsampleFragShader = VK_NULL_HANDLE;
    VkShaderModule m_compositeFragShader = VK_NULL_HANDLE;

    VkSampler m_sampler = VK_NULL_HANDLE;
    VkRenderPass m_bloomRenderPass = VK_NULL_HANDLE;

    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_singleTextureLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_twoTextureLayout = VK_NULL_HANDLE;

    VkDescriptorSet m_compositeDescriptorSet = VK_NULL_HANDLE;

    VkPipelineLayout m_downsamplePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_downsamplePipeline = VK_NULL_HANDLE;

    VkPipelineLayout m_upsamplePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_upsamplePipeline = VK_NULL_HANDLE;

    VkPipelineLayout m_compositePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_compositePipeline = VK_NULL_HANDLE;

    std::array<BloomMip, kBloomLevels> m_bloomMips{};
};

} // namespace engine
